#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include <ArduinoJson.h>

ESP8266WiFiMulti WiFiMulti;

const int btn0 = D0;
const int btn1 = D1;
const int btn2 = D2;
const int btn3 = D3;

bool btnState0, btnState1, btnState2, btnState3;

const int readDelayMs = 100;

const char* ssid = "<YOUR NETWORK SSID>";
const char* password = "<YOUR NETWORK PASSWORD>";

const char* deepstreamHubHttpUrl = "<YOUR HTTP URL>";

StaticJsonBuffer<400> jsonBuffer;
/*
 * Generated TLS fingerprints:
 *
 * 013.deepstreamhub.com: "3A:FC:6E:78:94:18:C0:A2:36:F3:C7:DF:86:27:4B:5A:CA:CF:28:3F"
 * 035.deepstreamhub.com: "57:18:5A:22:07:94:03:EF:90:C9:C2:56:58:C9:BB:06:66:A6:EA:76"
 * 154.deepstreamhub.com: "3C:65:CA:7C:3F:43:2D:FF:A1:63:38:F3:23:D5:59:25:E4:85:8C:0F"
 */
const char* deepstreamHubTlsFingerprint = "<YOUR HTTP DOMAIN FINGERPRINT>";

// the possible record actions
enum class RecordAction {
    Read,
    Write,
    Head
};

void setup() {
    Serial.begin(115200);

    // connect to WiFi
    WiFiMulti.addAP(ssid, password);

    // initialize buttons
    pinMode(btn0, INPUT);
    pinMode(btn1, INPUT);
    pinMode(btn2, INPUT);
    pinMode(btn3, INPUT);

    // initial button state
    btnState0 = btnState1 = btnState2 = btnState3 = false;
}

void loop() {
    if (WiFiMulti.run() != WL_CONNECTED) {
        delay(200);
        Serial.print(".");
        return;
    }
    
    checkBtn(btn0, btnState0, "inventory/cola");
    checkBtn(btn1, btnState1, "inventory/beer");
    checkBtn(btn2, btnState2, "inventory/water");
    checkBtn(btn3, btnState3, "inventory/apfelschorle");

    delay(readDelayMs);
}

void checkBtn(const int btn, bool &btnState, char *recordName) {
    if (digitalRead(btn) == LOW && !btnState) {
        // button pressed
        btnState = true;
    } else if (digitalRead(btn) == HIGH && btnState) {
        // button released
        btnState = false;
        decRecord(recordName);
    }
}

JsonVariant recordRequest(RecordAction action, char *recordName, char *path = nullptr, JsonVariant data = {}) {
    HTTPClient http;

    // configure client
    http.begin(deepstreamHubHttpUrl, deepstreamHubTlsFingerprint);

    // set content type
    http.addHeader("Content-Type", "application/json");

    // create message body
    jsonBuffer.clear();
    JsonObject& root = jsonBuffer.createObject();
    JsonArray& body = root.createNestedArray("body");
    JsonObject& message = body.createNestedObject();
    message["topic"] = "record";
    
    switch (action) {
        case RecordAction::Read:
            message["action"] = "read";
            break;
        case RecordAction::Head:
            message["action"] = "head";
            break;
        case RecordAction::Write:
            message["action"] = "write";
            if (path != nullptr) {
                message["path"] = "amount";
            }
            message["data"] = data;
            break;
        default:
            Serial.printf("Unknown record action %d\n", static_cast<int>(action));
    }
    
    message["recordName"] = recordName;

    // copy object into array
    size_t requestBodySize = root.measureLength() + 1;
    char requestBody[requestBodySize];
    root.printTo(requestBody, requestBodySize);

    //print request
    Serial.print("request: ");
    root.printTo(Serial);
    Serial.println();

    // make request
    int httpCode = http.POST(requestBody);
    
    JsonVariant result = (char *) nullptr;
    if(httpCode == HTTP_CODE_OK) {
        // parse response
        String payload = http.getString();
        jsonBuffer.clear();
        JsonObject& resp = jsonBuffer.parseObject(payload);
        if (!resp.success()) {
            // failed to parse JSON response
            Serial.printf("Failed to parse response: %s\n", payload.c_str());
        } else if (!resp["body"][0]["success"]) {
            // failed to update record
            Serial.printf("Record update error: %s\n", resp["body"][0]["error"]);
        } else {
            // record update success
            Serial.println("Record request success");
            result = resp["body"][0]["data"];
        }
    } else if (httpCode < 0) {
        Serial.printf("Request failed, error: %s\n", http.errorToString(httpCode).c_str());
    } else {
        Serial.printf("Error response %d: %s\n", httpCode, http.getString().c_str());
    }

    http.end();
    return result;
}

void decRecord(char *recordName) {
    JsonVariant recordData = recordRequest(RecordAction::Read, recordName);
    if (!recordData.is<JsonObject>()) {
        Serial.println("Decrement aborted");
        return;
    }
    int amount = recordData["amount"];
    Serial.printf("Record read: %d\n", amount);
    amount--;
    recordRequest(RecordAction::Write, recordName, "amount", amount);
}
