#ifndef WifiFunctionDeclarations_h
#define WifiFunctionDeclarations_h


#include <WiFi.h>
#include <PubSubClient.h>
#include "esp_secrets.h"

#ifndef DEBUG
#define DEBUG
#endif
// constants for MQTT & WiFi
char ssid[] = SECRET_SSID;    // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int status = WL_IDLE_STATUS;  // the WiFi radio's status

const char* mqtt_server = "10.1.10.10"; // Replace with your MQTT broker IP address
const char* topic = "ToDevice/NameOfMachine"; // Replace with your topic
const char* hostTopic = "ToHost/NameOfMachine"; // Replace with your topic
const char* deviceID = "NameOfMachine"; //NameOfMachine


WiFiClient espClient;
PubSubClient client(espClient);

void wifiSetup();
void reconnectMQTT();
void handleWifiReconnect();
void handleMQTTReconnect();
void onSolve();
void onReset();

//************WIFI and MQTT FUNCTIONS************

void callback(char* thisTopic, byte* message, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(thisTopic);
  Serial.print("] ");
  Serial.println("Message: ");
  
  // Convert byte array to C-style string
  char messageArrived[length + 1];
  memcpy(messageArrived, message, length);
  messageArrived[length] = '\0'; // Null-terminate the string
 
  for (unsigned int i = 0; i < length; i++) {
    messageArrived[i] = tolower(messageArrived[i]);
  }

  // Use strcmp to look for certain messages
  if (strcmp(messageArrived, "solve") == 0){
    Serial.print("Solve Received from MQTT Message!");
    onSolve();
  }   else if (strcmp(messageArrived, "reset") == 0){
    Serial.print("reset received from MQTT Message!");
    onReset();
  }  
    else {
    Serial.print("Unknown message received from MQTT Message!");
  }
  Serial.println(messageArrived);
  Serial.println();
}

void wifiSetup() {
  #ifdef DEBUG
  Serial.println();
  Serial.println("****************************");
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);
  WiFi.setAutoReconnect(true);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP Address: ");
  Serial.print(WiFi.localIP());
 #endif
 }

void MQTTsetup() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.subscribe(topic);
  reconnectMQTT();
}

void reconnectMQTT() {
  while (!client.connected()) {

    // Debug info
    #ifdef DEBUG
    Serial.print("Attempting to connect to the MQTT broker at ");
    Serial.println(mqtt_server);
    #endif
    // Attempt to connect
    if (client.connect(deviceID)) {
      #ifdef DEBUG
      Serial.println("Connected to MQTT broker");
      #endif
      client.publish(hostTopic, "Alchemy Machine Connected!");
      client.subscribe(topic);
      #ifdef DEBUG
      Serial.println("Subscribed to topic: ");
      Serial.println(topic);
      #endif
    } else {
      // Debug info
      #ifdef DEBUG
      Serial.print("Failed to connect to MQTT broker, rc =");
      Serial.print(client.state());
      Serial.println("Retrying in 5 seconds...");
      #endif
      delay(5000);
    }
  }
}

void handleWifiReconnect() {
  if (WiFi.status() != WL_CONNECTED) {
    #ifdef DEBUG
    Serial.println("WiFi connection lost. Reconnecting...");
    #endif
    wifiSetup();
  }
}

void handleMQTTReconnect() {
  if (!client.connected()) {
    #ifdef DEBUG
    Serial.println("MQTT connection lost. Reconnecting...");
    #endif
    reconnectMQTT();
  }
}

#endif
