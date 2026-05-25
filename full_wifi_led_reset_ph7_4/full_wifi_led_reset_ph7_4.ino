#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// LCD I2C
// =====================================================
// thử 0x27 trước
// nếu không lên đổi thành 0x3F
LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// PIN
// =====================================================
#define PH_PIN        34
#define CAL_BTN_PIN   27
#define WIFI_BTN_PIN  0
#define BUZZER_PIN    25
#define LED_PIN       2

// =====================================================
// BUFFER
// =====================================================
#define BUFFER_SIZE 100

// =====================================================
// WIFI AP
// =====================================================
const char* AP_SSID = "ESP32-Setup";
const char* AP_PASS = "12345678";

// =====================================================
// SERVER URL
// =====================================================
const char* serverURL =
    "https://httpbin.org/post";

// =====================================================
// OBJECTS
// =====================================================
Preferences phPrefs;
Preferences wifiPrefs;

WebServer server(80);

// =====================================================
// PH BUFFER
// =====================================================
int samples[BUFFER_SIZE];

int sampleIndex = 0;

unsigned long lastSampleTime = 0;

// =====================================================
// PH VALUES
// =====================================================
float voltage = 0;
float phValue = 0;

// =====================================================
// CALIBRATION
// =====================================================
float voltage7 = 2.50;
float voltage4 = 3.00;

float slope = 0;
float offset = 0;

// =====================================================
// CAL BUTTON
// =====================================================
bool calButtonPressed = false;

unsigned long calButtonTime = 0;

// =====================================================
// WIFI
// =====================================================
String savedSSID = "";
String savedPASS = "";

bool wifiConnected = false;

// =====================================================
// LED
// =====================================================
unsigned long ledMillis = 0;

bool ledState = false;

// =====================================================
// WIFI BUTTON
// =====================================================
bool wifiBtnHolding = false;

unsigned long wifiBtnTime = 0;

// =====================================================
// SEND TIMER
// =====================================================
unsigned long lastSend = 0;

// =====================================================
// SETUP
// =====================================================
void setup()
{
    Serial.begin(115200);

    // =========================
    // PIN MODE
    // =========================
    pinMode(PH_PIN, INPUT);

    pinMode(CAL_BTN_PIN, INPUT_PULLUP);

    pinMode(WIFI_BTN_PIN, INPUT_PULLUP);

    pinMode(BUZZER_PIN, OUTPUT);

    pinMode(LED_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, LOW);

    // =========================
    // LCD
    // =========================
    Wire.begin(21, 22);

    lcd.init();

    lcd.backlight();

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("PH Meter");

    lcd.setCursor(0, 1);
    lcd.print("Starting...");

    // =========================
    // LOAD PH CALIBRATION
    // =========================
    phPrefs.begin("phcal", false);

    voltage7 =
        phPrefs.getFloat("v7", 2.50);

    voltage4 =
        phPrefs.getFloat("v4", 3.00);

    Serial.println("=== LOAD CALIBRATION ===");

    Serial.print("V7: ");
    Serial.println(voltage7);

    Serial.print("V4: ");
    Serial.println(voltage4);

    calculateCalibration();

    // =========================
    // INIT BUFFER
    // =========================
    for (int i = 0; i < BUFFER_SIZE; i++)
    {
        samples[i] = analogRead(PH_PIN);
    }

    // =========================
    // WIFI
    // =========================
    loadWiFi();

    if (
        savedSSID != "" &&
        connectWiFi(savedSSID, savedPASS))
    {
        server.on("/", handleRoot);

        server.begin();

        Serial.println("WebServer started");
    }
    else
    {
        startAP();
    }

    beep(1);

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("System Ready");

    delay(1000);
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
    readPH();

    calculatePH();

    handleCalButton();

    handleWiFiButton();

    handleLED();

    updateLCD();

    server.handleClient();

    // =========================
    // SERIAL DEBUG
    // =========================
    static unsigned long lastPrint = 0;

    if (millis() - lastPrint > 1000)
    {
        lastPrint = millis();

        Serial.print("Voltage: ");

        Serial.print(voltage, 3);

        Serial.print(" | pH: ");

        Serial.println(phValue, 2);
    }

    // =========================
    // SEND DATA
    // =========================
    if (millis() - lastSend > 3000)
    {
        lastSend = millis();

        sendData();
    }
}

// =====================================================
// READ PH
// =====================================================
void readPH()
{
    if (millis() - lastSampleTime >= 50)
    {
        lastSampleTime = millis();

        samples[sampleIndex] =
            analogRead(PH_PIN);

        sampleIndex =
            (sampleIndex + 1) % BUFFER_SIZE;

        long total = 0;

        for (int i = 0; i < BUFFER_SIZE; i++)
        {
            total += samples[i];
        }

        float adcAvg =
            total / (float)BUFFER_SIZE;

        voltage =
            adcAvg * (3.3 / 4095.0);
    }
}

// =====================================================
// CALCULATE PH
// =====================================================
void calculatePH()
{
    phValue =
        slope * voltage + offset;
}

// =====================================================
// CALCULATE CALIBRATION
// =====================================================
void calculateCalibration()
{
    slope =
        (4.01 - 7.01) /
        (voltage4 - voltage7);

    offset =
        7.01 - slope * voltage7;

    Serial.println("=== CALIBRATION UPDATED ===");

    Serial.print("Slope: ");

    Serial.println(slope);

    Serial.print("Offset: ");

    Serial.println(offset);
}

// =====================================================
// HANDLE CAL BUTTON
// =====================================================
void handleCalButton()
{
    bool state =
        (digitalRead(CAL_BTN_PIN) == LOW);

    if (state && !calButtonPressed)
    {
        calButtonPressed = true;

        calButtonTime = millis();
    }

    if (!state && calButtonPressed)
    {
        calButtonPressed = false;

        unsigned long hold =
            millis() - calButtonTime;

        // =========================
        // CAL PH4
        // HOLD 8s
        // =========================
        if (hold >= 8000)
        {
            voltage4 = voltage;

            phPrefs.putFloat(
                "v4",
                voltage4);

            calculateCalibration();

            Serial.println("CAL PH4 SAVED");

            lcd.clear();

            lcd.setCursor(0, 0);
            lcd.print("CAL PH4 DONE");

            beep(2);

            delay(1500);
        }

        // =========================
        // CAL PH7
        // HOLD 3s
        // =========================
        else if (hold >= 3000)
        {
            voltage7 = voltage;

            phPrefs.putFloat(
                "v7",
                voltage7);

            calculateCalibration();

            Serial.println("CAL PH7 SAVED");

            lcd.clear();

            lcd.setCursor(0, 0);
            lcd.print("CAL PH7 DONE");

            beep(1);

            delay(1500);
        }
    }
}

// =====================================================
// LCD UPDATE
// =====================================================
void updateLCD()
{
    static unsigned long lastLCD = 0;

    if (millis() - lastLCD < 1000)
        return;

    lastLCD = millis();

    //lcd.clear();

    // =========================
    // LINE 1
    // =========================
    lcd.setCursor(0, 0);

    lcd.print("pH:");

    lcd.print(phValue, 2);

    lcd.print("      "); // xoá ký tự dư

    // =========================
    // WIFI STATUS
    // =========================
    lcd.setCursor(10, 0);

    if (wifiConnected)
    {
        lcd.print("WiFi ");
    }
    else
    {
        lcd.print("AP   ");
    }

    // =========================
    // LINE 2
    // =========================
    lcd.setCursor(0, 1);

    lcd.print("V:");

    lcd.print(voltage, 2);

    lcd.print(" ");

    lcd.setCursor(9, 1);

    if (WiFi.status() == WL_CONNECTED)
    {
        lcd.print("SEND");
    }
    else
    {
        lcd.print("WAIT");
    }
}

// =====================================================
// BUZZER
// =====================================================
void beep(int times)
{
    for (int i = 0; i < times; i++)
    {
        digitalWrite(BUZZER_PIN, HIGH);

        delay(120);

        digitalWrite(BUZZER_PIN, LOW);

        delay(120);
    }
}

// =====================================================
// LOAD WIFI
// =====================================================
void loadWiFi()
{
    wifiPrefs.begin("wifi", true);

    savedSSID =
        wifiPrefs.getString("ssid", "");

    savedPASS =
        wifiPrefs.getString("pass", "");

    wifiPrefs.end();

    Serial.println("=== LOAD WIFI ===");

    Serial.print("SSID: ");

    Serial.println(savedSSID);
}

// =====================================================
// SAVE WIFI
// =====================================================
void saveWiFi(
    String ssid,
    String pass)
{
    wifiPrefs.begin("wifi", false);

    wifiPrefs.putString("ssid", ssid);

    wifiPrefs.putString("pass", pass);

    wifiPrefs.end();

    Serial.println("WiFi Saved");
}

// =====================================================
// CONNECT WIFI
// =====================================================
bool connectWiFi(
    String ssid,
    String pass)
{
    Serial.println("");

    Serial.print("Connecting: ");

    Serial.println(ssid);

    WiFi.mode(WIFI_STA);

    WiFi.begin(
        ssid.c_str(),
        pass.c_str());

    int timeout = 0;

    while (
        WiFi.status() != WL_CONNECTED &&
        timeout < 20)
    {
        delay(500);

        Serial.print(".");

        timeout++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("");

        Serial.println("CONNECTED!");

        Serial.print("IP: ");

        Serial.println(WiFi.localIP());

        wifiConnected = true;

        return true;
    }

    Serial.println("");

    Serial.println("FAILED");

    wifiConnected = false;

    return false;
}

// =====================================================
// RESET WIFI
// =====================================================
void resetWiFi()
{
    lcd.clear();
    

    lcd.setCursor(0, 0);

    lcd.print("RESET WIFI");

    Serial.println("RESET WIFI");

    wifiPrefs.begin("wifi", false);

    wifiPrefs.clear();

    wifiPrefs.end();

    delay(1000);

    ESP.restart();
}

// =====================================================
// HANDLE WIFI BUTTON
// =====================================================
void handleWiFiButton()
{
    if (digitalRead(WIFI_BTN_PIN) == LOW)
    {
        if (!wifiBtnHolding)
        {
            wifiBtnHolding = true;

            wifiBtnTime = millis();
        }
        else
        {
            if (
                millis() - wifiBtnTime >= 5000)
            {
                resetWiFi();
            }
        }
    }
    else
    {
        wifiBtnHolding = false;
    }
}

// =====================================================
// LED STATUS
// =====================================================
void handleLED()
{
    unsigned long interval;

    if (wifiConnected)
    {
        interval = 1000;
    }
    else
    {
        interval = 200;
    }

    if (millis() - ledMillis >= interval)
    {
        ledMillis = millis();

        ledState = !ledState;

        digitalWrite(
            LED_PIN,
            ledState);
    }
}

// =====================================================
// WIFI SCAN
// =====================================================
String wifiOptions = "";

void scanWiFi()
{
    Serial.println("Scanning WiFi...");

    wifiOptions = "";

    int n = WiFi.scanNetworks();

    for (int i = 0; i < n; i++)
    {
        wifiOptions += "<option value='";

        wifiOptions += WiFi.SSID(i);

        wifiOptions += "'>";

        wifiOptions += WiFi.SSID(i);

        wifiOptions += " (";

        wifiOptions += String(WiFi.RSSI(i));

        wifiOptions += " dBm)";

        wifiOptions += "</option>";
    }

    WiFi.scanDelete();

    Serial.println("Scan done");
}

// =====================================================
// HTML PAGE
// =====================================================
String htmlPage()
{
    String html = R"rawliteral(

    <!DOCTYPE html>
    <html>

    <head>

    <meta charset="utf-8">

    <title>ESP32 Setup</title>

    <style>

    body{
        font-family:Arial;
        margin:20px;
    }

    input,select,button{
        width:100%;
        padding:12px;
        margin-top:10px;
    }

    </style>

    </head>

    <body>

    <h2>ESP32 WiFi Setup</h2>

    <form action="/save" method="POST">

    <label>WiFi:</label>

    <select name="ssid">

    )rawliteral";

    html += wifiOptions;

    html += R"rawliteral(

    </select>

    <label>Password:</label>

    <input type="password" name="pass">

    <button type="submit">
    Save WiFi
    </button>

    </form>

    <hr>

    <p>
    Hold BOOT 5 seconds to reset WiFi
    </p>

    </body>
    </html>

    )rawliteral";

    return html;
}

// =====================================================
// WEB ROOT
// =====================================================
void handleRoot()
{
    server.send(
        200,
        "text/html",
        htmlPage());
}

// =====================================================
// SAVE WIFI WEB
// =====================================================
void handleSave()
{
    String ssid =
        server.arg("ssid");

    String pass =
        server.arg("pass");

    saveWiFi(ssid, pass);

    server.send(
        200,
        "text/html",
        "<h2>Saved! Restarting...</h2>");

    delay(2000);

    ESP.restart();
}

// =====================================================
// START AP
// =====================================================
void startAP()
{
    WiFi.mode(WIFI_AP);

    WiFi.softAP(
        AP_SSID,
        AP_PASS);

    wifiConnected = false;

    scanWiFi();

    Serial.println("");

    Serial.println("=== AP MODE ===");

    Serial.print("CONFIG URL: http://");

    Serial.println(
        WiFi.softAPIP());

    server.on(
        "/",
        handleRoot);

    server.on(
        "/save",
        HTTP_POST,
        handleSave);

    server.begin();

    Serial.println(
        "WebServer started");
}

// =====================================================
// SEND DATA
// =====================================================
void sendData()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println(
            "WiFi NOT CONNECTED");

        return;
    }

    HTTPClient http;

    http.begin(serverURL);

    http.addHeader(
        "Content-Type",
        "application/x-www-form-urlencoded");

    String postData =
        "ph=" + String(phValue, 2);

    int httpCode =
        http.POST(postData);

    Serial.print("HTTP Code: ");

    Serial.println(httpCode);

    String response =
        http.getString();

    Serial.println(response);

    http.end();
}