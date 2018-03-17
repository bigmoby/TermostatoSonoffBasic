/* Create a WiFi access point and provide a web server on it
 * in order to serve relay command on ESP8266 (or Sonoff Basic relay).
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>

/* Set these to your desired credentials. */
const char* ssid = "";
const char* password = "";
const char* deviceName = "TermostatoCasa";

IPAddress ip(192, 168, 1, 222);
IPAddress gateway_ip(192, 168, 1, 1);
IPAddress subnet_mask(255, 255, 255, 0);
IPAddress dns(192, 168, 1, 1);

uint8_t relayState = LOW;  // HIGH: closed switch
uint8_t buttonState = LOW;
uint8_t currentButtonState = buttonState;

long buttonStartPressed = 0;
long buttonDurationPressed = 0;

// Sonoff properties
const uint8_t BUTTON_PIN = 0;
const uint8_t RELAY_PIN  = 12;
const uint8_t LED_PIN    = 13;

enum CMD {
  CMD_NOT_DEFINED,
  CMD_BUTTON_STATE_CHANGED,
};
volatile uint8_t cmd = CMD_NOT_DEFINED;

ESP8266WebServer server(80);

void setup_wifi() {
  WiFi.config(ip, gateway_ip, subnet_mask, dns);

  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  delay(500);
}

///////////////////////////////////////////////////////////////////////////
//   ISR
///////////////////////////////////////////////////////////////////////////
/*
  Function called when the button is pressed/released
*/
void buttonStateChangedISR() {
  cmd = CMD_BUTTON_STATE_CHANGED;
}

///////////////////////////////////////////////////////////////////////////
//   Sonoff switch
///////////////////////////////////////////////////////////////////////////
/*
  Function called to set the state of the relay
*/
void setRelayState() {
  digitalWrite(RELAY_PIN, relayState);
  digitalWrite(LED_PIN, (relayState + 1) % 2);
}

void changeState() {
  relayState = relayState == HIGH ? LOW : HIGH;
  setRelayState();

  String message = "";
  message += relayState;
  server.send(200, "text/html", message);
}

void setON() {
  relayState = HIGH;
  setRelayState();

  String message = "";
  message += relayState;
  server.send(200, "text/html", message);
}

void setOFF() {
  relayState = LOW;
  setRelayState();

  String message = "";
  message += relayState;
  server.send(200, "text/html", message);
}

void OffThenOnSwitch() {
  relayState = LOW;
  setRelayState();

  delay(3000);

  relayState = HIGH;
  setRelayState();

  String message = "";
  message += relayState;
  server.send(200, "text/html", message);
}

void temporarySwitch() {
  relayState = relayState == HIGH ? LOW : HIGH;
  setRelayState();

  delay(3000);

  relayState = relayState == HIGH ? LOW : HIGH;
  setRelayState();

  String message = "";
  message += relayState;
  server.send(200, "text/html", message);
}

void getStatus() {
  String message = "";
  message += relayState;
  server.send(200, "text/html", message);
}

void getDeviceName() {
  server.send(200, "text/html", deviceName);
}

void handlePost() {
  if (server.hasArg("plain") == false) { //Check if body received
    server.send(200, "text/plain", "Body not received");
    return;
  }

  String data = server.arg("plain");
  StaticJsonBuffer<200> jBuffer;
  JsonObject& jObject = jBuffer.parseObject(data);
  String deviceArg = jObject["device"];
  String statusArg = jObject["status"];

  Serial.println("Received args [" + deviceArg + "] [" + statusArg + "]");

  if (String(deviceName).equalsIgnoreCase(deviceArg)) {
    if (String("ON").equalsIgnoreCase(statusArg)) {
      relayState = HIGH;
      setRelayState();
    } else if (String("OFF").equalsIgnoreCase(statusArg)) {
      relayState = LOW;
      setRelayState();
    }
  }

  String message = "Device status:\n";
  message += relayState;
  server.send(200, "text/html", message);

  Serial.println(message);
}

void setup() {
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  // init the I/O
  pinMode(LED_PIN,    OUTPUT);
  pinMode(RELAY_PIN,  OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  attachInterrupt(BUTTON_PIN, buttonStateChangedISR, CHANGE);

  setup_wifi();

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  ArduinoOTA.setHostname(deviceName);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", getDeviceName);
  server.on("/switch", changeState);
  server.on("/temporarySwitch", temporarySwitch);
  server.on("/deviceName", getDeviceName);
  server.on("/offThenOnSwitch", OffThenOnSwitch);
  server.on("/state", getStatus);
  server.on("/ON", setON);
  server.on("/OFF", setOFF);
  server.on("/status", HTTP_POST, handlePost);
  server.begin();
  Serial.println("HTTP server started");
  delay(2000);

  setOFF();
}

void loop() {
  ArduinoOTA.handle();

  yield();

  switch (cmd) {
    case CMD_NOT_DEFINED:
      // do nothing
      server.handleClient();
      break;
    case CMD_BUTTON_STATE_CHANGED:
      currentButtonState = digitalRead(BUTTON_PIN);
      if (buttonState != currentButtonState) {
        // tests if the button is released or pressed
        if (buttonState == LOW && currentButtonState == HIGH) {
          buttonDurationPressed = millis() - buttonStartPressed;
          if (buttonDurationPressed < 500) {
            relayState = relayState == HIGH ? LOW : HIGH;
            setRelayState();
          }
          else if (buttonDurationPressed > 3000) {
            delay(1000);
            ESP.restart();
          }
        } else {
          buttonStartPressed = millis();
        }
        buttonState = currentButtonState;
      }
      cmd = CMD_NOT_DEFINED;
      server.handleClient();
      break;
  }

  yield();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  server.handleClient();

  yield();
}
