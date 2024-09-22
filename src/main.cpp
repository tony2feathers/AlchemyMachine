//+------------------------------------------------------------------------
//
// Two Feathers LLC - (c) 2024 Robert Nelson. All Rights Reserved.
//
// File: main.cpp (Created for the Alchemy machine as part of **REDACTED** Escape Room Game)
//
// Description:
//
//      Program for handling the Alchemy Puzzle and props in **REDACTED** Escape Room game.
//      This program is designed to run on a NodeMCU-32S microcontroller and is written in C++.
//      A laser sensor is used to detect the presence of a laser reflect off multiple mirrors, giving the impression of
//      powering the device. When the laser is detected, the device will perform a series of checks. First, a check of two RFID
//      readers to determine if correct beakers are in place. If not, the lights (LS1) will flash red (for as long as the laser
//      is powering the device). If the correct beakers are in place and the door is not closed (sensed using a limit switch), the
//      lights will flash green. Once the door is closed, the maglock will engage and the lights will turn solid green.
//      Next, light strip 2 and 3 will activate (LS2 will be Red and LS3 will be blue). The lights will be sequencing to simulate
//      movement along the backside of PVC pipe. After five seconds, the sequencing lights will stop, a latch lock will release
//      and light strip 4 will create a purple glow.
//
// History:     JUL-27-2024       tony2feathers     Created
//              JUL-28-2024       tony2feathers     Added MQTT and WiFi setup functions
//              AUG-31-2024       tony2feathers     Added main setup function and main loop, added libraries
//              SEP-20-2024       tony2feathers     Changed to PN5180 RFID readers using ISO15693 protocol and added new lighting effects
//              SEP-21-2024       tony2feathers     Bug fixes and refactored lighting effects and case switch statement



#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "WifiFunctions.h"
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include "lights.h"
#include <PN5180.h>
#include <PN5180ISO15693.h>

#define DEBUG

//Globals

enum PuzzleState {Initializing, Unpowered, Powered, Solved, GameOver};
PuzzleState puzzleState = Initializing;

unsigned long solvedMillis = 0;
unsigned long currentMillis = 0;

// Constants
const byte laserPin = 34;
const byte beakerDoor = 32;
const byte crystalDoor = 33;
const byte lightStrip1 = 25; // Beaker Lights
const byte lightStrip2 = 26; // Red Pipe Lights
const byte lightStrip3 = 27;  // Purple lights for crystal compartment
const byte lightStrip4 = 13; // Blue Pipe Lights
const byte limitSwitch = 14; // Reed switch for the beaker door
const byte numReaders = 2;

uint8_t correctUid[][8] = {
  {0x3C, 0x33, 0x13, 0x66, 0x08, 0x01, 0x04, 0xE0}, // Red beaker
  {0x04, 0x3A, 0x13, 0x66, 0x08, 0x01, 0x04, 0xE0}  // Blue beaker
};

// If any of the readers detect this tag, the puzzle will be reset
uint8_t resetUid[8] = {0x24,0x43,0x13,0x66,0x08,0x01,0x04,0xE0};

uint8_t noUid[8] = {0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0};

// Array to record the value of the last UID read by each reader
uint8_t lastUid[numReaders][8];

bool errorFlag = false;
bool beakersCorrect = false;
bool alchemyPower = false;
bool doorClosed = false;

// Each PN5180 reader requires unique NSS, BUSY, and RESET pins,
// as defined in the constructor below
PN5180ISO15693 nfc[] = {
  PN5180ISO15693(21,5,22),
  PN5180ISO15693(16,4,17)
};

// Lights
const int Strip1Length = 27;  // Beaker Lights
const int Strip1Start = 0;
const int Strip2Length = 8;  // Pipe Lights (Left - Red)  
const int Strip2Start = 0;
const int Strip3Length = 22;  // Pipe Lights (Middle - Purple)
const int Strip3Start = 0;
const int Strip4Length = 8;  // Pipe Lights (Right - Blue)
const int Strip4Start = 0; 

// Create instances of the lights
NeoPatterns LS1(Strip1Length, lightStrip1, NEO_GRB + NEO_KHZ800, nullptr);
NeoPatterns LS2(Strip2Length, lightStrip2, NEO_GRB + NEO_KHZ800, nullptr);
NeoPatterns LS3(Strip3Length, lightStrip3, NEO_GRB + NEO_KHZ800, nullptr);
NeoPatterns LS4(Strip4Length, lightStrip4, NEO_GRB + NEO_KHZ800, nullptr);


//Function Prototypes
void onSolve();
void onReset();
void gameOver();
void showCurrentStatus();

void setup() {

  // Open a serial connection for debugging
  Serial.begin(115200);
  
  Serial.println("Setup function begining");

  // Print out the file and the date at which it was last compiled
  Serial.println(__FILE__ __DATE__);

  // Connect to the WiFi network
  wifiSetup();
  // Connect to the MQTT broker
  MQTTsetup();
  
  // Initialize the GPIO pins
  // Initialize the laser sensor
  #ifdef DEBUG
  Serial.println("Setting up laser sensor");
  #endif
  pinMode(laserPin, INPUT);
  delay(500);

  // Initialize the door locks & lock the crystal door (beaker door is unlocked by default, crystal door must remain low and only triggered high for a moment to release the lock)
  Serial.println("Setting up door locks");
  pinMode(beakerDoor, OUTPUT);
  pinMode(crystalDoor, OUTPUT);
  Serial.println("Ensuring Beaker door is unlocked!");
  digitalWrite(beakerDoor, LOW);
  delay(500);
  Serial.println("Ensuring Crystal door is not active!"); // Momentary high signal will unlock the door
  digitalWrite(crystalDoor, LOW);
  
  // Initialize the limit switch
  pinMode(limitSwitch, INPUT_PULLUP);
  


  Serial.println("Setting up RFID readers");
  for(int i=0; i<numReaders; i++){
    Serial.print("Reader #");
    Serial.println(i);
    Serial.println(F("Initialising..."));
    nfc[i].begin();
    Serial.println(F("Resetting..."));
    nfc[i].reset();
    Serial.println(F("Enabling RF field..."));
    nfc[i].setupRF();
  }

  delay(500);

  // Initialize the lights
  LS1.begin();
  LS1.show();
  LS1.setBrightness(255);

  LS2.begin();
  LS2.show();
  LS2.setBrightness(255);

  LS3.begin();
  LS3.show();
  LS3.setBrightness(255);

  LS4.begin();
  LS4.show();
  LS4.setBrightness(255);

  delay(50);

  // Color Wipe the lights red and then blue with a 1 second delay
  LS1.ColorSet(LS1.Color(255, 0, 0), Strip1Start, Strip1Length);
  delay(1000);
  LS1.ColorSet(0x0000FF, Strip1Start, Strip1Length);
  delay(1000);
  LS2.ColorSet(0xFF0000, Strip2Start, Strip2Length);
  delay(1000);
  LS2.ColorSet(0x0000FF, Strip2Start, Strip2Length);
  delay(1000);
  LS3.ColorSet(0xFF0000, Strip3Start, Strip3Length);
  delay(1000);
  LS3.ColorSet(0x0000FF, Strip3Start, Strip3Length);
  delay(1000);
  LS4.ColorSet(0xFF0000, Strip4Start, Strip4Length);
  delay(1000);
  LS4.ColorSet(0x0000FF, Strip4Start, Strip4Length);
  delay(1000);

  
  // Set the lights to a solid color
  LS1.ColorSet(LS1.Color(0, 0, 0), Strip1Start, Strip1Length);
  LS2.ColorSet(LS2.Color(0, 0, 0), Strip2Start, Strip2Length);
  LS3.ColorSet(LS3.Color(0, 0, 0), Strip3Start, Strip3Length);
  LS4.ColorSet(LS4.Color(0, 0, 0), Strip4Start, Strip4Length);
  LS1.show();
  LS2.show();
  LS3.show();
  LS4.show();
  delay(500);
  Serial.println("Lights setup complete");  
  
  // DEBUG block for RFID scanning using the serial monitor
  /*
  #ifdef DEBUG
  Serial.println("Type 'go' to begin scanning RFID tags");

  // Wait for the user to type 'go' in the serial monitor
  while(!Serial.available() || Serial.readStringUntil('\n') != "go") {
    Serial.println("Waiting for 'go' command...");
    delay(500);
  }

  Serial.println("'go' command received. Scanning RFID tags...");

  // Loop through each RFID reader to scan tags
  for (int i=0; i<numReaders; i++) {
    Serial.print("Place the tag near reader #");
    Serial.println(i);

    // Wait for an RFID tag to be detected
    uint8_t testUid[numReaders][8];
    while(nfc[i].getInventory(testUid[i]) != ISO15693_EC_OK) {
      Serial.println("Waiting for tag on reader #");
      Serial.println(i);
      delay(500);

    }

    // Show the scanned UID in serial output
    Serial.print("Scanned UID for Reader #");
    Serial.print(i);
    Serial.print(": ");
    for (int j=0; j<8; j++) {
      Serial.print(testUid[i][j], HEX);
      if(j <7) Serial.print(" ");
    }
    Serial.println();

    // Wait for the user to type 'ok' in the serial monitor
    Serial.println("Type 'ok' to continue scanning...");
    while(!Serial.available() || Serial.readStringUntil('\n') != "ok") {
      Serial.println("Waiting for 'ok' command...");
      delay(500);
    }

    Serial.println("'ok' command received. Scanning next tag...");
    }

  Serial.println("All tags scanned. Setup complete.");
  #endif*/

  Serial.println("Setup function complete");

}

void loop() {
  // Check the current state of the puzzle
  alchemyPower = (digitalRead(laserPin) == LOW);
  doorClosed = (digitalRead(limitSwitch) == LOW);

  switch (puzzleState)
  { 
  case Initializing: // Should only be during setup and onReset
    {
    Serial.println("Puzzle State: Initializing");
    // Turn the lights off
    Serial.println("Turning off lights");
    if(LS1.ActivePattern!= none) {
    LS1.ColorSet(LS1.Color(0, 0, 0), Strip1Start, Strip1Length);
    LS1.show();
    }
    if(LS2.ActivePattern!= none) {
    LS2.ColorSet(LS2.Color(0, 0, 0), Strip2Start, Strip2Length);
    LS2.show();
    }
    if(LS3.ActivePattern!= none) {
    LS3.ColorSet(LS3.Color(0, 0, 0), Strip3Start, Strip3Length);
    LS3.show();
    }
    if(LS4.ActivePattern!= none) {
    LS4.ColorSet(LS4.Color(0, 0, 0), Strip4Start, Strip4Length);
    LS4.show();
    }

    // Lock the crystal door and unlock the beaker door
    Serial.println("Locking crystal door and unlocking beaker door");
    digitalWrite(beakerDoor, LOW);
    //digitalWrite(crystalDoor, LOW);

    // Set the puzzle state to Unpowered
    puzzleState = Unpowered;

    break;
    }
  case Unpowered:
  {
    Serial.println("Puzzle State: Unpowered");

    // Ensure all lights are turned off when unpowered
    if (LS1.ActivePattern != none)
    {
      LS1.ColorSet(LS1.Color(0, 0, 0), Strip1Start, Strip1Length);
      LS1.show();
    }
    if (LS2.ActivePattern != none)
    {
      LS2.ColorSet(LS2.Color(0, 0, 0), Strip2Start, Strip2Length);
      LS2.show();
    }
    if (LS3.ActivePattern != none)
    {
      LS3.ColorSet(LS3.Color(0, 0, 0), Strip3Start, Strip3Length);
      LS3.show();
    }
    if (LS4.ActivePattern != none)
    {
      LS4.ColorSet(LS4.Color(0, 0, 0), Strip4Start, Strip4Length);
      LS4.show();
    }
    // Check if the laser is detected
    if(alchemyPower)
    {
      Serial.println("Laser detected, Alchemy machine is now powered! Checking for beaker placement...");
      puzzleState = Powered;
    }
    else
    {
      Serial.println("Laser not detected, Alchemy machine is not powered!");
      puzzleState = Unpowered;
    }
    delay(100);
    break;
  }
  case Powered:
  {
    Serial.println("Puzzle State: Powered");
    
    beakersCorrect = true;
    // Check the RFID readers
    for (int i = 0; i < numReaders; i++)
    {
      // Variable to store the ID of any tag read by this reader
      uint8_t thisUid[8];
      // Try to read a tag ID (or "get inventory" in ISO15693-speak)
      ISO15693ErrorCode rc = nfc[i].getInventory(thisUid);

      // If no tag is detected, set the UID to all zeros
      if (rc != ISO15693_EC_OK)
      {
        if(memcmp(lastUid[i], noUid, 8) != 0)
        {
          // Tag was removed, clear last UID
          memset(lastUid[i], 0, 8); // Clear last UID to reflect no tag
          Serial.print("Tag removed from reader #"); 
          Serial.println(i);
          showCurrentStatus(); // Show the cleared status
        }
      beakersCorrect = false; // No tag detected, so beakers are incorrect
        continue; // move to the next reader
      }

      // If a new tag is detected, update the lastUID array
      if(memcmp(thisUid, lastUid[i], 8) != 0)
      {
        memcpy(lastUid[i], thisUid, 8);
        showCurrentStatus(); // Show the updated UID
      }
      
        // If this the detected tag matches the correct tag
        if (memcmp(thisUid, correctUid[i], 8) != 0)
        {
          beakersCorrect = false; // Incorrect tag detected
        }

        // If reset tag is detected, call reset function
      if (memcmp(thisUid, resetUid, 8) == 0)
      {
        onReset();
      }      
    }
    
    delay(50);
    if (!beakersCorrect)
    {
      // Flash red lights if beakers are incorrect
      if (LS1.ActivePattern != flash || LS1.Color1 != LS1.Color(255, 0, 0))
      {
        LS1.Flash(LS1.Color(255, 0, 0), 80, Strip1Start, Strip1Length, forward);
      }
      showCurrentStatus();
      delay(2000);
    }
    else if (!doorClosed)
    {
      if(alchemyPower && beakersCorrect)
      {
        //Lock the beaker door
        digitalWrite(beakerDoor, HIGH);
      }
      // Flash green lights if beakers are correct but door is open
      if (LS1.ActivePattern != flash || LS1.Color1 != LS1.Color(0, 255, 0))
      {
        LS1.Flash(LS1.Color(0, 255, 0), 80, Strip1Start, Strip1Length, forward);
      }
    }
    else if(alchemyPower && beakersCorrect && doorClosed)
    {
      // Beakers are correct, door is closed, move to solved state
      onSolve();
      
    }

    // Continue checking for power loss
    if (!alchemyPower)
    {
      LS1.ActivePattern = none;
      delay(50);
      LS1.ColorSet(LS1.Color(0, 0, 0), Strip1Start, Strip1Length);
      LS1.show();
      LS1.ActivePattern = none;
      delay(50);
      LS2.ColorSet(LS2.Color(0, 0, 0), Strip2Start, Strip2Length);
      LS2.show();
      LS2.ActivePattern = none;
      delay(50);
      LS3.ColorSet(LS3.Color(0, 0, 0), Strip3Start, Strip3Length);
      LS3.show();
      LS3.ActivePattern = none;
      delay(50);
      LS4.ColorSet(LS4.Color(0, 0, 0), Strip4Start, Strip4Length);
      LS4.show();
      puzzleState = Unpowered;
    }
    break;
  }
  case Solved:
    {
    Serial.println("Puzzle State: Solved");
    //Check if game has been solved more than 30 minutes, if so turn off all lights.
    currentMillis = millis();
    if (currentMillis - solvedMillis > 1800000)
    {
      gameOver();
    }
    else {
      // Check and Set the final light states
    if(LS1.Color1 != LS1.Color(0, 255, 0))
    {
      LS1.ColorSet(LS1.Color(0, 255, 0), Strip1Start, Strip1Length);
      LS1.show();
    }
    if(LS2.Color1 != LS2.Color(0, 255, 0))
    {
      LS2.ColorSet(LS2.Color(0, 255, 0), Strip2Start, Strip2Length);
      LS2.show();
    }
    if(LS3.Color1 != LS3.Color(128, 0, 128))
    {
      LS3.ColorSet(LS3.Color(128, 0, 128), Strip3Start, Strip3Length);
      LS3.show();
    }
    if(LS4.Color1 != LS4.Color(0, 255, 0))
    {
      LS4.ColorSet(LS4.Color(0, 255, 0), Strip4Start, Strip4Length);
      LS4.show();
    }
    }
    // Check the RFID readers
    for (int i = 0; i < numReaders; i++)
    {
      // Variable to store the ID of any tag read by this reader
      uint8_t thisUid[8];
      // Try to read a tag ID (or "get inventory" in ISO15693-speak)
      ISO15693ErrorCode rc = nfc[i].getInventory(thisUid);

      // If reset tag is detected, call reset function
      if (memcmp(thisUid, resetUid, 8) == 0)
      {
        onReset();
      }
      }
    break;
    }
  case GameOver:
    {
    Serial.println("Puzzle State: Game Over...Awaiting Reset");
    // Turn all lights off if they are not already off
    if(LS1.ActivePattern != none)
    {
    LS1.ColorSet(LS1.Color(0, 0, 0), Strip1Start, Strip1Length);
    LS1.show();
    }
    if(LS2.ActivePattern != none)
    {
    LS2.ColorSet(LS2.Color(0, 0, 0), Strip2Start, Strip2Length);
    LS2.show();
    }
    if(LS3.ActivePattern != none)
    {
    LS3.ColorSet(LS3.Color(0, 0, 0), Strip3Start, Strip3Length);
    LS3.show();
    }
    if(LS4.ActivePattern != none)
    {
    LS4.ColorSet(LS4.Color(0, 0, 0), Strip4Start, Strip4Length);
    LS4.show();
    }

    // Check the RFID readers
    for (int i = 0; i < numReaders; i++)
    {
      // Variable to store the ID of any tag read by this reader
      uint8_t thisUid[8];
      // Try to read a tag ID (or "get inventory" in ISO15693-speak)
      ISO15693ErrorCode rc = nfc[i].getInventory(thisUid);

      // If reset tag is detected, call reset function
      if (memcmp(thisUid, resetUid, 8) == 0)
      {
        onReset();
      }
    }
    delay(1000);
  break;
  }
  }
  delay(50);      
  client.loop();
  LS1.Update();
  LS2.Update();
  LS3.Update();
  LS4.Update();
}

void onSolve()
{
  Serial.println("Puzzle Solved!");

  // Start the light sequences for LS2 and LS4
  LS2.AcceleratingSequence(LS2.Color(255, 0, 0), Strip2Start, Strip2Length, forward);
  LS3.AcceleratingSequence(LS3.Color(128, 0, 128), Strip3Start, Strip3Length, forward);
  LS4.AcceleratingSequence(LS4.Color(0, 0, 255), Strip4Start, Strip4Length, reverse);
    
  // Run for 5 seconds (use millis() for non-blocking delay)
  unsigned long startTime = millis();
  while(millis() - startTime < 5000)
  {
    LS2.Update();
    LS3.Update();
    LS4.Update();
  }
  // Clear all light effects before setting the final light states
  LS1.ActivePattern = none;
  LS2.ActivePattern = none;
  LS3.ActivePattern = none;
  LS4.ActivePattern = none;
  delay(50);
  LS1.ColorSet(LS1.Color(0, 0, 0), Strip1Start, Strip1Length);
  LS2.ColorSet(LS2.Color(0, 0, 0), Strip2Start, Strip2Length);
  LS3.ColorSet(LS3.Color(0, 0, 0), Strip3Start, Strip3Length);
  LS4.ColorSet(LS4.Color(0, 0, 0), Strip4Start, Strip4Length);
  LS1.show();
  LS2.show();
  LS3.show();
  LS4.show();
  delay(50);
  // Set LS3 to purple, LS1, 2, and 4 to green and trigger the relay momentarily to unlock the crystal door
  // Set the final light states
  LS1.ColorSet(LS1.Color(0, 255, 0), Strip1Start, Strip1Length);
  LS2.ColorSet(LS2.Color(0, 255, 0), Strip2Start, Strip2Length);
  LS3.ColorSet(LS3.Color(128, 0, 128), Strip3Start, Strip3Length);
  LS4.ColorSet(LS4.Color(0, 255, 0), Strip4Start, Strip4Length);
  LS1.show();
  LS2.show();
  LS3.show();
  LS4.show();
  delay(50);  
  digitalWrite(crystalDoor, HIGH);
  delay(10);
  digitalWrite(crystalDoor, LOW);


  solvedMillis = millis();  
  puzzleState = Solved;
}

void onReset()
{
  Serial.println("Puzzle Reset!");
  // Lock the crystal door and unlock the beaker door
  digitalWrite(crystalDoor, HIGH);
  delay(10);
  digitalWrite(crystalDoor, LOW);
  delay(10);
  digitalWrite(beakerDoor, LOW);
  // Turn off all lights
  LS1.ActivePattern = none;
  LS2.ActivePattern = none;
  LS3.ActivePattern = none;
  LS4.ActivePattern = none;
  delay(50);
  LS1.ColorSet(LS1.Color(0, 0, 0), Strip1Start, Strip1Length);
  LS2.ColorSet(LS2.Color(0, 0, 0), Strip2Start, Strip2Length);
  LS3.ColorSet(LS3.Color(0, 0, 0), Strip3Start, Strip3Length);
  LS4.ColorSet(LS4.Color(0, 0, 0), Strip4Start, Strip4Length);
  LS1.show();
  LS2.show();
  LS3.show();
  LS4.show();
  delay(50);
  puzzleState = Unpowered;
}

void gameOver()
{
  Serial.println("Game Over!");
  // Lock the crystal door and unlock the beaker door
  //digitalWrite(crystalDoor, LOW);
  digitalWrite(beakerDoor, LOW);
  puzzleState = GameOver;
}

void showCurrentStatus() {
  for(int i=0; i<numReaders; i++) {
    Serial.print(F("Reader #"));
    Serial.print(i);
    Serial.print(": "); 
    if(memcmp(lastUid[i], noUid, 8) == 0) {
      Serial.print(F("---"));
    }
    else {
      for (int j=0; j<8; j++) {
        // If single byte, pad with a leading zero
        if(lastUid[i][j]<0x10) { Serial.print("0"); }
        Serial.print(lastUid[i][j], HEX);
      }
      if(memcmp(lastUid[i], correctUid[i], 8) == 0) {
        Serial.print(F(" - CORRECT"));
      }
      else {
        Serial.print(F(" - INCORRECT"));
      }
    }
    Serial.println("");
 }
 Serial.println(F("---"));
}