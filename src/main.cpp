// Basic demo for accelerometer readings from Adafruit MPU6050
#include <Arduino.h>

#include "countdown.h"
#include "secret.h" // Include your secret credentials here or in a separate file


#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>


#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <ArduinoJson.h>


// #define SDA_PIN GPIO_NUM_33 // Default SDA pin for ESP32
// #define SCL_PIN GPIO_NUM_35 // Default SCL pin for ESP32

const char* hostname = "Dribble-O-Mat";

// Json Variable to Hold Sensor Readings
StaticJsonDocument<1024> readings;

Countdown countdown(60000); // Countdown timer for 1 second

Adafruit_MPU6050 mpu;
uint8_t LED = GPIO_NUM_15;    // Pin for the onboard LED
uint8_t buttonPin = GPIO_NUM_37; // Pin for the button

AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;

unsigned int counter = 0; // Counter for motion detection


enum Mode {
  normal,   // Normal mode, no motion detection
  armed,    // Armed mode, motion detection is active
  running   // Running mode, countdown is active
};

Mode gameMode = normal; // Initial mode is normal


char buf[16];

String getSensorReadings()
{
  readings.clear(); // Clear previous readings
  readings["counter"] = String(counter); // Convert counter to string
  readings["countdown"] = String(int(countdown.timeLeft()/1000));
  readings["mode"] = String(gameMode);
  String jsonString;
  serializeJson(readings, jsonString);
  return jsonString;
}


// Initialize LittleFS
// This function mounts the LittleFS filesystem and checks for errors
void initLittleFS()
{
  if (!LittleFS.begin(true))
  {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  Serial.println("LittleFS mounted successfully");
}


// Initialize WiFi
void initWiFi()
{
  WiFi.mode(WIFI_AP_STA);  // Enable both AP and Station modes
  //WiFi.softAP(ap_ssid, ap_password);  // Start the access point
  WiFi.begin(sta_ssid, sta_password); // Connect to existing network
}


void notifyClients(String sensorReadings)
{
  ws.textAll(sensorReadings);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    // data[len] = 0;
    // String message = (char*)data;
    //  Check if the message is "getReadings"
    // if (strcmp((char*)data, "getReadings") == 0) {
    // if it is, send current sensor readings
    String sensorReadings = getSensorReadings();
    Serial.print(sensorReadings);
    notifyClients(sensorReadings);
    //}
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup(void)
{
  Serial.begin(115200);

  pinMode(LED, OUTPUT);   // Set LED pin as output
  pinMode(buttonPin, INPUT_PULLUP); // Set interrupt pin as input

  initWiFi();


  while (!mpu.begin())
  {
    digitalWrite(LED, HIGH);
    delay(100); // Turn on the LED to indicate failure
    digitalWrite(LED, LOW);
    delay(100); // Turn off the LED to indicate failure
    digitalWrite(LED, HIGH);
    delay(100); // Turn on the LED to indicate failure
    digitalWrite(LED, LOW);
    delay(700); // Turn off the LED to indicate failure
  }
  Serial.println("MPU6050 Found!");

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setHighPassFilter(MPU6050_HIGHPASS_1_25_HZ);
  mpu.setMotionDetectionThreshold(3); // 3 counts, about 0.25g
  mpu.setMotionDetectionDuration(10); // 10 ms
  mpu.setInterruptPinLatch(true);     // Keep it latched.  Will turn off when reinitialized.
  mpu.setInterruptPinPolarity(false); // Active HIGH
  mpu.setMotionInterrupt(true);

  initLittleFS();
  initWebSocket();

  delay(100);

  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
      Serial.println("Request for index.html");
      request->send(LittleFS, "/index.html", "text/html"); });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
            { 
      Serial.println("Request for index.html");
      request->send(LittleFS, "/settings.html", "text/html"); });

  server.serveStatic("/", LittleFS, "/");

  // Start server
  server.begin();
}

ulong nextCall = 0;
ulong lastMotion = 0;
uint16_t motionTimeout = 100; // 200 ms timeout for motion detection

bool oldState = false; // Variable to store the previous state of the interrupt pin
ulong lastButtonPress = 0; // Variable to store the last button press time

void loop()
{
  ulong currentMillis = millis();

  // REGION: Button Handling
  if (digitalRead(buttonPin) == LOW && oldState == true && lastButtonPress + 500 < currentMillis)
  {
    lastButtonPress = currentMillis; // Update the last button press time
    Serial.println("Button pressed, changing mode");

    // Change the game mode based on the current mode
    switch (gameMode)
    {
      case normal:
        gameMode = armed; // Change mode to armed
        countdown.set(60000); // Set countdown to 60 seconds
        Serial.println("Mode changed to armed");
        break;
      case armed:
        gameMode = normal; // Change mode to normal
        countdown.stop(); // Stop the countdown
        Serial.println("Mode changed to normal");
        break;
      case running:
        gameMode = normal; // Change mode to normal
        countdown.stop(); // Stop the countdown
        break;
      default:
        gameMode = normal; // Change mode to normal
        break;
    }
    notifyClients(getSensorReadings());
  }
  oldState = digitalRead(buttonPin) == LOW; // Update the oldState variable
  // ENDREGION: Button Handling


  // REGION: MPU6050 Motion Detection
  if (mpu.getMotionInterruptStatus())
  {
    if (currentMillis - lastMotion > motionTimeout)
    {
      lastMotion = currentMillis;
      nextCall = currentMillis + 200; // Reset nextCall to avoid immediate re-triggering
      
      if (gameMode == armed){
        gameMode = running; // Change mode to running
        countdown.start(); // Start the countdown if it is not running
        counter = 0;       // Reset the motion counter
        Serial.println("Countdown started");
      }

      if (countdown.isRunning())
      {
        counter++; // Increment the motion counter
      }
      else if (gameMode == running)
      {
        gameMode = normal; // If countdown is not running, change mode to armed
      }
      else
      {
        /* code */
      }
      
      notifyClients(getSensorReadings());
    }
  }
  // ENDREGION: MPU6050 Motion Detection


  // REGION: Countdown Handling
  if (nextCall < currentMillis)
  {
    if (countdown.isRunning())
      nextCall = currentMillis + 500; // If countdown is running, check every 200 ms
    else
      nextCall = currentMillis + 1000; // Otherwise, check every second

    // If countdown is not running, change mode to armed
    if (gameMode == running && !countdown.isRunning())
      gameMode = normal;

    // If countdown is running, notify clients with the current sensor readings
    notifyClients(getSensorReadings());
    digitalWrite(LED, !digitalRead(LED)); // Turn on the LED to indicate failure
  }
  // ENDREGION: Countdown Handling

  // Handle WebSocket events
  ws.cleanupClients();
}
