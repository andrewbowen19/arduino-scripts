#include <WiFiS3.h>
#include <time.h>
#include "arduino_secrets.h"

const char foundryHost[] =
    "andrewssandbox.usw-17.palantirfoundry.com";

const char foundryPath[] =
    "/api/v2/highScale/streams/datasets/"
    "ri.foundry.main.dataset.6a46b15b-a807-4115-a890-dcefde328b97"
    "/streams/master/publishRecords?preview=true";

const int SOIL_SENSOR_PIN = A0;
const int OUTPUT_PIN = 12;
const int MOISTURE_THRESHOLD = 850;

// Avoid publishing every loop unless that is intentional.
const unsigned long PUBLISH_INTERVAL_MS = 10000;

WiFiSSLClient client;

unsigned long lastPublishTime = 0;

void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(SECRET_SSID, SECRET_PASS);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (millis() - startTime > 30000) {
      Serial.println("\nWi-Fi connection timed out.");
      return;
    }
  }

  Serial.println();
  Serial.println("Connected to Wi-Fi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

/*
 * Gets the current time from the WiFi module and formats it as:
 * 2026-08-11T15:39:15Z
 */
String getIsoTimestamp() {
  unsigned long epoch = WiFi.getTime();

  if (epoch == 0) {
    Serial.println("Unable to get network time.");
    return "";
  }

  time_t rawTime = (time_t)epoch;
  struct tm *utcTime = gmtime(&rawTime);

  if (utcTime == nullptr) {
    return "";
  }

  char timestamp[25];

  strftime(
      timestamp,
      sizeof(timestamp),
      "%Y-%m-%dT%H:%M:%SZ",
      utcTime
  );

  return String(timestamp);
}

bool publishToFoundry(int moisture, const char *plantName) {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();

    if (WiFi.status() != WL_CONNECTED) {
      return false;
    }
  }

  String timestamp = getIsoTimestamp();

  if (timestamp.length() == 0) {
    Serial.println("Skipping publish because timestamp is unavailable.");
    return false;
  }

  String body =
      "{\"records\":[{"
      "\"timestamp\":\"" + timestamp + "\","
      "\"soil_moisture\":" + String(moisture) + ","
      "\"plant_name\":\"" + String(plantName) + "\""
      "}]}";

  Serial.println("Connecting to Foundry...");

  if (!client.connect(foundryHost, 443)) {
    Serial.println("TLS connection to Foundry failed.");
    return false;
  }

  // Equivalent to curl -X POST
  client.print("POST ");
  client.print(foundryPath);
  client.println(" HTTP/1.1");

  // Equivalent to the curl headers
  client.print("Host: ");
  client.println(foundryHost);

  client.print("Authorization: Bearer ");
  client.println(FOUNDRY_TOKEN);

  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println("Connection: close");

  // Blank line separates headers from the request body.
  client.println();

  // Equivalent to curl -d
  client.print(body);

  Serial.println("Request body:");
  Serial.println(body);

  // Wait up to 15 seconds for a response.
  unsigned long responseStart = millis();

  while (!client.available()) {
    if (!client.connected()) {
      Serial.println("Foundry closed the connection without a response.");
      client.stop();
      return false;
    }

    if (millis() - responseStart > 15000) {
      Serial.println("Timed out waiting for Foundry.");
      client.stop();
      return false;
    }

    delay(10);
  }

  // The first response line looks like:
  // HTTP/1.1 200 OK
  String statusLine = client.readStringUntil('\n');
  statusLine.trim();

  Serial.print("Foundry response: ");
  Serial.println(statusLine);

  bool success =
      statusLine.indexOf(" 200 ") >= 0 ||
      statusLine.indexOf(" 201 ") >= 0 ||
      statusLine.indexOf(" 202 ") >= 0 ||
      statusLine.indexOf(" 204 ") >= 0;

  // Print the remaining headers and response body for debugging.
  while (client.connected() || client.available()) {
    while (client.available()) {
      Serial.write(client.read());
    }
  }

  Serial.println();
  client.stop();

  return success;
}

void setup() {
  Serial.begin(115200);

  // The sensor's analog output must be an input.
  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(OUTPUT_PIN, OUTPUT);

  connectToWiFi();
}

void loop() {
  int moisture = analogRead(SOIL_SENSOR_PIN);

  Serial.print("Soil moisture: ");
  Serial.println(moisture);

  if (moisture > MOISTURE_THRESHOLD) {
    digitalWrite(OUTPUT_PIN, LOW);
    Serial.println("Soil moisture low");
  } else {
    digitalWrite(OUTPUT_PIN, HIGH);
    Serial.println("Soil moisture high");
  }

  if (millis() - lastPublishTime >= PUBLISH_INTERVAL_MS) {
    lastPublishTime = millis();

    bool published = publishToFoundry(
        moisture,
        "value"  // Replace with your plant name.
    );

    if (published) {
      Serial.println("Record published successfully.");
    } else {
      Serial.println("Record publish failed.");
    }
  }

  delay(2000);
}
