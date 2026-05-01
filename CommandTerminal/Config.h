/**
 * @file Config.h
 * @brief 지령단말기 (Command Terminal) — Pin definitions and project configuration
 *
 * Target board : Arduino Uno / Nano
 *
 * === Wiring Summary ===
 *
 * LCD 16×2 (4-bit parallel mode)
 *   RS  -> PIN 8
 *   EN  -> PIN 9
 *   D4  -> PIN 4
 *   D5  -> PIN 5
 *   D6  -> PIN 6
 *   D7  -> PIN 7
 *   RW  -> GND
 *   VSS -> GND, VDD -> 5V
 *   V0  -> 10 kΩ pot (contrast)
 *   A   -> 5V via 220 Ω (backlight), K -> GND
 *
 * 4×4 Matrix Keypad
 *   ROW1-4 -> PIN 14(A0), 15(A1), 16(A2), 17(A3)
 *   COL1-4 -> PIN 10, 11, 12, 13
 *
 * Status LEDs (via 220 Ω resistors)
 *   LED_IDLE     -> PIN A4  (green  — system idle)
 *   LED_ACTIVE   -> PIN A5  (yellow — command received / in progress)
 *   LED_ACK      -> PIN 3   (red    — acknowledged / error)
 *
 * Buzzer (passive or active)
 *   BUZZER_PIN   -> PIN 2
 *
 * Acknowledge Button (pull-up, active LOW)
 *   ACK_BTN_PIN  -> PIN 18 (A4)  ← shares physical pin with LED_IDLE; see warning below
 *
 * NOTE: On Arduino Uno the analog pins A0-A5 can be used as digital I/O
 *       (digital pin numbers 14-19).  Adjust if using a different board.
 */

#ifndef CONFIG_H
#define CONFIG_H

// ─── LCD pins ────────────────────────────────────────────────────────────────
#define LCD_RS  8
#define LCD_EN  9
#define LCD_D4  4
#define LCD_D5  5
#define LCD_D6  6
#define LCD_D7  7

#define LCD_COLS 16
#define LCD_ROWS  2

// ─── 4×4 Keypad ──────────────────────────────────────────────────────────────
#define KP_ROWS 4
#define KP_COLS 4

// Row pins (output, driven LOW one at a time)
const uint8_t KP_ROW_PINS[KP_ROWS] = {14, 15, 16, 17};   // A0-A3

// Column pins (input with internal pull-up)
const uint8_t KP_COL_PINS[KP_COLS] = {10, 11, 12, 13};

// Key map — row-major order
const char KP_KEYS[KP_ROWS][KP_COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

// ─── LED pins ─────────────────────────────────────────────────────────────────
#define LED_IDLE   A4   // green
#define LED_ACTIVE A5   // yellow
#define LED_ACK     3   // red

// ─── Buzzer ──────────────────────────────────────────────────────────────────
#define BUZZER_PIN    2
#define BEEP_FREQ   880   // Hz
#define BEEP_SHORT  100   // ms
#define BEEP_LONG   400   // ms

// ─── Acknowledge button ───────────────────────────────────────────────────────
// Uses internal pull-up; press = LOW.
// WARNING: ACK_BTN_PIN (18 / A4) and LED_IDLE (A4) reference the same physical
// pin on Arduino Uno.  You must choose ONE of:
//   (a) Use LED_IDLE only (no physical button) — the default wiring.
//   (b) Wire the button to a separate free pin, update ACK_BTN_PIN accordingly,
//       and remove or reassign LED_IDLE.
#define ACK_BTN_PIN 18    // A4 as digital 18

// ─── Serial ──────────────────────────────────────────────────────────────────
#define SERIAL_BAUD 9600

// ─── Command buffer ──────────────────────────────────────────────────────────
#define CMD_MAX_LEN    32   // maximum characters per command (incl. null)
#define CMD_QUEUE_SIZE  8   // number of commands that can be queued

// ─── Timings ─────────────────────────────────────────────────────────────────
#define DEBOUNCE_MS         50    // button debounce
#define BLINK_INTERVAL_MS  500    // LED blink period for active state
#define SCROLL_DELAY_MS    300    // ms between display scroll steps
#define ACK_STATE_TIMEOUT_MS 2000 // ms to hold STATE_ACK before returning to IDLE

#endif // CONFIG_H
