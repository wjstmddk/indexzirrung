/**
 * @file SerialComm.h
 * @brief 지령단말기 (Command Terminal) — Serial communication handler
 *
 * Receives newline-terminated command strings from the host PC (or another
 * microcontroller) over UART and feeds them into the CommandManager queue.
 *
 * Protocol (host → terminal):
 *   <command text>\n          Normal-priority command
 *   !<command text>\n         Urgent-priority command (prefix '!')
 *   ?LOG\n                    Request EEPROM log dump
 *   ?CLEAR\n                  Erase EEPROM log
 *   ?STATUS\n                 Print current state info
 *
 * Protocol (terminal → host):
 *   [INFO] <text>             Informational message
 *   [ACK]  <command text>     Command acknowledged
 *   [LOG]  <n>: <text> (p:<priority>)   Log dump entry
 *   [ERR]  <text>             Error message
 */

#ifndef SERIAL_COMM_H
#define SERIAL_COMM_H

#include <Arduino.h>
#include "Config.h"
#include "CommandManager.h"

class SerialComm {
public:
    SerialComm() : _bufIdx(0) {}

    /**
     * @brief Initialise UART.  Call once from setup().
     */
    void begin() {
        Serial.begin(SERIAL_BAUD);
        Serial.println(F("[INFO] 지령단말기 Command Terminal ready"));
    }

    /**
     * @brief Non-blocking receive loop.
     *
     * Call from loop().  When a complete line is received, it is parsed and
     * the appropriate action is taken on the supplied CommandManager.
     *
     * @param mgr  Reference to the active CommandManager.
     */
    void update(CommandManager& mgr) {
        while (Serial.available()) {
            char c = (char)Serial.read();

            if (c == '\r') continue;  // ignore CR

            if (c == '\n') {
                _buf[_bufIdx] = '\0';
                processLine(_buf, mgr);
                _bufIdx = 0;
                return;  // process one line per call for responsiveness
            }

            if (_bufIdx < CMD_MAX_LEN - 1) {
                _buf[_bufIdx++] = c;
            }
            // Silently drop characters that overflow the buffer
        }
    }

    /**
     * @brief Send an acknowledgement message to the host.
     * @param cmdText  The command text that was acknowledged.
     */
    void sendAck(const char* cmdText) {
        Serial.print(F("[ACK]  "));
        Serial.println(cmdText);
    }

    /**
     * @brief Send an informational message to the host.
     * @param msg  Message string.
     */
    void sendInfo(const char* msg) {
        Serial.print(F("[INFO] "));
        Serial.println(msg);
    }

    /**
     * @brief Dump the EEPROM log over serial.
     * @param mgr  Reference to the CommandManager.
     */
    void dumpLog(CommandManager& mgr) {
        Serial.println(F("[INFO] --- EEPROM Log ---"));
        char buf[CMD_MAX_LEN];
        for (uint8_t i = 0; i < EEPROM_LOG_COUNT; i++) {
            uint8_t prio = mgr.readLog(i, buf);
            if (prio == 0xFF && buf[0] == (char)0xFF) {
                Serial.print(F("[LOG]  "));
                Serial.print(i);
                Serial.println(F(": (empty)"));
            } else {
                Serial.print(F("[LOG]  "));
                Serial.print(i);
                Serial.print(F(": "));
                Serial.print(buf);
                Serial.print(F(" (p:"));
                Serial.print(prio);
                Serial.println(')');
            }
        }
        Serial.println(F("[INFO] --- End of Log ---"));
    }

private:
    char    _buf[CMD_MAX_LEN];
    uint8_t _bufIdx;

    void processLine(const char* line, CommandManager& mgr) {
        if (strlen(line) == 0) return;

        // ── Query commands ──────────────────────────────────────────────────
        if (strcmp(line, "?LOG") == 0) {
            dumpLog(mgr);
            return;
        }
        if (strcmp(line, "?CLEAR") == 0) {
            mgr.eraseLog();
            Serial.println(F("[INFO] EEPROM log erased"));
            return;
        }
        if (strcmp(line, "?STATUS") == 0) {
            Serial.print(F("[INFO] State="));
            Serial.print((uint8_t)mgr.state());
            Serial.print(F(" Queued="));
            Serial.println(mgr.count());
            return;
        }

        // ── Incoming command ────────────────────────────────────────────────
        uint8_t priority = 0;
        const char* text = line;

        if (line[0] == '!') {
            priority = 1;
            text     = line + 1;   // skip '!' prefix
        }

        if (strlen(text) == 0) {
            Serial.println(F("[ERR]  Empty command text"));
            return;
        }

        if (mgr.isFull()) {
            Serial.println(F("[ERR]  Command queue full"));
            return;
        }

        if (mgr.enqueue(text, priority)) {
            Serial.print(F("[INFO] Queued: "));
            Serial.print(text);
            Serial.print(F(" (p:"));
            Serial.print(priority);
            Serial.println(')');
        }
    }
};

#endif // SERIAL_COMM_H
