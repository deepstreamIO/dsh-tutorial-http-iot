#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include <ArduinoJson.h>

ESP8266WiFiMulti WiFiMulti;

const int sensorPin = A0;

const int greenLed = D1;
const int redLed = D2;

const int readDelayMs = 10000;

const char* ssid = "<YOUR NETWORK SSID>";
const char* password = "<YOUR NETWORK PASSWORD>";

const char* deepstreamHubHttpUrl = "<YOUR HTTP URL>";
/*
 * Generated TLS fingerprints:
 *
 * 013.deepstreamhub.com: "3A:FC:6E:78:94:18:C0:A2:36:F3:C7:DF:86:27:4B:5A:CA:CF:28:3F"
 * 035.deepstreamhub.com: "57:18:5A:22:07:94:03:EF:90:C9:C2:56:58:C9:BB:06:66:A6:EA:76"
 * 154.deepstreamhub.com: "3C:65:CA:7C:3F:43:2D:FF:A1:63:38:F3:23:D5:59:25:E4:85:8C:0F"
 */
const char* deepstreamHubTlsFingerprint = "<YOUR HTTP DOMAIN FINGERPRINT>";

void setup() {
    Serial.begin(115200);

    // connect to WiFi
    WiFiMulti.addAP(ssid, password);

    // initialize sensor
    pinMode(sensorPin, INPUT);

    // initialize LEDs
    pinMode(redLed, OUTPUT);
    pinMode(greenLed, OUTPUT);
    digitalWrite(redLed, LOW);
    digitalWrite(greenLed, LOW);
}

void loop() {
    if (WiFiMulti.run() != WL_CONNECTED) {
      delay(200);
      return;
    }

    int level = analogRead(sensorPin);
    Serial.printf("Light level: %d\n", level);

    updateRecord(level);

    delay(readDelayMs);
}

void updateRecord(int level) {
    HTTPClient http;

    // configure client
    http.begin(deepstreamHubHttpUrl, deepstreamHubTlsFingerprint);

    // set content type
    http.addHeader("Content-Type", "application/json");

    // create message body
    StaticJsonBuffer<200> bodyBuff;
    JsonObject& root = bodyBuff.createObject();
    JsonArray& body = root.createNestedArray("body");
    JsonObject& message = body.createNestedObject();
    message["topic"] = "record";
    message["action"] = "write";
    message["recordName"] = "readings/light-level";
    message["path"] = "value";
    message["data"] = level;

    // copy object into array
    size_t bodySize = bodyBuff.size();
    char requestBody[bodySize];
    root.printTo(requestBody, bodySize);

    // make request
    int httpCode = http.POST(requestBody);

    // httpCode will be negative on error
    if(httpCode == HTTP_CODE_OK) {
        // parse response
        String payload = http.getString();
        StaticJsonBuffer<200> respBuff;
        JsonObject& resp = respBuff.parseObject(payload);
        if (!resp.success()) {
            // failed to parse JSON response
            Serial.printf("Failed to parse response: %s\n", payload.c_str());
            flashLed(redLed);
            return;
        }
        if (!resp["body"][0]["success"]) {
            // failed to update record
            Serial.printf("Record update error: %s\n", resp["body"][0]["error"]);
            flashLed(redLed);
            return;
        }
        // record update success
        Serial.println("Record was updated successfully!");
        flashLed(greenLed);
    } else if (httpCode < 0) {
        Serial.printf("Request failed, error: %s\n", http.errorToString(httpCode).c_str());
        flashLed(redLed);
    } else {
        Serial.printf("Error response %d: %s\n", httpCode, http.getString().c_str());
        flashLed(redLed);
    }

    http.end();
}

void flashLed(int led) {
    digitalWrite(led, HIGH);
    delay(500);
    digitalWrite(led, LOW);
}
