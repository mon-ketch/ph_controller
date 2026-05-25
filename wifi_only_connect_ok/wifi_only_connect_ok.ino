#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>

WebServer server(80);
Preferences prefs;

#define PH_PIN 34

// ===== CALIBRATION VALUES =====
float v7 = 0;      // voltage tại pH 7
float v4 = 0;      // voltage tại pH 4

float slope = 0;
float offset = 0;



// ==========================
// PIN
// ==========================
#define LED_PIN 2
#define BOOT_PIN 0

// ==========================
// WIFI AP
// ==========================
const char* AP_SSID = "ESP32-Setup";
const char* AP_PASS = "12345678";


// ==========================
// SERVER PHP (XAMPP)
// ==========================
// IP máy tính chạy XAMPP (KHÔNG dùng localhost)
const char* serverURL = "https://httpbin.org/post";

// ==========================
// WIFI DATA
// ==========================
String savedSSID = "";
String savedPASS = "";

bool wifiConnected = false;

// ==========================
// LED
// ==========================
unsigned long ledMillis = 0;
bool ledState = false;

// ==========================
// BUTTON
// ==========================
bool buttonHolding = false;
unsigned long buttonPressTime = 0;


// ==========================
// TIMER GỬI DATA
// ==========================
// lưu thời gian gửi lần cuối
unsigned long lastSend = 0;


// ===============================
// ĐỌC ADC → VOLTAGE
// ===============================
float readVoltage()
{
    long sum = 0;

    for (int i = 0; i < 20; i++)
    {
        sum += analogRead(PH_PIN);
        delay(10);
    }

    float adc = sum / 20.0;

    float voltage = adc * 3.3 / 4095.0;

    return voltage;
}

// ===============================
// MODE 1: ĐO pH 7 (CALIBRATE)
// ===============================
void calPH7()
{
    v7 = readVoltage();

    Serial.println("===== CAL PH 7 =====");
    Serial.print("Voltage pH7 = ");
    Serial.println(v7);
}

// ===============================
// MODE 2: ĐO pH 4 (CALIBRATE)
// ===============================
void calPH4()
{
    v4 = readVoltage();

    Serial.println("===== CAL PH 4 =====");
    Serial.print("Voltage pH4 = ");
    Serial.println(v4);
}

// ===============================
// TÍNH SLOPE + OFFSET
// ===============================
void computeCalib()
{
    if (v7 == 0 || v4 == 0)
    {
        Serial.println("ERROR: chưa có đủ 2 điểm!");
        return;
    }

    slope = (4.0 - 7.0) / (v4 - v7);
    offset = 7.0 - slope * v7;

    Serial.println("===== CALIBRATION DONE =====");
    Serial.print("Slope = ");
    Serial.println(slope);

    Serial.print("Offset = ");
    Serial.println(offset);
}


// ==========================
// HÀM GIẢ LẬP pH
// ==========================
// mục đích: giả lập giá trị pH (thay cho cảm biến thật)
float readPH()
{
    float voltage = readVoltage();

    float ph = slope * voltage + offset;

    return ph;
    // giá trị trung bình 6.5
    // dao động ±0.1
    //return 6.5 + random(-10, 10) / 100.0;
}

// ===============================
// DEBUG REALTIME
// ===============================
void printDebug()
{
    float voltage = readVoltage();
    float ph = readPH();

    Serial.print("Voltage: ");
    Serial.print(voltage);

    Serial.print(" | pH: ");
    Serial.println(ph);
}


// ==========================
// HÀM GIẢ LẬP EC
// ==========================
// mục đích: giả lập độ dẫn điện EC
float readEC()
{
    // giá trị trung bình 1.2 mS/cm
    // dao động ±0.2
    return 1.2 + random(-20, 20) / 100.0;
}

// ==========================
// HÀM GỬI DATA LÊN PHP
// ==========================
// mục đích: gửi pH + EC lên server mỗi 3 giây
void sendData()
{
    // kiểm tra WiFi trước khi gửi
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi NOT CONNECTED -> skip send");
        return;
    }
    else
    {
        Serial.println("WiFi OK -> sending data...");
    }

    HTTPClient http;   // tạo HTTP client

    http.begin(serverURL);  // kết nối tới PHP

    // khai báo kiểu dữ liệu POST form
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    // đọc sensor
    float ph = readPH();
    float ec = readEC();

    // đóng gói dữ liệu gửi lên server
    String postData =
        "ph=" + String(ph) +
        "&ec=" + String(ec);

    // gửi POST request
    int httpCode = http.POST(postData);

    // debug kết quả server trả về
    Serial.print("HTTP Code: ");
    Serial.println(httpCode);

    Serial.println("Response: ");
    String response = http.getString();
    Serial.println(response);

    // đóng kết nối HTTP
    http.end();
}


// ==========================
// LOAD WIFI
// ==========================
void loadWiFi()
{
    prefs.begin("wifi", true);

    savedSSID =
        prefs.getString("ssid", "");

    savedPASS =
        prefs.getString("pass", "");

    prefs.end();
}


// ==========================
// SAVE WIFI
// ==========================
void saveWiFi(
    String ssid,
    String pass)
{
    prefs.begin("wifi", false);

    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);

    prefs.end();

    Serial.println("WiFi Saved");
}


// ==========================
// RESET WIFI
// ==========================
void resetWiFi()
{
    Serial.println("RESET WIFI");

    prefs.begin("wifi", false);

    prefs.clear();

    prefs.end();

    delay(1000);

    ESP.restart();
}


// ==========================
// CONNECT WIFI
// ==========================
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
        pass.c_str()
    );

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

        Serial.print("WEB URL: http://");
        Serial.println(WiFi.localIP());

        wifiConnected = true;

        return true;
    }

    Serial.println("");
    Serial.println("FAILED");

    wifiConnected = false;

    return false;
}


// ==========================
// SCAN WIFI HTML
// ==========================
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


// ==========================
// HTML PAGE
// ==========================
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


// ==========================
// WEB ROOT
// ==========================
void handleRoot()
{
    server.send(
        200,
        "text/html",
        htmlPage()
    );
}


// ==========================
// SAVE WIFI WEB
// ==========================
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
        "<h2>Saved! Restarting...</h2>"
    );

    delay(2000);

    ESP.restart();
}


// ==========================
// START AP
// ==========================
void startAP()
{
    WiFi.mode(WIFI_AP);

    WiFi.softAP(
        AP_SSID,
        AP_PASS
    );

    wifiConnected = false;

    scanWiFi();

    Serial.println("");
    Serial.println("=== AP MODE ===");

    Serial.print("CONFIG URL: http://");

    Serial.println(
        WiFi.softAPIP()
    );

    server.on(
        "/",
        handleRoot
    );

    server.on(
        "/save",
        HTTP_POST,
        handleSave
    );

    server.begin();

    Serial.println(
        "WebServer started"
    );
}


// ==========================
// LED STATUS
// ==========================
void handleLED()
{
    unsigned long interval;

    if (wifiConnected)
    {
        // chớp chậm
        interval = 1000;
    }
    else
    {
        // chớp nhanh
        interval = 200;
    }

    if (millis() - ledMillis >= interval)
    {
        ledMillis = millis();

        ledState = !ledState;

        digitalWrite(
            LED_PIN,
            ledState
        );
    }
}


// ==========================
// BOOT BUTTON
// ==========================
void handleButton()
{
    if (digitalRead(BOOT_PIN) == LOW)
    {
        if (!buttonHolding)
        {
            buttonHolding = true;

            buttonPressTime =
                millis();
        }
        else
        {
            if (
                millis() -
                buttonPressTime >= 5000)
            {
                resetWiFi();
            }
        }
    }
    else
    {
        buttonHolding = false;
    }
}


// ==========================
// SETUP
// ==========================
void setup()
{
    Serial.begin(115200);

    Serial.println("");
    Serial.println("ESP32 START");

    pinMode(
        LED_PIN,
        OUTPUT
    );

    pinMode(
        BOOT_PIN,
        INPUT_PULLUP
    );

    delay(1000);

    loadWiFi();

    if (
        savedSSID != "" &&
        connectWiFi(
            savedSSID,
            savedPASS))
    {
        server.on(
            "/",
            handleRoot
        );

        server.begin();

        Serial.println(
            "WebServer started"
        );
    }
    else
    {
        startAP();
    }
}


// ==========================
// LOOP
// ==========================
void loop()
{
    server.handleClient();

    handleLED();

    handleButton();

    // mỗi 3 giây gửi 1 lần
    if (millis() - lastSend > 3000)
    {
        lastSend = millis();  // cập nhật thời gian

        sendData();          // gửi dữ liệu pH + EC
    }
}