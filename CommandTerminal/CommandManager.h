/**
 * @file CommandManager.h
 * @brief 지령단말기 (Command Terminal) — Command queue and state management
 *
 * Provides a fixed-size FIFO queue for incoming commands and tracks the
 * current terminal state.
 *
 * States:
 *   STATE_IDLE    — No pending commands; terminal is waiting.
 *   STATE_ACTIVE  — One or more commands are queued, awaiting acknowledgement.
 *   STATE_INPUT   — Operator is typing a command via the keypad.
 *   STATE_ACK     — Last command was acknowledged; brief confirmation display.
 */

#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include "Config.h"

// ─── Terminal states ──────────────────────────────────────────────────────────
enum TerminalState : uint8_t {
    STATE_IDLE   = 0,
    STATE_ACTIVE = 1,
    STATE_INPUT  = 2,
    STATE_ACK    = 3
};

// ─── Command entry ────────────────────────────────────────────────────────────
struct Command {
    char    text[CMD_MAX_LEN]; ///< Null-terminated command string
    uint8_t priority;          ///< 0 = normal, 1 = urgent
    bool    acknowledged;      ///< Has the operator acknowledged this command?
};

// ─── EEPROM log layout ────────────────────────────────────────────────────────
// We store up to 8 command records starting at address 0.
// Each record: CMD_MAX_LEN bytes for text + 1 byte for priority = 33 bytes.
#define EEPROM_LOG_COUNT  8
#define EEPROM_RECORD_LEN (CMD_MAX_LEN + 1)   // 33 bytes per record

// ─── CommandManager class ─────────────────────────────────────────────────────
class CommandManager {
public:
    CommandManager()
        : _head(0), _tail(0), _count(0),
          _state(STATE_IDLE), _eepromIndex(0) {}

    // ── Queue operations ────────────────────────────────────────────────────

    /**
     * @brief Enqueue a new command.
     * @param text      Null-terminated command string (will be truncated to CMD_MAX_LEN-1).
     * @param priority  0 = normal, 1 = urgent.
     * @return true if enqueued successfully; false if queue is full.
     */
    bool enqueue(const char* text, uint8_t priority = 0) {
        if (_count >= CMD_QUEUE_SIZE) return false;

        Command& cmd = _queue[_tail];
        strncpy(cmd.text, text, CMD_MAX_LEN - 1);
        cmd.text[CMD_MAX_LEN - 1] = '\0';
        cmd.priority = priority;
        cmd.acknowledged = false;

        _tail = (_tail + 1) % CMD_QUEUE_SIZE;
        _count++;

        if (_state == STATE_IDLE) {
            _state = STATE_ACTIVE;
        }

        logToEEPROM(cmd);
        return true;
    }

    /**
     * @brief Peek at the front command without removing it.
     * @return Pointer to the front Command, or nullptr if queue is empty.
     */
    const Command* peek() const {
        if (_count == 0) return nullptr;
        return &_queue[_head];
    }

    /**
     * @brief Acknowledge and remove the front command.
     *
     * When the queue becomes empty after acknowledgement, the terminal
     * transitions to STATE_ACK.  The caller MUST later invoke clearAck()
     * (or let the main loop's timeout do so) to return to STATE_IDLE.
     *
     * @return true if a command was acknowledged; false if queue was empty.
     */
    bool acknowledge() {
        if (_count == 0) return false;

        _queue[_head].acknowledged = true;
        _head = (_head + 1) % CMD_QUEUE_SIZE;
        _count--;

        _state = (_count > 0) ? STATE_ACTIVE : STATE_ACK;
        return true;
    }

    /**
     * @brief Transition from STATE_ACK back to STATE_IDLE.
     */
    void clearAck() {
        if (_state == STATE_ACK) {
            _state = STATE_IDLE;
        }
    }

    // ── Input buffer (keypad) ───────────────────────────────────────────────

    /**
     * @brief Append a character to the input buffer.
     * @return true if appended; false if buffer is full.
     */
    bool inputAppend(char c) {
        uint8_t len = (uint8_t)strlen(_inputBuf);
        if (len >= CMD_MAX_LEN - 1) return false;
        _inputBuf[len]     = c;
        _inputBuf[len + 1] = '\0';
        _state = STATE_INPUT;
        return true;
    }

    /**
     * @brief Remove the last character from the input buffer (backspace).
     */
    void inputBackspace() {
        uint8_t len = (uint8_t)strlen(_inputBuf);
        if (len > 0) {
            _inputBuf[len - 1] = '\0';
        }
        if (len <= 1) {
            // Buffer is now empty — return to previous state
            _state = (_count > 0) ? STATE_ACTIVE : STATE_IDLE;
        }
    }

    /**
     * @brief Submit the current input buffer as a new command.
     * @return true if successfully enqueued; false if buffer empty or queue full.
     */
    bool inputSubmit() {
        if (strlen(_inputBuf) == 0) {
            _state = (_count > 0) ? STATE_ACTIVE : STATE_IDLE;
            return false;
        }
        bool ok = enqueue(_inputBuf);
        inputClear();
        return ok;
    }

    /**
     * @brief Clear the input buffer without submitting.
     */
    void inputClear() {
        memset(_inputBuf, 0, sizeof(_inputBuf));
        _state = (_count > 0) ? STATE_ACTIVE : STATE_IDLE;
    }

    /** @return Current input buffer (read-only). */
    const char* inputText() const { return _inputBuf; }

    // ── Accessors ───────────────────────────────────────────────────────────

    TerminalState state()  const { return _state; }
    uint8_t       count()  const { return _count; }
    bool          isEmpty() const { return _count == 0; }
    bool          isFull()  const { return _count >= CMD_QUEUE_SIZE; }

    /** @brief Force a specific state (use sparingly). */
    void setState(TerminalState s) { _state = s; }

    // ── EEPROM log ──────────────────────────────────────────────────────────

    /**
     * @brief Erase the EEPROM command log (write 0xFF to all log bytes).
     */
    void eraseLog() {
        for (int i = 0; i < EEPROM_LOG_COUNT * EEPROM_RECORD_LEN; i++) {
            EEPROM.update(i, 0xFF);
        }
        _eepromIndex = 0;
    }

    /**
     * @brief Read a logged command from EEPROM by index (0-based).
     * @param index  Record index (0 … EEPROM_LOG_COUNT-1).
     * @param[out] out  Buffer of at least CMD_MAX_LEN bytes.
     * @return  Priority byte (0 or 1), or 0xFF if record is empty/invalid.
     */
    uint8_t readLog(uint8_t index, char* out) const {
        if (index >= EEPROM_LOG_COUNT) { out[0] = '\0'; return 0xFF; }
        int base = index * EEPROM_RECORD_LEN;
        for (uint8_t i = 0; i < CMD_MAX_LEN; i++) {
            out[i] = (char)EEPROM.read(base + i);
        }
        out[CMD_MAX_LEN - 1] = '\0';
        return (uint8_t)EEPROM.read(base + CMD_MAX_LEN);
    }

private:
    Command       _queue[CMD_QUEUE_SIZE];
    uint8_t       _head;
    uint8_t       _tail;
    uint8_t       _count;
    TerminalState _state;
    char          _inputBuf[CMD_MAX_LEN];
    uint8_t       _eepromIndex;  ///< Circular write pointer for EEPROM log

    void logToEEPROM(const Command& cmd) {
        int base = _eepromIndex * EEPROM_RECORD_LEN;
        for (uint8_t i = 0; i < CMD_MAX_LEN; i++) {
            EEPROM.update(base + i, (uint8_t)cmd.text[i]);
        }
        EEPROM.update(base + CMD_MAX_LEN, cmd.priority);
        _eepromIndex = (_eepromIndex + 1) % EEPROM_LOG_COUNT;
    }
};

#endif // COMMAND_MANAGER_H
