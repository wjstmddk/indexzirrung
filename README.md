# 지령단말기 (Command Terminal) — Arduino Project

> **지령단말기** (Jilyeong Danmalgi) is a Korean term for a *command/order terminal* — a device used by operators to receive, display, and acknowledge operational commands.

This repository contains an Arduino sketch that implements a compact command terminal using an **Arduino Uno / Nano**, a **16×2 LCD**, a **4×4 matrix keypad**, status LEDs, and a passive buzzer.

---

## Features

| Feature | Details |
|---|---|
| **Command reception** | Receives newline-terminated strings via UART (Serial Monitor / host PC) |
| **Keypad input** | Operator can type commands directly on the 4×4 matrix keypad |
| **LCD display** | 16×2 character LCD shows status on row 0, command text on row 1; auto-scrolls long messages |
| **Priority commands** | Prefix a serial command with `!` to mark it urgent (double beep + `!! URGENT !!` banner) |
| **Command queue** | Up to 8 commands can be queued in a FIFO buffer |
| **Acknowledgement** | Physical button (active-LOW) or keypad `B` key acknowledges the front command |
| **Status LEDs** | Green = idle · Yellow (blink) = active · Red = acknowledged |
| **Buzzer** | Short beep on normal command, double long beep on urgent, low beep on acknowledge |
| **EEPROM logging** | Last 8 commands are persisted in a circular EEPROM log (survives power-off) |
| **UART query interface** | `?LOG`, `?CLEAR`, `?STATUS` commands over serial |

---

## File Structure

```
CommandTerminal/
├── CommandTerminal.ino   Main sketch (setup + loop + state machine)
├── Config.h              Pin definitions and project constants
├── Display.h             LCD helper functions (LiquidCrystal wrapper)
├── CommandManager.h      FIFO command queue + input buffer + EEPROM log
├── Keypad.h              4×4 matrix keypad driver (no external library)
└── SerialComm.h          UART command receiver and query handler
```

---

## Hardware Requirements

| Component | Quantity | Notes |
|---|---|---|
| Arduino Uno or Nano | 1 | Any 5 V AVR Arduino |
| LCD 16×2 (HD44780) | 1 | 4-bit parallel mode |
| 10 kΩ potentiometer | 1 | LCD contrast (V0) |
| 4×4 matrix keypad | 1 | Membrane or tactile |
| Passive buzzer | 1 | Driven by `tone()` |
| LEDs × 3 | 3 | Green / Yellow / Red |
| 220 Ω resistors | 4 | 3 × LED, 1 × LCD backlight |
| Tactile push button | 1 | Acknowledge button |

---

## Wiring

### LCD 16×2 (4-bit parallel)

| LCD Pin | Arduino Pin |
|---|---|
| RS | D8 |
| EN | D9 |
| D4 | D4 |
| D5 | D5 |
| D6 | D6 |
| D7 | D7 |
| RW | GND |
| V0 | 10 kΩ pot wiper |
| A (backlight +) | 5 V via 220 Ω |
| K (backlight −) | GND |

### 4×4 Matrix Keypad

| Keypad | Arduino Pin |
|---|---|
| ROW 1 | A0 (D14) |
| ROW 2 | A1 (D15) |
| ROW 3 | A2 (D16) |
| ROW 4 | A3 (D17) |
| COL 1 | D10 |
| COL 2 | D11 |
| COL 3 | D12 |
| COL 4 | D13 |

### Status LEDs & Other

| Component | Arduino Pin |
|---|---|
| LED — Green (idle) | A4 (D18) |
| LED — Yellow (active) | A5 (D19) |
| LED — Red (ACK) | D3 |
| Buzzer | D2 |
| Acknowledge button (active LOW, pull-up) | A4 (D18) *(see note)* |

> **Note:** `LED_IDLE` (A4) and `ACK_BTN_PIN` (A4, digital 18) share the same physical pin on Arduino Uno.  
> You must choose **one of two options**:  
> - **(a) LED only (default):** the idle LED lights on A4; no physical button is needed — use keypad `B` or serial acknowledgement instead.  
> - **(b) Button only:** connect the button to a different free digital pin, update `ACK_BTN_PIN` in `Config.h` to that pin number, and reassign or remove `LED_IDLE`.

---

## Getting Started

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) 1.8+ or Arduino IDE 2.x
- No additional libraries needed — only built-in **LiquidCrystal** and **EEPROM**

### Installation

1. Clone or download this repository.
2. Open `CommandTerminal/CommandTerminal.ino` in the Arduino IDE.
3. Select your board (**Tools → Board → Arduino Uno** or Nano) and port.
4. Click **Upload**.

### Serial Interface (9600 baud)

| Command | Effect |
|---|---|
| `TURN ON PUMP\n` | Enqueue normal-priority command `TURN ON PUMP` |
| `!EMERGENCY STOP\n` | Enqueue urgent command (double beep + urgent banner) |
| `?STATUS\n` | Print current state and queue depth |
| `?LOG\n` | Dump EEPROM command log (8 entries) |
| `?CLEAR\n` | Erase EEPROM log |

### Keypad Controls

| Key | Function |
|---|---|
| `0`–`9` | Append digit to input buffer |
| `A`–`D` | See below |
| `A` | Toggle input mode on/off |
| `B` | Acknowledge front queued command |
| `C` | Clear input buffer |
| `#` | Submit typed command (Enter) |
| `*` | Backspace (delete last character) |

---

## State Machine

```
         ┌──────────┐
    ┌───▶│  IDLE    │◀─────────────────────────────────────┐
    │    └────┬─────┘                                      │
    │         │ command enqueued                           │
    │         ▼                                            │
    │    ┌──────────┐ acknowledge()   ┌──────────┐         │
    │    │  ACTIVE  │────────────────▶│   ACK    │──2 s──▶│
    │    └────┬─────┘                 └──────────┘
    │         │ key 'A' pressed
    │         ▼
    │    ┌──────────┐
    └────│  INPUT   │ '#' submits, 'A' cancels
         └──────────┘
```

---

## License

MIT — see [LICENSE](LICENSE) for details.
