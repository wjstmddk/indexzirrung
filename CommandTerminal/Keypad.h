/**
 * @file Keypad.h
 * @brief 지령단말기 (Command Terminal) — 4×4 matrix keypad driver
 *
 * Software-scanned keypad using the row/column pin arrays defined in Config.h.
 * No external library required.
 *
 * Key map:
 *   1  2  3  A
 *   4  5  6  B
 *   7  8  9  C
 *   *  0  #  D
 *
 * Special keys used by the terminal:
 *   '*' — Backspace (delete last input character)
 *   '#' — Enter / submit input
 *   'A' — Toggle input mode
 *   'B' — Acknowledge front command
 *   'C' — Clear input buffer
 *   'D' — (reserved / future use)
 */

#ifndef KEYPAD_H
#define KEYPAD_H

#include <Arduino.h>
#include "Config.h"

// Sentinel for "no key pressed"
#define KEY_NONE '\0'

class Keypad {
public:
    Keypad() : _lastKey(KEY_NONE), _lastPressTime(0), _pressed(false) {}

    /**
     * @brief Initialise row and column pins.
     *        Call once from setup().
     */
    void begin() {
        for (uint8_t r = 0; r < KP_ROWS; r++) {
            pinMode(KP_ROW_PINS[r], OUTPUT);
            digitalWrite(KP_ROW_PINS[r], HIGH);
        }
        for (uint8_t c = 0; c < KP_COLS; c++) {
            pinMode(KP_COL_PINS[c], INPUT_PULLUP);
        }
    }

    /**
     * @brief Scan the keypad.
     * @return The character of the key currently pressed, or KEY_NONE.
     *
     * Includes software debounce (DEBOUNCE_MS from Config.h) and returns
     * each key-press event only once per physical press.
     */
    char getKey() {
        char raw = scan();

        if (raw == KEY_NONE) {
            _pressed = false;
            _lastKey = KEY_NONE;
            return KEY_NONE;
        }

        // New key or same key after release
        if (!_pressed || raw != _lastKey) {
            unsigned long now = millis();
            if ((now - _lastPressTime) >= DEBOUNCE_MS) {
                _lastKey       = raw;
                _lastPressTime = now;
                _pressed       = true;
                return raw;
            }
        }
        return KEY_NONE;
    }

private:
    char          _lastKey;
    unsigned long _lastPressTime;
    bool          _pressed;

    /**
     * @brief Raw hardware scan — no debounce.
     * @return Pressed key character, or KEY_NONE if nothing is pressed.
     */
    char scan() {
        for (uint8_t r = 0; r < KP_ROWS; r++) {
            // Drive current row LOW
            digitalWrite(KP_ROW_PINS[r], LOW);

            for (uint8_t c = 0; c < KP_COLS; c++) {
                if (digitalRead(KP_COL_PINS[c]) == LOW) {
                    // Key at (r, c) is pressed — release row and return
                    digitalWrite(KP_ROW_PINS[r], HIGH);
                    return KP_KEYS[r][c];
                }
            }

            // Release row before scanning next
            digitalWrite(KP_ROW_PINS[r], HIGH);
        }
        return KEY_NONE;
    }
};

#endif // KEYPAD_H
