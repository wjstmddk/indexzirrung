/**
 * @file Display.h
 * @brief 지령단말기 (Command Terminal) — LCD 16×2 display helpers
 *
 * Wraps LiquidCrystal to provide simple, fixed-layout display routines.
 *
 * Screen layout (16 columns × 2 rows):
 *
 *   Row 0: [Status bar]  e.g. "지령단말기  [OK]"
 *   Row 1: [Command text, scrolls if > 16 chars]
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <LiquidCrystal.h>
#include "Config.h"

// ─── LCD instance (defined in Display.cpp / accessed via extern) ──────────────
static LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// ─── Custom characters ────────────────────────────────────────────────────────
// Bell icon (5×8) — shown when a new command arrives
static const uint8_t CHAR_BELL[8] = {
    0b00100,
    0b01110,
    0b01110,
    0b01110,
    0b11111,
    0b00000,
    0b00100,
    0b00000
};

// Checkmark icon — shown after acknowledgement
static const uint8_t CHAR_CHECK[8] = {
    0b00000,
    0b00001,
    0b00011,
    0b10110,
    0b11100,
    0b01000,
    0b00000,
    0b00000
};

/**
 * @brief Initialise the LCD and register custom characters.
 */
inline void displayInit() {
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcd.createChar(0, const_cast<uint8_t*>(CHAR_BELL));
    lcd.createChar(1, const_cast<uint8_t*>(CHAR_CHECK));
    lcd.clear();
}

/**
 * @brief Show the splash / start-up screen.
 */
inline void displaySplash() {
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print(F("CMD TERMINAL"));
    lcd.setCursor(3, 1);
    lcd.print(F("Jillyeong v1"));
}

/**
 * @brief Print a status string on row 0 (truncated to LCD_COLS).
 * @param status  C-string to display.
 */
inline void displayStatus(const char* status) {
    lcd.setCursor(0, 0);
    // Pad with spaces to clear previous content
    char buf[LCD_COLS + 1];
    memset(buf, ' ', LCD_COLS);
    buf[LCD_COLS] = '\0';
    uint8_t len = (uint8_t)strlen(status);
    if (len > LCD_COLS) len = LCD_COLS;
    memcpy(buf, status, len);
    lcd.print(buf);
}

/**
 * @brief Print a command string on row 1 (truncated to LCD_COLS).
 * @param cmd  C-string to display.
 */
inline void displayCommand(const char* cmd) {
    lcd.setCursor(0, 1);
    char buf[LCD_COLS + 1];
    memset(buf, ' ', LCD_COLS);
    buf[LCD_COLS] = '\0';
    uint8_t len = (uint8_t)strlen(cmd);
    if (len > LCD_COLS) len = LCD_COLS;
    memcpy(buf, cmd, len);
    lcd.print(buf);
}

/**
 * @brief Clear both rows.
 */
inline void displayClear() {
    lcd.clear();
}

/**
 * @brief Show a brief "ACK" confirmation on row 0, then restore status.
 * @param restoreStatus  String to restore on row 0 after the flash.
 */
inline void displayAckFlash(const char* restoreStatus) {
    lcd.setCursor(0, 0);
    lcd.write(byte(1));           // checkmark glyph
    lcd.print(F(" ACKNOWLEDGED  "));
    delay(1000);
    displayStatus(restoreStatus);
}

/**
 * @brief Show bell icon and truncated command on row 1.
 * @param cmd  Incoming command string.
 */
inline void displayNewCommand(const char* cmd) {
    lcd.setCursor(0, 1);
    lcd.write(byte(0));           // bell glyph
    lcd.print(' ');
    // Print up to 14 chars (bell + space take 2)
    char buf[LCD_COLS - 1];
    memset(buf, ' ', sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    uint8_t len = (uint8_t)strlen(cmd);
    if (len > LCD_COLS - 2) len = LCD_COLS - 2;
    memcpy(buf, cmd, len);
    lcd.print(buf);
}

/**
 * @brief Scroll a long command string across row 1.
 *
 * This is a blocking call that scrolls text one character at a time.
 * The caller is responsible for timing (call periodically from loop()).
 *
 * @param cmd         Full command string.
 * @param scrollPos   Current scroll offset (incremented each call).
 * @return            New scroll position (wraps around).
 */
inline uint8_t displayScrollStep(const char* cmd, uint8_t scrollPos) {
    uint8_t len = (uint8_t)strlen(cmd);
    if (len <= LCD_COLS) {
        // No scrolling needed
        displayCommand(cmd);
        return 0;
    }

    lcd.setCursor(0, 1);
    for (uint8_t i = 0; i < LCD_COLS; i++) {
        uint8_t idx = (scrollPos + i) % len;
        lcd.print(cmd[idx]);
    }

    scrollPos++;
    if (scrollPos >= len) scrollPos = 0;
    return scrollPos;
}

#endif // DISPLAY_H
