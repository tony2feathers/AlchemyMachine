# Alchemy Machine Puzzle

## Project Overview

The **Alchemy Machine Puzzle** is a custom-designed puzzle for the **REDACTED** Escape Room game, developed by Two Feathers LLC. This project is built using an ESP32-based NodeMCU-32S microcontroller and includes RFID readers, a laser sensor, NeoPixel light strips, and door locks to create an immersive puzzle experience.

### Key Features:
- **Laser Activation**: The puzzle starts when the laser beam is detected.
- **RFID Detection**: Detects the correct placement of RFID-tagged beakers.
- **Door Locks**: Controls the locking and unlocking of doors based on the puzzle's progress.
- **Light Effects**: Utilizes NeoPixel light strips for customizable lighting sequences.
- **MQTT Integration**: Allows remote control via MQTT messages to reset or solve the puzzle.

---

## Hardware Requirements 
- **NodeMCU-32S (ESP32)**
- **PN5180 RFID Readers** (2x)
- **NeoPixel Light Strips**
- **Laser Sensor**
- **Maglock** (for door locking)
- **Momentary Latch Lock**

---

### Software Requirements 

- **Arduino IDE** or **PlatformIO** (VS Code Extension)
- **PubSubClient Library** (for MQTT communication)
- **WiFi Library** (for handling WiFi connections)
- **Adafruit NeoPixel Library** (for LED control)
- **PN5180 Library** (for RFID reader control)

### Required Libraries in main.cpp: 
```cpp
#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "WifiFunctions.h"
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include "lights.h"
#include <PN5180.h>
#include <PN5180ISO15693.h>
```
### Setup Instructions

1. **Clone the Repository**:
   - Clone the project to your local machine using:
   ```
   bash
   git clone https://github.com/yourusername/alchemy-machine-puzzle.git
   ```

2. **WiFi Configuration**:
    - Configure your WiFi settings in esp_secrets.h.
    - The file should look like this:
    ```
    #define SECRET_SSID ""   // your network SSID (name)
    #define SECRET_PASS " "  // your network password (use for WPA, or use as key for WEP)
    ```

3. **Upload Code**:
    - Open the project in VS Code with PlatformIO or the Arduino IDE.
    - Ensure all necessary libraries are installed.
    - Upload the code to the NodeMCU-32S.

4. **Configure MQTT**:
    - Set up your MQTT broker (e.g., Mosquitto) to communicate with the puzzle. The device will connect to the broker at 10.1.10.55 and subscribe to the topic ToDevice/NameOfMachine.

## MQTT Commands

- **Solve the Puzzle**:
    Send the message solve to the topic ToDevice/NameOfMachine to simulate the puzzle being solved. The machine will execute the solve sequence with lights and door unlocking.
- **Reset the Puzzle**:
    Send the message reset to the topic ToDevice/NameOfMachine to reset the puzzle to its initial state.

## How The Puzzle Works

This puzzle simulates an alchemy machine, and it operates in several stages based on user interaction with beakers, a door, and a laser sensor. Here's how the puzzle works step by step:

1. **Unpowered State**:
    - Initially, the alchemy machine is unpowered. The laser sensor detects whether a laser is aligned with the machine. 
    - When the laser beam is broken or missing, the machine remains in an unpowered state with no lights active.

2. **Powered State**:
    - Once the laser is detected, the machine enters the powered state.
    - The next step is to verify the placement of two beakers using RFID readers.
    - If either beaker is missing or placed incorrectly, the lights on the beaker strip (LS1) will flash red.
    - If both beakers are placed correctly, the beaker door is checked. If it is open, LS1 will flash green to signal the user to close the door.

3. **Solving the Puzzle**:
    - When both beakers are placed correctly and the door is closed, the machine transitions into the "solved" state.
    - Lights on LS2 and LS4 will sequence, simulating the alchemical process. After 5 seconds, the lights stop, and LS3 will glow purple.
    - The maglock for the crystal door is momentarily released, allowing the player to retrieve the "crystal."

4. **Solved State**:
    - Once the puzzle is solved, LS1, LS2, and LS4 will glow green, while LS3 remains purple.
    - The puzzle will stay in the solved state until reset via an RFID tag or an MQTT command.
    - If more than 30 minutes pass without a reset, the puzzle enters a "Game Over" state, turning off all lights and awaiting a reset.

5. **Resetting the Puzzle**:
    - The puzzle can be reset using an RFID tag or by sending a reset command via MQTT. This will return the machine to its unpowered state, allowing the puzzle to start over.

## Acknowledgements

  - Developed by Two Feathers, LLC as part of a live action Escape Room Game
  - Special thanks to Alastair Aitchison. His videos and patreon provide unmeasureable inspiration and knowledge needed to get these projects across the finish line 
    
