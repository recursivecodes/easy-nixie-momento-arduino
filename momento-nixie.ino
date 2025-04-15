#include "EasyNixie.h"
#include <WiFiS3.h>
#include "WiFiSSLClient.h"
#include "IPAddress.h"
#include "creds.h"
#include <ArduinoJson.h>

#define OUT_EN 3   //connect Pin ArduinoUNO 3 to OUT_EN
#define SHCP   2   //connect Pin ArduinoUNO 2 to SHCP
#define STCP   6   //connect Pin ArduinoUNO 6 to STCP
#define DSIN   7   //connect Pin ArduinoUNO 7 to DSIN

WiFiSSLClient client;
EasyNixie en(OUT_EN, SHCP, STCP, DSIN); 
int status = WL_IDLE_STATUS;    
String token;  
String topicResult;
bool isSubscribed = false;

// HTTP Client State Machine
enum HttpClientState {
  HTTP_IDLE,
  HTTP_CONNECTING,
  HTTP_SENDING_HEADERS,
  HTTP_WAITING_RESPONSE,
  HTTP_READING_HEADERS,
  HTTP_READING_BODY,
  HTTP_COMPLETE,
  HTTP_ERROR
};

HttpClientState httpState = HTTP_IDLE;
unsigned long lastHttpActivity = 0;
const unsigned long HTTP_TIMEOUT = 60000; // 60 seconds timeout
String currentResponse = "";
String* resultPtr = NULL;
const char* currentServer = NULL;
bool currentKeepAlive = false;

void startRequest(const char* server, String path, String* result, const char* authToken = NULL, bool keepAlive = false) {
  // Reset state
  httpState = HTTP_CONNECTING;
  currentResponse = "";
  resultPtr = result;
  lastHttpActivity = millis();
  currentServer = server;
  currentKeepAlive = keepAlive;
  
  // Start connection
  if (client.connect(server, 443)) {
    Serial.println(String("Connected to server: ") + server);
    httpState = HTTP_SENDING_HEADERS;
    
    // Send headers
    client.println("GET " + path + " HTTP/1.1");
    client.println(String("Host: ") + server);
    client.println("Content-Type: application/json");
    if (authToken != NULL) {
      client.println(String("Authorization: ") + authToken);
    }
    if(!keepAlive) {
      client.println("Connection: close");
    }
    client.println();
    
    httpState = HTTP_WAITING_RESPONSE;
  } else {
    Serial.println("Connection failed");
    httpState = HTTP_ERROR;
  }
}

void processHttp() {
  if (httpState == HTTP_IDLE) {
    return; 
  }
  
  // Check for timeout
  if (millis() - lastHttpActivity > HTTP_TIMEOUT) {
    Serial.println("HTTP request timed out");
    httpState = HTTP_ERROR;
    if (client.connected()) client.stop();
    return;
  }
  
  switch (httpState) {
    case HTTP_WAITING_RESPONSE:
      if (client.available()) {
        lastHttpActivity = millis();
        httpState = HTTP_READING_HEADERS;
      }
      break;
      
    case HTTP_READING_HEADERS:
      if (client.available()) {
        lastHttpActivity = millis();
        String line = client.readStringUntil('\n');
        if (line == "\r") {
          httpState = HTTP_READING_BODY;
        }
      }
      break;
      
    case HTTP_READING_BODY:
      if (client.available()) {
        lastHttpActivity = millis();
        char c = client.read();
        currentResponse += c;
      } else {
        // No more data and connection closed
        if (resultPtr != NULL) {
          *resultPtr = currentResponse;
        }
        httpState = HTTP_COMPLETE;
        Serial.println("HTTP request complete");
      }
      break;
      
    case HTTP_COMPLETE:
    case HTTP_ERROR:
      // Don't reset state here - let the calling code do it
      break;
  }
}

bool isRequestComplete() {
  return (httpState == HTTP_COMPLETE || httpState == HTTP_ERROR);
}

bool wasRequestSuccessful() {
  return (httpState == HTTP_COMPLETE);
}

void resetHttpState() {
  httpState = HTTP_IDLE;
}

// blocking request (used to retrieve token)
void makeRequest(const char* server, String path, String* result, const char* authToken = NULL, bool keepAlive = false) {
  startRequest(server, path, result, authToken, keepAlive);
  unsigned long startTime = millis();
  while (!isRequestComplete() && (millis() - startTime < HTTP_TIMEOUT)) {
    processHttp();
  }
  if (!wasRequestSuccessful()) {
    Serial.println("Request failed");
  }
  if (!keepAlive && client.connected()) {
    client.stop();
  }
  resetHttpState();
}

void setup() {
  Serial.begin(115200);
  while (status != WL_CONNECTED) {
    Serial.print("Connecting to WiFi network: ");
    Serial.println(WIFI_SSID);
    status = WiFi.begin(WIFI_SSID, WIFI_PASS);
    delay(100);
  }
  Serial.println("WiFi connected.");
  Serial.println(WiFi.localIP());
  Serial.println("Getting auth token...");
  StaticJsonDocument<550> tokenJson;
  String tokenResult;
  
  // Use blocking request for initial token
  makeRequest(tokenEndpoint, "/prod", &tokenResult);
  deserializeJson(tokenJson, tokenResult);
  token = String((const char*)tokenJson["authToken"]);
  // Serial.println(token);
  // Ensure HTTP state is reset to idle after setup
  resetHttpState();
  pinMode(LED_BUILTIN, OUTPUT);
}

// Non-blocking LED blink variables
unsigned long lastBlinkTime = 0;
const unsigned long BLINK_INTERVAL = 500;

void loop() {
  // Debug HTTP state (uncomment for debugging)
  // static HttpClientState lastState = HTTP_IDLE;
  // if (lastState != httpState) {
  //   Serial.print("HTTP state changed to: ");
  //   Serial.println(httpState);
  //   lastState = httpState;
  // }
  
  // Process any ongoing HTTP requests
  processHttp();
  
  // Start a new subscription if needed
  if (token.length() > 0 && !isSubscribed && httpState == HTTP_IDLE) {
    Serial.println("Subscribing to topic...");
    startRequest(
      momentoTopicEndpoint,
      "/topics/demo-cache/arduino-topic",
      &topicResult,
      token.c_str(), 
      true
    );
    isSubscribed = true;
  }

  // Handle incoming messages
  if (isSubscribed && isRequestComplete()) {
    if (wasRequestSuccessful() && topicResult.length() > 0) {
      Serial.println("Notification received...");
      Serial.println(topicResult);
      JsonDocument itemsDoc;
      deserializeJson(itemsDoc, topicResult);
      JsonObject item = itemsDoc["items"][itemsDoc["items"].size()-1]["item"];
      const char* message = item["value"]["text"];
      Serial.println(message);
      // the item itself is serialized as JSON, 
      // so deserialize it too
      JsonDocument messageDoc;
      deserializeJson(messageDoc, message);
      const char* value = messageDoc["value"];
      int color = messageDoc["color"];
      // the incoming value is zero padded to 
      // six digits and reversed (since we need to 
      // set the nixies from last -> first)
      // so loop over the value (which is a string
      // because zero padding), convert each char
      // to an int, and set the tube
      for (int i = 0; value[i] != '\0'; i++) {
        // Convert the character to an integer by subtracting ASCII value of '0'
        int digit = value[i] - '0';
        en.SetNixie(digit, color, true, true, 0);
        en.Latch();
      }
      topicResult = "";
    }
    // Reset for next request
    if (client.connected()) client.stop();
    isSubscribed = false;
    resetHttpState();  
    Serial.println("Ready for next subscription");
  }
  
  // Non-blocking LED blink
  unsigned long currentTime = millis();
  if (currentTime - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = currentTime;
    static bool ledState = false;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
  }
}
