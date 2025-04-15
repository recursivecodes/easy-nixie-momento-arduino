# Easy Nixie Arduino

Arduino firmware for controlling Nixie tube displays using Momento Topics for real-time updates.

## Overview

This project provides Arduino code to drive Nixie tube displays with real-time data from Momento Topics. The Arduino connects to WiFi, authenticates with an API endpoint to get a token, and then subscribes to a Momento topic to receive updates that are displayed on the Nixie tubes.

## Hardware Requirements

- Arduino Uno r4 board with WiFi capability (using WiFiS3 library)
- Nixie tube display modules (EasyNixie)

## Pin Connections

- OUT_EN: Arduino pin 3 to OUT_EN
- SHCP: Arduino pin 2 to SHCP
- STCP: Arduino pin 6 to STCP
- DSIN: Arduino pin 7 to DSIN

## Dependencies

- EasyNixie library
- WiFiS3 library
- ArduinoJson library

## Configuration

Copy the `creds.h.template` file to `creds.h` and update with your credentials:

```cpp
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASS "your-wifi-password"
char tokenEndpoint[] = "your-token-endpoint";
char momentoTopicEndpoint[] = "api.cache.cell-us-east-1-1.prod.a.momentohq.com";
```

Note: The `creds.h` file is included in `.gitignore` to prevent accidentally committing sensitive information.

## Functionality

The code:
1. Connects to WiFi using the provided credentials
2. Obtains an authentication token from the specified endpoint
3. Subscribes to a Momento topic for real-time updates
4. Processes incoming JSON messages and displays the values on Nixie tubes
5. Supports color control for RGB Nixie tubes
6. Uses non-blocking HTTP client implementation for better responsiveness

## Message Format

The expected JSON message format from the Momento topic:

```json
{
  "value": "123456",  // The numeric value to display (zero-padded)
  "color": 1          // Color code for RGB Nixie tubes
}
```

## Usage

1. Install the required libraries in your Arduino IDE
2. Configure your credentials in the `creds.h` file
3. Upload the sketch to your Arduino board
4. The Nixie tubes will display numeric values received via the Momento topic

## Development

The code includes:
- Non-blocking HTTP client implementation
- JSON parsing for Momento topic messages
- LED status indicator (blinks to show the system is running)
- Error handling for network and API issues