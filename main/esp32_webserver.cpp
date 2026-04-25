#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "esp_log.h"
#include <esp_now.h>
#include "time.h"

#include "config.h"


static const char *TAG = "garden";

//used guest wifi creds

constexpr char kMdnsHostname[] = "gardenmonitoringstation";
constexpr unsigned long kWifiConnectTimeoutMs = 15000;

//create server object
WebServer server(80);



//this is the full data store used by the webpage/api
struct SensorData {
    float humidity;
    uint32_t humidityReceived;
    float windSpeed;
    int windDirection;
    uint32_t windDataReceived;
    float rain;
    uint32_t rainReceived;
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
    //create NTP time object
    time_t now = time(NULL);

    //copy the wind packet into a local struct first
    SensorPacket packet;
    memcpy(&packet, data, sizeof(SensorPacket));


    switch (packet.sensorId) {
        case SENSOR_WIND: // wind
            latestData.windSpeed = packet.value1;
            latestData.windDirection = (int)packet.value2;
            latestData.windDataReceived = (uint32_t) now;
            ESP_LOGI(TAG, "Wind update speed=%.2f dir=%d",
                    latestData.windSpeed, latestData.windDirection);
            break;

        case SENSOR_HUMIDITY: // humidity
            latestData.humidity = packet.value1;
            latestData.humidityReceived = (uint32_t) now;
            ESP_LOGI(TAG, "Humidity update %.2f", latestData.humidity);
            break;

        case SENSOR_RAINFALL: // rainfall
            latestData.rain = packet.value1;
            latestData.rainReceived = (uint32_t) now;
            ESP_LOGI(TAG, "Rain update %.2f", latestData.rain);
            break;

        default:
            ESP_LOGI(TAG, "Unknown sensor id: %d", packet.sensorId);
            break;
    }   

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


//html - css and js put into memory
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
    <label class="toolbar" for="lastTimeCheckbox">
      <span>Show Last Updated</span>
      <input id="lastTimeCheckbox" type="checkbox">
    </label>
    <p class="status" id="status">Loading data...</p>

    <div class="card" aria-label="Wind details">
      <h2>Wind</h2>
      <p>
        <span id="windSpeed">-- </span> m/s
      </p>
      <p>
        <span id="windDirection">--</span> Degrees
      </p>
      <p>
        <span class="hide" id="windDataLastUpdated"> --</span>
      </p>
    </div>

    <div class="card" aria-label="Humidity">
      <h2>Humidity</h2>
      <p><span id="humidity">-- </span>%</p>
      <p><span class="hide" id="humidityLastUpdated">--</span></p>
    </div>

    <div class="card" aria-label="Rain chance">
      <h2>Rain</h2>
      <p><span id="rain">-- </span>mm</p>
      <p><span class="hide" id="rainLastUpdated">--</span></p>
    </div>
  </main>

  <script src="/app.js"></script>
</body>
</html>
)HTML";

const char kAppCss[] PROGMEM = R"CSS(
/* app.css */

:root {
  --bg-main: #101815;
  --bg-panel: #18231f;
  --bg-card: #1f3029;
  --bg-card-alt: #243a31;

  --text-main: #ecfff5;
  --text-muted: #a8c7b8;

  --accent: #42d97c;
  --accent-soft: rgba(66, 217, 124, 0.18);
  --warning: #f2c94c;
  --danger: #eb5757;

  --border: rgba(255, 255, 255, 0.12);
  --shadow: 0 16px 40px rgba(0, 0, 0, 0.35);

  --radius-lg: 22px;
  --radius-md: 14px;
}

/* Reset */
* {
  box-sizing: border-box;
}

html {
  font-size: 16px;
}

body {
  margin: 0;
  min-height: 100vh;
  font-family: Arial, Helvetica, sans-serif;
  background:
    radial-gradient(circle at top left, rgba(66, 217, 124, 0.18), transparent 35%),
    linear-gradient(135deg, #0b120f, var(--bg-main));
  color: var(--text-main);
}

/* Dashboard shell */
.dashboard {
  width: min(1100px, calc(100% - 32px));
  margin: 0 auto;
  padding: 32px 0;

  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 20px;
}

/* Header spans full dashboard */
.dashboard h1 {
  grid-column: 1 / -1;
  margin: 0;
  padding: 24px 28px;

  font-size: clamp(1.8rem, 4vw, 3rem);
  letter-spacing: 0.04em;
  text-transform: uppercase;

  background: linear-gradient(135deg, var(--bg-panel), #203b30);
  border: 1px solid var(--border);
  border-left: 8px solid var(--accent);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow);
}

/* Toolbar */
.toolbar {
  grid-column: 1 / -1;

  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 16px;

  min-height: 64px;
  padding: 16px 22px;

  background: var(--bg-panel);
  border: 1px solid var(--border);
  border-radius: var(--radius-md);

  color: var(--text-muted);
  font-size: 1rem;
  font-weight: 700;
  text-transform: uppercase;
  letter-spacing: 0.06em;
}

.toolbar input[type="checkbox"] {
  width: 48px;
  height: 28px;
  accent-color: var(--accent);
  cursor: pointer;
}

/* System status */
.status {
  grid-column: 1 / -1;

  margin: 0;
  padding: 14px 18px;

  color: var(--warning);
  background: rgba(242, 201, 76, 0.12);
  border: 1px solid rgba(242, 201, 76, 0.28);
  border-radius: var(--radius-md);

  font-size: 1rem;
  font-weight: 700;
}

/* Cards */
.card {
  position: relative;
  min-height: 230px;
  padding: 24px;

  background:
    linear-gradient(180deg, var(--bg-card-alt), var(--bg-card));
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  box-shadow: var(--shadow);

  overflow: hidden;
}

.card::before {
  content: "";
  position: absolute;
  inset: 0 0 auto 0;
  height: 6px;
  background: linear-gradient(90deg, var(--accent), transparent);
}

.card h2 {
  margin: 0 0 24px;

  color: var(--accent);
  font-size: 1.2rem;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.card p {
  margin: 14px 0;

  color: var(--text-muted);
  font-size: 1.05rem;
  line-height: 1.4;
}

/* Live values */
.card p:first-of-type span:not(.hide),
.card p:nth-of-type(2) span:not(.hide) {
  color: var(--text-main);
  font-size: clamp(2rem, 5vw, 3.4rem);
  font-weight: 800;
  line-height: 1;
}

/* Last updated values */
.hide {
  display: none;
}

.show,
.hide:not(:empty) {
  font-size: 0.9rem;
  color: var(--text-muted);
}

/* Optional: use this if JS toggles a parent class */
.dashboard.show-last-updated .hide {
  display: inline;
}

/* Focus states for HMI/touch accessibility */
input:focus-visible,
.toolbar:focus-within,
.card:focus-within {
  outline: 3px solid var(--accent);
  outline-offset: 3px;
}

/* Tablet displays */
@media (max-width: 850px) {
  .dashboard {
    grid-template-columns: 1fr 1fr;
    width: min(100% - 24px, 760px);
    gap: 16px;
    padding: 24px 0;
  }

  .card {
    min-height: 210px;
    padding: 20px;
  }

  .dashboard h1 {
    padding: 20px;
  }
}

/* Small HMI panels / phones */
@media (max-width: 560px) {
  html {
    font-size: 15px;
  }

  .dashboard {
    grid-template-columns: 1fr;
    width: min(100% - 20px, 420px);
    gap: 14px;
    padding: 16px 0;
  }

  .dashboard h1 {
    padding: 18px;
    border-left-width: 6px;
    text-align: center;
  }

  .toolbar {
    flex-direction: column;
    align-items: stretch;
    text-align: center;
    padding: 16px;
  }

  .toolbar input[type="checkbox"] {
    align-self: center;
    width: 56px;
    height: 32px;
  }

  .status {
    text-align: center;
  }

  .card {
    min-height: 180px;
    padding: 18px;
  }

  .card h2 {
    margin-bottom: 18px;
    font-size: 1.1rem;
  }

  .card p {
    margin: 10px 0;
  }
}

/* Very small embedded displays */
@media (max-width: 360px) {
  .dashboard {
    width: calc(100% - 12px);
    gap: 10px;
    padding: 10px 0;
  }

  .dashboard h1 {
    font-size: 1.35rem;
    padding: 14px;
  }

  .toolbar,
  .status,
  .card {
    border-radius: 12px;
  }

  .card {
    min-height: 160px;
    padding: 14px;
  }

  .card p:first-of-type span:not(.hide),
  .card p:nth-of-type(2) span:not(.hide) {
    font-size: 2rem;
  }
}

)CSS";

const char kAppJs[] PROGMEM = R"JS(

console.log('Loaded Garden Monitoring System');

const lastTimeCheckbox = document.getElementById("lastTimeCheckbox");
if (lastTimeCheckbox) {
    lastTimeCheckbox.addEventListener("change", checkboxUpdated);
}

/**
 * Weather dashboard data
 * @typedef {Object} weatherData
 * @property {number} humidity - Relative humidity
 * @property {number} humidityReceived
 * @property {number} windSpeed - Wind speed
 * @property {string} windDirection - Wind direction
 * @property {number} windDataReceived
 * @property {number} rain - Rain amount
 * @property {number} rainReceived
 */


/**
 * @param {weatherData} data
 */
function updateDashboard(data) {
    const options = {
        weekday: "short",
        year : "numeric",
        month : "short",
        hour : "numeric",
        minute : "numeric",
        second : "numeric",
        hour12 : false,
    }
    const humidityTime = new Date(data.humidityReceived * 1000);
    const windDataTime =  new Date(data.windDataReceived * 1000);
    const rainTime =  new Date(data.rainReceived * 1000);

    document.getElementById("humidity").textContent = data.humidityReceived ? data.humidity : "--";

    if(!data.humidityReceived){
        document.getElementById('humidityLastUpdated').textContent = "--";
    } else {
        document.getElementById('humidityLastUpdated').textContent = humidityTime.toLocaleString("en-AU", options);
    };

    document.getElementById("windSpeed").textContent = data.windDataReceived ? data.windSpeed : "--";
    document.getElementById("windDirection").textContent = data.windDataReceived ? data.windDirection : "--";
    if(!data.windDataReceived){
        document.getElementById('windDataLastUpdated').textContent = "--";
    } else {
        document.getElementById('windDataLastUpdated').textContent = windDataTime.toLocaleString("en-AU", options);
    };

    document.getElementById("rain").textContent = data.rainReceived ? data.rain : "--";

    if(!data.rainReceived){
        document.getElementById('rainLastUpdated').textContent = "--";
    } else {
        document.getElementById('rainLastUpdated').textContent = rainTime.toLocaleString("en-AU", options);
    };

}

function setStatus(message) {
    document.getElementById("status").textContent = message;
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

function checkboxUpdated(){
    const showLastUpdated = Boolean(lastTimeCheckbox?.checked);

    document.getElementById('humidityLastUpdated').classList.toggle('hide', !showLastUpdated);
    document.getElementById('rainLastUpdated').classList.toggle('hide', !showLastUpdated);
    document.getElementById('windDataLastUpdated').classList.toggle('hide', !showLastUpdated);

}

async function refreshDashboard() {
    try {
        const response = await requestWeather();
        updateDashboard(response);
        setStatus("Live API data");
    } catch (error) {
        console.error("Failed to fetch weather data", error);
        // updateDashboard(FALLBACK_DATA);
        // setStatus("Offline demo data");
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
    json += latestData.humidityReceived ? String(latestData.humidity) : "null";
    json += ",";

    json += "\"humidityReceived\":";
    json += latestData.humidityReceived ? String(latestData.humidityReceived) : "null";
    json += ",";

    json += "\"windSpeed\":";
    json += latestData.windDataReceived ? String(latestData.windSpeed) : "null";
    json += ",";
    json += "\"windDirection\":";
    json += latestData.windDataReceived ? String(latestData.windDirection) : "null";
    json += ",";

    json += "\"windDataReceived\":";
    json += latestData.windDataReceived ? String(latestData.windDataReceived) : "null";
    json += ",";

    json += "\"rain\":";
    json += latestData.rainReceived ? String(latestData.rain) : "null";
    json += ",";

    json += "\"rainReceived\":";
    json += latestData.rainReceived ? String(latestData.rainReceived) : "null";

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

    //get ntp time
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

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
