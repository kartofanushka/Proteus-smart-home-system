# ATmega32 Smart Thermostat & Access Control System

This repository contains the embedded C firmware and hardware simulation for a comprehensive smart thermostat system. The project integrates a secure PIN-based access control mechanism, real-time temperature monitoring, serial telemetry, and a hardware-level emergency override using an **ATmega32** microcontroller.

## Core Features

* **Authentication:** System access requires a 4-digit PIN input via a matrix keypad before granting access to temperature controls.
* **Active Temperature Control:** Reads analog data from an LM35 sensor, compares it against a user-configurable threshold, and automatically triggers a cooling fan if the temperature exceeds the limit.
* **Live Serial Telemetry:** Streams real-time system logs, temperature readings, and fan states to a computer or virtual terminal via UART (9600 baud).
* **Hardware Emergency Override:** Utilizes an external hardware interrupt (INT0) to instantly lock the system, disable actuators, and trigger visual/audio alarms in case of an emergency.
* **Interactive UI:** Provides direct feedback through a 16x2 alphanumeric LCD, status LEDs, and a piezoelectric buzzer.

---

## Simulation Preview
https://youtu.be/DanRkrJZZwI

---

## Hardware Architecture & Pin Configuration

The system is designed to run at an 8.0 MHz clock frequency. The component mapping is structured as follows:

### Inputs

* **Temperature Sensor (LM35):** Connected to the internal ADC via **PA0**.
* **4x4 Matrix Keypad:** Connected to **PORTC (PC0-PC7)** for polling user input (PIN entry and menu navigation).
* **Emergency Button:** Connected to **PD2 (INT0)**. Configured as a falling-edge hardware interrupt.

### Outputs

* **16x2 LCD (LM016L):** * Data bus connected to **PORTB**.
* Control pins (RS, RW, E) connected to **PA5, PA6, PA7**.


* **Cooling Fan (Motor):** Driven via **PD4**.
* **Status Indicators:** * Buzzer on **PA1** (Audio feedback for key presses, access denial, and alarms).
* Blue LED on **PA3** (Indicates normal operation / system safe).
* Pink LED on **PA4** (Indicates warning, fan active, or access denied).


* **Serial Terminal (UART):** Uses hardware TX/RX pins to broadcast system logs.

---

## System State Machine

The firmware operates on a robust finite state machine (FSM) to handle different operational modes seamlessly:

| State | Description |
| --- | --- |
| `STATE_LOGIN` | The boot state. Prompts the user for a 4-digit PIN (default: `1234`). Denies access and sounds an alarm upon failure. |
| `STATE_MENU` | Main navigation hub. Allows the user to select Mode 1 (View) or Mode 2 (Set Threshold). |
| `STATE_VIEW` | Active monitoring mode. Displays live temperature. If the temperature exceeds the threshold, the fan activates. Data is continuously pushed to the UART terminal. |
| `STATE_SET` | Threshold configuration. Allows the user to type a new 2-digit maximum temperature limit via the keypad and save it using the `=` key. |
| `STATE_EMERGENCY` | Triggered exclusively via hardware interrupt (`INT0`). Immediately shuts off the fan, locks the system in an infinite loop, and activates continuous alarms. Requires a hard reset to clear. |

---

## Firmware Highlights

* **Interrupt Service Routine (ISR):** The emergency stop is detached from the main polling loop. Pressing the emergency button instantly triggers the `INT0_vect` ISR, guaranteeing an immediate system halt regardless of the current FSM state.
* **ADC Conversion:** Uses the ATmega32's 10-bit ADC with a prescaler of 64. The raw analog value is converted to Celsius using fixed-point arithmetic (`(ADC * 500) / 1024`).
* **Keypad Debouncing:** Implements structured microsecond and millisecond software delays to prevent phantom key presses during matrix scanning.

