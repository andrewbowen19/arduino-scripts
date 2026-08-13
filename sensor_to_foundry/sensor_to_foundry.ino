#include <WiFiS3.h>
#include <time.h>

#include "arduino_secrets.h"
#include "plant_config.h"

// ------------------------------------------------------------
// Foundry configuration
// ------------------------------------------------------------

const char FOUNDRY_HOST[] =
    "andrewssandbox.usw-17.palantirfoundry.com";

const char FOUNDRY_STREAM_RID[] =
    "ri.foundry.main.dataset.d746df15-c23d-41de-908d-a17e9050ee64";

// Publish every 30 seconds.
// Increase this for normal operation if readings do not need to
// be sent this frequently.
const unsigned long PUBLISH_INTERVAL_MS = 30000;

// Maximum time to wait for an HTTP response.
const unsigned long HTTP_TIMEOUT_MS = 15000;

// ------------------------------------------------------------
// Global variables
// ------------------------------------------------------------

WiFiSSLClient client;

unsigned long lastPublishTime = 0;

// ------------------------------------------------------------
// Wi-Fi
// ------------------------------------------------------------

bool connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.print("Connecting to Wi-Fi");

  WiFi.begin(SECRET_SSID, SECRET_PASS);

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (millis() - startTime >= 30000) {
      Serial.println();
      Serial.println("Wi-Fi connection timed out.");
      return false;
    }
  }

  Serial.println();
  Serial.println("Connected to Wi-Fi");

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  return true;
}

// ------------------------------------------------------------
// Timestamp
// ------------------------------------------------------------

String getEpochMilliseconds() {
  // WiFi.getTime() returns seconds since 1970-01-01 UTC.
  unsigned long epochSeconds = WiFi.getTime();

  if (epochSeconds == 0) {
    Serial.println("Unable to get network time.");
    return "";
  }

  /*
   * Foundry expects milliseconds.
   *
   * Appending "000" avoids overflowing a 32-bit unsigned long
   * when multiplying epoch seconds by 1000.
   */
  return String(epochSeconds) + "000";
}


// ------------------------------------------------------------
// JSON helpers
// ------------------------------------------------------------

String jsonEscape(const char *value) {
  String escaped;

  while (*value != '\0') {
    char current = *value;

    switch (current) {
      case '"':
        escaped += "\\\"";
        break;

      case '\\':
        escaped += "\\\\";
        break;

      case '\n':
        escaped += "\\n";
        break;

      case '\r':
        escaped += "\\r";
        break;

      case '\t':
        escaped += "\\t";
        break;

      default:
        escaped += current;
        break;
    }

    value++;
  }

  return escaped;
}

// ------------------------------------------------------------
// Read sensors and build the Foundry request
// ------------------------------------------------------------

bool publishAllPlantsToFoundry() {
  if (!connectToWiFi()) {
    Serial.println("Cannot publish because Wi-Fi is disconnected.");
    return false;
  }

String timestampMilliseconds = getEpochMilliseconds();

if (timestampMilliseconds.length() == 0) {
  Serial.println(
      "Skipping publish because the timestamp is unavailable."
  );
  return false;
}


  String body;

  // Preallocate some memory to reduce String reallocations.
  body.reserve(100 + (PLANT_COUNT * 150));

  body = "{\"records\":[";

  for (size_t i = 0; i < PLANT_COUNT; i++) {
    const PlantConfig &plant = PLANTS[i];

    int moisture = analogRead(plant.analogPin);

    Serial.println("--------------------------------");
    Serial.print("Plant: ");
    Serial.println(plant.plantName);

    Serial.print("Analog pin: ");
    Serial.println(plant.analogPin);

    Serial.print("Soil moisture: ");
    Serial.println(moisture);

    if (moisture > plant.dryThreshold) {
      Serial.println("Status: Soil moisture low");
    } else {
      Serial.println("Status: Soil moisture high");
    }

    // Insert a comma before every record except the first one.
    if (i > 0) {
      body += ",";
    }

    body += "{";

    body += "\"timestamp\":";
    body += timestampMilliseconds;
    body += ",";

    body += "\"water_level\":";
    body += String(moisture);
    body += ",";

    body += "\"plant_name\":\"";
    body += jsonEscape(plant.plantName);
    body += "\",";

    body += "\"plant_id\":\"";
    body += jsonEscape(plant.plantId);
    body += "\"";

    body += "}";
  }

  body += "]}";

  // Construct the endpoint for the single shared stream.
  String foundryPath =
      "/api/v2/highScale/streams/datasets/";

  foundryPath += FOUNDRY_STREAM_RID;
  foundryPath +=
      "/streams/master/publishRecords?preview=true";

  Serial.println("--------------------------------");
  Serial.println("Publishing batch to Foundry");
  Serial.println("Request body:");
  Serial.println(body);

  // Open a TLS connection to Foundry.
  if (!client.connect(FOUNDRY_HOST, 443)) {
    Serial.println("TLS connection to Foundry failed.");
    return false;
  }

  // HTTP request line
  client.print("POST ");
  client.print(foundryPath);
  client.println(" HTTP/1.1");

  // HTTP headers
  client.print("Host: ");
  client.println(FOUNDRY_HOST);

  client.print("Authorization: Bearer ");
  client.println(FOUNDRY_TOKEN);

  client.println("Content-Type: application/json");

  client.print("Content-Length: ");
  client.println(body.length());

  client.println("Connection: close");

  // Blank line separates headers from the request body.
  client.println();

  // HTTP request body
  client.print(body);

  // Wait for Foundry to respond.
  unsigned long responseStart = millis();

  while (!client.available()) {
    if (!client.connected()) {
      Serial.println(
          "Foundry closed the connection without a response."
      );

      client.stop();
      return false;
    }

    if (millis() - responseStart >= HTTP_TIMEOUT_MS) {
      Serial.println("Timed out waiting for Foundry.");
      client.stop();
      return false;
    }

    delay(10);
  }

  // Read the first line, such as:
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

  // Print the remaining response headers and body.
  unsigned long lastResponseData = millis();

  while (
      (client.connected() || client.available()) &&
      millis() - lastResponseData < 10000
  ) {
    while (client.available()) {
      Serial.write(client.read());
      lastResponseData = millis();
    }

    delay(1);
  }

  Serial.println();
  client.stop();

  if (success) {
    Serial.println("Plant readings published successfully.");
  } else {
    Serial.println("Plant readings failed to publish.");
  }

  return success;
}

// ------------------------------------------------------------
// Arduino setup
// ------------------------------------------------------------

void setup() {
  Serial.begin(9600);

  // Configure all plant sensor pins as analog inputs.
  for (size_t i = 0; i < PLANT_COUNT; i++) {
    pinMode(PLANTS[i].analogPin, INPUT);
  }

  connectToWiFi();

  // Send an initial reading after startup.
  publishAllPlantsToFoundry();

  lastPublishTime = millis();
}

// ------------------------------------------------------------
// Arduino main loop
// ------------------------------------------------------------

void loop() {
  if (millis() - lastPublishTime >= PUBLISH_INTERVAL_MS) {
    lastPublishTime = millis();

    publishAllPlantsToFoundry();
  }

  delay(100);
}
