/**
 * @file  CommandTerminal.ino
 * @brief 지령단말기 (Command Terminal) — Main Arduino sketch
 *
 * A command-receiving and acknowledgement terminal for Arduino Uno / Nano.
 *
 * Features
 * --------
 *  • Receives commands over UART (Serial Monitor or host computer)
 *  • Operator can also type commands via a 4×4 matrix keypad
 *  • Commands are displayed on a 16×2 LCD (long text auto-scrolls)
 *  • Status LEDs indicate idle / active / acknowledged state
 *  • Passive buzzer beeps on new commands and acknowledgement
 *  • Acknowledged button (or 'B' on keypad) pops the front command
 *  • Commands are logged to EEPROM (circular, 8 entries)
 *  • UART query interface: ?LOG, ?CLEAR, ?STATUS
 *
 * Hardware
 * --------
 *  See Config.h for full wiring details.
 *
 * Required Libraries
 * ------------------
 *  • LiquidCrystal  (built-in with Arduino IDE)
 *  • EEPROM         (built-in with Arduino IDE)
 *
 * License : MIT
 */

#include <Arduino.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

#include "Config.h"
#include "Display.h"
#include "CommandManager.h"
#include "Keypad.h"
#include "SerialComm.h"

// ─── Global objects ───────────────────────────────────────────────────────────
CommandManager cmdMgr;
Keypad         keypad;
SerialComm     serial;

// ─── State tracking ───────────────────────────────────────────────────────────
static TerminalState prevState        = STATE_IDLE;
static uint8_t       scrollPos        = 0;
static unsigned long lastScrollTime   = 0;
static unsigned long lastBlinkTime    = 0;
static bool          ledBlinkOn       = false;
static unsigned long ackStateEnterMs  = 0;  ///< When STATE_ACK was entered

// Acknowledge button debounce
static bool          btnWasPressed    = false;
static unsigned long btnLastTime      = 0;

// ─── Non-blocking buzzer scheduler ───────────────────────────────────────────
// Plays up to BUZZ_MAX_BEEPS beeps without blocking loop().
#define BUZZ_MAX_BEEPS 4
#define BUZZ_GAP_MS    80   // silent gap between consecutive beeps

struct BuzzSchedule {
    uint16_t     freq;
    uint16_t     dur;
    uint8_t      remaining;
    bool         inGap;
    unsigned long startMs;
};
static BuzzSchedule buzz = {0, 0, 0, false, 0};

/**
 * @brief Schedule one or more non-blocking beeps.
 * @param freq    Tone frequency in Hz.
 * @param durMs   Duration of each beep in milliseconds.
 * @param count   Number of beeps (default 1, max BUZZ_MAX_BEEPS).
 */
static void scheduleBuzz(uint16_t freq, uint16_t durMs, uint8_t count = 1) {
    if (count > BUZZ_MAX_BEEPS) count = BUZZ_MAX_BEEPS;
    buzz.freq      = freq;
    buzz.dur       = durMs;
    buzz.remaining = count;
    buzz.inGap     = false;
    buzz.startMs   = millis();
    tone(BUZZER_PIN, freq, durMs);
}

/** @brief Call every loop() iteration to advance the buzzer state machine. */
static void updateBuzzer() {
    if (buzz.remaining == 0) return;

    unsigned long elapsed = millis() - buzz.startMs;

    if (!buzz.inGap) {
        // Currently playing a beep
        if (elapsed >= (unsigned long)buzz.dur) {
            noTone(BUZZER_PIN);
            buzz.remaining--;
            if (buzz.remaining > 0) {
                buzz.inGap   = true;
                buzz.startMs = millis();
            }
        }
    } else {
        // In the silent gap between beeps
        if (elapsed >= BUZZ_GAP_MS) {
            buzz.inGap   = false;
            buzz.startMs = millis();
            tone(BUZZER_PIN, buzz.freq, buzz.dur);
        }
    }
}

// ─── Forward declarations ─────────────────────────────────────────────────────
static void updateLEDs();
static void handleAck();
static void handleKey(char key);
static void onStateChanged(TerminalState newState);

// ─────────────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    // ── LED pins ──────────────────────────────────────────────────────────
    pinMode(LED_IDLE,   OUTPUT);
    pinMode(LED_ACTIVE, OUTPUT);
    pinMode(LED_ACK,    OUTPUT);

    // ── Buzzer ────────────────────────────────────────────────────────────
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    // ── Acknowledge button ────────────────────────────────────────────────
    pinMode(ACK_BTN_PIN, INPUT_PULLUP);

    // ── Keypad ────────────────────────────────────────────────────────────
    keypad.begin();

    // ── Display ───────────────────────────────────────────────────────────
    displayInit();
    displaySplash();
    delay(1500);
    displayClear();
    displayStatus("CMD TERMINAL");
    displayCommand("Ready...");

    // ── Serial ────────────────────────────────────────────────────────────
    serial.begin();

    // ── Initial LED state ─────────────────────────────────────────────────
    digitalWrite(LED_IDLE,   HIGH);
    digitalWrite(LED_ACTIVE, LOW);
    digitalWrite(LED_ACK,    LOW);

    // ── Startup beep (blocking is acceptable in setup) ────────────────────
    tone(BUZZER_PIN, BEEP_FREQ, BEEP_SHORT);
    delay(BEEP_SHORT + 10);
    noTone(BUZZER_PIN);
}

// ─────────────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // ── 1. Receive serial data ────────────────────────────────────────────
    serial.update(cmdMgr);

    // ── 2. Scan keypad ────────────────────────────────────────────────────
    char key = keypad.getKey();
    if (key != KEY_NONE) {
        handleKey(key);
    }

    // ── 3. Acknowledge button ─────────────────────────────────────────────
    handleAck();

    // ── 4. State change detection ─────────────────────────────────────────
    TerminalState curState = cmdMgr.state();
    if (curState != prevState) {
        onStateChanged(curState);
        prevState = curState;
    }

    // ── 5. STATE_ACK auto-return to IDLE after ACK_STATE_TIMEOUT_MS ──────────
    if (curState == STATE_ACK) {
        if (millis() - ackStateEnterMs >= ACK_STATE_TIMEOUT_MS) {
            cmdMgr.clearAck();
            displayStatus("CMD TERMINAL");
            displayCommand("Ready...");
        }
    }

    // ── 6. Non-blocking buzzer ────────────────────────────────────────────
    updateBuzzer();

    // ── 7. LED blinking in ACTIVE state ───────────────────────────────────
    updateLEDs();

    // ── 8. Scroll long commands ───────────────────────────────────────────
    if (curState == STATE_ACTIVE) {
        const Command* front = cmdMgr.peek();
        if (front && strlen(front->text) > LCD_COLS) {
            unsigned long now = millis();
            if (now - lastScrollTime >= SCROLL_DELAY_MS) {
                scrollPos     = displayScrollStep(front->text, scrollPos);
                lastScrollTime = now;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// onStateChanged — called once when the terminal enters a new state
// ─────────────────────────────────────────────────────────────────────────────
static void onStateChanged(TerminalState newState) {
    scrollPos = 0;

    switch (newState) {
        case STATE_IDLE:
            displayStatus("CMD TERMINAL");
            displayCommand("Ready...");
            break;

        case STATE_ACTIVE: {
            const Command* front = cmdMgr.peek();
            if (front) {
                if (front->priority == 1) {
                    displayStatus("!! URGENT !!");
                    scheduleBuzz(BEEP_FREQ * 2, BEEP_LONG, 2);  // double long beep
                } else {
                    displayStatus("New Command:");
                    scheduleBuzz(BEEP_FREQ, BEEP_SHORT, 1);
                }
                displayNewCommand(front->text);
            }
            break;
        }

        case STATE_INPUT:
            displayStatus("Input:");
            displayCommand(cmdMgr.inputText());
            break;

        case STATE_ACK:
            ackStateEnterMs = millis();
            displayAckFlash("CMD TERMINAL");
            scheduleBuzz(BEEP_FREQ / 2, BEEP_SHORT, 1);
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// updateLEDs
// ─────────────────────────────────────────────────────────────────────────────
static void updateLEDs() {
    TerminalState s = cmdMgr.state();

    switch (s) {
        case STATE_IDLE:
            digitalWrite(LED_IDLE,   HIGH);
            digitalWrite(LED_ACTIVE, LOW);
            digitalWrite(LED_ACK,    LOW);
            break;

        case STATE_INPUT:
            digitalWrite(LED_IDLE,   HIGH);
            digitalWrite(LED_ACTIVE, HIGH);
            digitalWrite(LED_ACK,    LOW);
            break;

        case STATE_ACTIVE: {
            // Blink ACTIVE LED
            unsigned long now = millis();
            if (now - lastBlinkTime >= BLINK_INTERVAL_MS) {
                ledBlinkOn    = !ledBlinkOn;
                lastBlinkTime  = now;
            }
            digitalWrite(LED_IDLE,   LOW);
            digitalWrite(LED_ACTIVE, ledBlinkOn ? HIGH : LOW);
            digitalWrite(LED_ACK,    LOW);
            break;
        }

        case STATE_ACK:
            digitalWrite(LED_IDLE,   LOW);
            digitalWrite(LED_ACTIVE, LOW);
            digitalWrite(LED_ACK,    HIGH);
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// handleAck — polls the physical acknowledge button
// ─────────────────────────────────────────────────────────────────────────────
static void handleAck() {
    bool pressed = (digitalRead(ACK_BTN_PIN) == LOW);
    unsigned long now = millis();

    if (pressed && !btnWasPressed && (now - btnLastTime) >= DEBOUNCE_MS) {
        btnWasPressed = true;
        btnLastTime   = now;

        if (cmdMgr.state() == STATE_ACTIVE) {
            const Command* front = cmdMgr.peek();
            if (front) {
                serial.sendAck(front->text);
            }
            cmdMgr.acknowledge();
        }
    } else if (!pressed) {
        btnWasPressed = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// handleKey — processes a keypad key press
// ─────────────────────────────────────────────────────────────────────────────
static void handleKey(char key) {
    TerminalState s = cmdMgr.state();

    switch (key) {
        // ── Special keys ───────────────────────────────────────────────────
        case 'A':
            // Toggle input mode
            if (s == STATE_INPUT) {
                cmdMgr.inputClear();
                displayStatus("CMD TERMINAL");
                displayCommand("Ready...");
            } else {
                cmdMgr.setState(STATE_INPUT);
                displayStatus("Input:");
                displayCommand("");
            }
            break;

        case 'B':
            // Acknowledge front command (same as physical button)
            if (s == STATE_ACTIVE) {
                const Command* front = cmdMgr.peek();
                if (front) {
                    serial.sendAck(front->text);
                }
                cmdMgr.acknowledge();
            }
            break;

        case 'C':
            // Clear input buffer
            cmdMgr.inputClear();
            displayCommand("");
            break;

        case '#':
            // Submit / Enter
            if (s == STATE_INPUT) {
                if (cmdMgr.inputSubmit()) {
                    serial.sendInfo("Keypad command submitted");
                } else {
                    serial.sendInfo("Input buffer empty or queue full");
                }
            }
            break;

        case '*':
            // Backspace
            if (s == STATE_INPUT) {
                cmdMgr.inputBackspace();
                displayCommand(cmdMgr.inputText());
            }
            break;

        default:
            // Digit or letter — append to input buffer
            if (s == STATE_INPUT || s == STATE_IDLE) {
                if (s == STATE_IDLE) {
                    cmdMgr.setState(STATE_INPUT);
                    displayStatus("Input:");
                }
                cmdMgr.inputAppend(key);
                displayCommand(cmdMgr.inputText());
            }
            break;
    }
}
