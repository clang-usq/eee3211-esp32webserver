#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "esp_log.h"
#include <esp_now.h>

static const char *TAG = "garden";
constexpr char kWifiSsid[] = "Deco_Guest";
constexpr char kWifiPassword[] = "";
constexpr char kMdnsHostname[] = "gardenmonitoringstation";
constexpr unsigned long kWifiConnectTimeoutMs = 15000;

WebServer server(80);

//this is the full data store used by the webpage/api
struct SensorData {
    float humidity;
    float windSpeed;
    int windDirection;
    float rain;
    uint32_t lastUpdate;
};

//Rainfall ID-1, Wind ID-2, Humidity ID-3
enum SensorType : uint8_t {
    SENSOR_RAINFALL = 1,
    SENSOR_WIND = 2,
    SENSOR_HUMIDITY = 3
};

// packet from each esp now node with ID

struct SensorPacket {
    uint8_t sensorId;
    float value1;
    float value2;
};

//store the latest received sensor values here
SensorData latestData = {0};
bool hasSensorData = false;


void logRequest(const char* routeName) {
    ESP_LOGI(TAG, "GET %s from %s",
            routeName,
            server.client().remoteIP().toString().c_str());
}

//callback for when esp now data is received
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    //right now only the wind sensor is sending data
    if (len != sizeof(SensorPacket)) {
        ESP_LOGI(TAG, "ESP-NOW packet size mismatch: %d", len);
        return;
    }

    //copy the wind packet into a local struct first
    SensorPacket packet;
    memcpy(&packet, data, sizeof(SensorPacket));


    switch (packet.sensorId) {
        case SENSOR_WIND: // wind
            latestData.windSpeed = packet.value1;
            latestData.windDirection = (int)packet.value2;
            latestData.lastUpdate = millis();
            ESP_LOGI(TAG, "Wind update speed=%.2f dir=%d",
                    latestData.windSpeed, latestData.windDirection);
            break;

        case SENSOR_HUMIDITY: // humidity
            latestData.humidity = packet.value1;
            latestData.lastUpdate = millis();
            ESP_LOGI(TAG, "Humidity update %.2f", latestData.humidity);
            break;

        case SENSOR_RAINFALL: // rainfall
            latestData.rain = packet.value1;
            latestData.lastUpdate = millis();
            ESP_LOGI(TAG, "Rain update %.2f", latestData.rain);
            break;

        default:
            ESP_LOGI(TAG, "Unknown sensor id: %d", packet.sensorId);
            break;
    }   

    hasSensorData = true;


    ESP_LOGI(TAG,
            "ESP-NOW wind recv from %02X:%02X:%02X:%02X:%02X:%02X speed=%.2f direction=%d",
            info->src_addr[0], info->src_addr[1], info->src_addr[2],
            info->src_addr[3], info->src_addr[4], info->src_addr[5],
            latestData.windSpeed, latestData.windDirection);
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
            ESP_LOGI(TAG, "Wi-Fi station started");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            ESP_LOGI(TAG, "Connected to access point");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&info.got_ip.ip_info.ip));
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Wi-Fi disconnected, reason=%d", info.wifi_sta_disconnected.reason);
            break;
        default:
            break;
    }
}

const char html[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Garden Monitoring System</title>
    <link rel="stylesheet" href="/app.css">
</head>
<body>
    <main class="dashboard">
        <h1>Garden Monitoring System</h1>

    <div class="card" aria-label="Wind details">
        <h2>Wind</h2>
        <p><span id="windSpeed"></span> m/s <span id="windDirection"></span> degrees</p>
    </div>

    <div class="card" aria-label="Humidity">
        <h2>Humidity</h2>
        <p><span id="humidity">64</span>%</p>
    </div>

    <div class="card" aria-label="Rain chance">
        <h2>Rain</h2>
        <p><span id="rain">10</span>mm</p>
    </div>
    <div class="card" aria-label="Last updated">
        <h2>Last Updated</h2>
        <p><span id="lastUpdate"></span></p>
    </div>
    </main>

    <script src="/app.js"></script>
</body>
</html>

)HTML";

const char kAppCss[] PROGMEM = R"CSS(
* {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

body {
  min-height: 100vh;
  padding: 12px;
  background: linear-gradient(180deg, #dff5d7 0%, #9ed28f 48%, #5f9f5c 100%);
  font-family: sans-serif;
}

.dashboard {
  width: min(100%, 420px);
  min-height: calc(100vh - 24px);
  margin: 0 auto;
  padding: 12px;
  background: rgba(252, 252, 252, 0.92);
  border-radius: 18px;
  box-shadow: 0 16px 40px rgba(34, 76, 31, 0.18);
}

h1 {
  margin: 0 0 12px;
  font-size: 1.2rem;
  color: #1f3a1d;
}

.card {
  margin-bottom: 10px;
  padding: 12px;
  border: none;
  border-radius: 14px;
  background: rgba(255, 255, 255, 0.78);
  box-shadow: 0 8px 22px rgba(46, 87, 42, 0.12);
}

h2,
.card p {
  margin: 0 0 4px;
}

h2 {
  font-size: 0.95rem;
  color: #365a31;
}

.card p {
  font-size: 1rem;
  color: #1f2b1f;
}

@media (max-width: 320px) {
  body {
    padding: 8px;
  }

  .dashboard {
    min-height: calc(100vh - 16px);
    padding: 10px;
  }

  h1 {
    font-size: 1.05rem;
  }

  .card {
    padding: 10px;
  }
}

@media (min-width: 768px) {
  body {
    padding: 24px;
  }

  .dashboard {
    width: min(100%, 560px);
    padding: 18px;
  }

  h1 {
    font-size: 1.45rem;
  }

  .card {
    padding: 14px;
  }
}

)CSS";

const char kAppJs[] PROGMEM = R"JS(

console.log('Loaded Garden Monitoring System');

/**
 * Weather dashboard data.
 * @typedef {Object} weatherData
 * @property {number} humidity - Relative humidity.
 * @property {number} windSpeed - Wind speed.
 * @property {string} windDirection - Wind direction.
 * @property {number} rain - Rain amount.
 */

/**
 * @param {weatherData} data
 */
function updateDashboard(data) {
    document.getElementById("humidity").textContent = data.humidity;
    document.getElementById("windSpeed").textContent = data.windSpeed;
    document.getElementById("windDirection").textContent = data.windDirection;
    document.getElementById("rain").textContent = data.rain;
    document.getElementById("lastUpdate").textContent = data.lastUpdate;
}


// fetch request to API endpoint
async function requestWeather(){
    const response = await fetch("/v1/data/");
    //log an error if status is not 200. 
    if (!response.ok) {
        throw new Error(`Error: ${response.status}`);
    }
    //else return the response parsed as json.
    return response.json();
}

//pass the json response here to format data to send to updateDashboard
function parseJson(data) {
    return {
        humidity: data.humidity ?? "--",
        windSpeed: data.windSpeed ?? "--",
        windDirection: data.windDirection ?? "--",
        rain: data.rain ?? "--",
        lastUpdate: data.lastUpdate ?? new Date().toLocaleTimeString(),
    };
}

async function refreshDashboard() {
    try {
        const response = await requestWeather();
        updateDashboard(parseJson(response));
    } catch (error) {
        console.error("Failed to fetch weather data", error);
    }
}

refreshDashboard();
setInterval(refreshDashboard, 5000);

)JS";

String buildDataJson() {
    String json = "{";
    json += "\"device\":\"esp32c6\",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"uptime_ms\":" + String(millis()) + ",";
    json += "\"humidity\":";
    json += hasSensorData ? String(latestData.humidity) : "null";
    json += ",";
    json += "\"windSpeed\":";
    json += hasSensorData ? String(latestData.windSpeed) : "null";
    json += ",";
    json += "\"windDirection\":";
    json += hasSensorData ? String(latestData.windDirection) : "null";
    json += ",";
    json += "\"rain\":";
    json += hasSensorData ? String(latestData.rain) : "null";
    json += ",";
    json += "\"lastUpdate\":";
    json += hasSensorData ? String(latestData.lastUpdate) : "null";
    json += "}";
    return json;
}

void handleRoot() {
    logRequest("/");
    server.send_P(200, "text/html", html);
}

void handleCss() {
    logRequest("/app.css");
    server.send_P(200, "text/css", kAppCss);
}

void handleJs() {
    logRequest("/app.js");
    server.send_P(200, "application/javascript", kAppJs);
}

void handleData() {
    logRequest("/v1/data/");
    server.send(200, "application/json", buildDataJson());
}

void handleNotFound() {
    ESP_LOGI(TAG, "404 %s from %s",
            server.uri().c_str(),
            server.client().remoteIP().toString().c_str());
    server.send(404, "application/json",
                "{\"error\":\"Not found\",\"available_endpoint\":\"/v1/data/\"}");
}

bool connectToWifi() {
    WiFi.onEvent(onWiFiEvent);
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(kMdnsHostname);
    WiFi.begin(kWifiSsid, kWifiPassword);

    ESP_LOGI(TAG, "Connecting to %s", kWifiSsid);
    ESP_LOGI(TAG, "ESP-NOW receiver MAC: %s", WiFi.macAddress().c_str());

    const unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED &&
            millis() - startMs < kWifiConnectTimeoutMs) {
        delay(500);
        ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    }

    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi connection timed out, status=%d", WiFi.status());
        return false;
    }

    ESP_LOGI(TAG, "Wi-Fi connected");
    ESP_LOGI(TAG, "Wi-Fi IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

void startMdns() {
    if (!MDNS.begin(kMdnsHostname)) {
        ESP_LOGI(TAG, "mDNS setup failed");
        return;
    }

    MDNS.addService("http", "tcp", 80);
    ESP_LOGI(TAG, "mDNS ready: http://%s.local", kMdnsHostname);
}

//start esp-now in receive mode
bool startEspNow() {
    if (esp_now_init() != ESP_OK) {
        ESP_LOGI(TAG, "ESP-NOW init failed");
        return false;
    }

    //register the receive callback so this esp can accept sensor packets
    esp_now_register_recv_cb(onEspNowRecv);
    ESP_LOGI(TAG, "ESP-NOW receiver ready");
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    ESP_LOGI(TAG, "Starting Garden Monitoring Station");

    const bool wifiConnected = connectToWifi();
    startMdns();
    startEspNow();

    server.on("/", handleRoot);
    server.on("/app.css", handleCss);
    server.on("/app.js", handleJs);
    server.on("/v1/data/", handleData);
    server.onNotFound(handleNotFound);
    server.begin();
    ESP_LOGI(TAG, "Web server started");
    ESP_LOGI(TAG, "Open http://%s", WiFi.localIP().toString().c_str());
    ESP_LOGI(TAG, "Or http://%s.local", kMdnsHostname);

    if (!wifiConnected) {
        ESP_LOGI(TAG, "Server started, but Wi-Fi is not connected yet");
    }
}

void loop() {
    server.handleClient();
    delay(2);
}

extern "C" void app_main(void) {
    ESP_EARLY_LOGI(TAG, "app_main reached");
    initArduino();
    setup();

    while (true) {
        loop();
    }
}
