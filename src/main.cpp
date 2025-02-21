#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiManager.h> // Til nem WiFi-konfiguration
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include "time.h"
#include "FS.h"
#include "LITTLEFS.h"

#define ONE_WIRE_BUS 4

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
AsyncWebServer server(80);

unsigned long lastLogTime = 0;
const unsigned long logInterval = 10000;

const char *ssid = "E308";
const char *password = "98806829";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

String storageFormat = "CSV";
String temperatureC = "";

String readDSTemperatureC();
void logTemperature();
String getTimestamp();
String processor(const String &var);

String readDSTemperatureC(){
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  
  Serial.print("🌡️ Sensor Reading: ");
  Serial.println(tempC);  // Debugging temperature readings

  if (tempC == -127.00) {
    Serial.println("❌ Failed to read from DS18B20 sensor");
    return "--";
  }
  return String(tempC);
}

void printLogFile() {
  File logFile = LITTLEFS.open("/temperature_log.txt", "r");
  if (!logFile) {
      Serial.println("⚠️ Failed to open log file for reading!");
      return;
  }

  Serial.println("📂 Reading Log File:");
  while (logFile.available()) {
      Serial.write(logFile.read());  // Print every character
  }
  logFile.close();
}

void logTemperature()
{
  String temperature = readDSTemperatureC();
  String timestamp = getTimestamp();
  String logEntry;
  if (storageFormat == "JSON")
  {
    logEntry = "{\"timestamp\": \"" + timestamp + "\", \"temperature\": " + temperature + "}";
  }
  else
  {
    logEntry = timestamp + "," + temperature;
  }
  Serial.println("Attempting to write: " + logEntry); // Debugging message

  File logFile = LITTLEFS.open("/temperature_log.txt", "a");
  if (!logFile) {
      Serial.println("⚠️ Failed to open log file for writing!");
      return;
  }

  logFile.println(logEntry);
  logFile.close();

  Serial.println("Logged: " + logEntry);

  // Check if data is being stored correctly
  Serial.println("📂 Current Log Data:");
  logFile = LITTLEFS.open("/temperature_log.txt", "r");
  if (logFile) {
      while (logFile.available()) {
          Serial.write(logFile.read());  // Print all saved logs
      }
      logFile.close();
  } else {
      Serial.println("⚠️ Failed to read log file");
  }
}
void checkLogFile() {
  if (LITTLEFS.exists("/temperature_log.txt")) {
      Serial.println("✅ Log file exists!");
  } else {
      Serial.println("❌ Log file does NOT exist!");
  }
}


String getTimestamp()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return "00-00-0000 00:00:00";
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%d-%m-%Y %H:%M:%S", &timeinfo);
  return String(timeString);
}
// Function to replace placeholders in HTML
String processor(const String &var)
{
  if (var == "TEMPERATUREC")
  {
    return temperatureC;
  }
  return String();
}
const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Temperature Logger</title>
  
    <script src="https://code.highcharts.com/highcharts.js"></script>
    
    <style>
      body {
        font-family: Arial, sans-serif;
        background-color: #222;
        color: white;
        text-align: center;
        margin: 0;
        padding: 20px;
      }
      .container {
        max-width: 600px;
        margin: auto;
        background: #333;
        padding: 20px;
        border-radius: 10px;
        box-shadow: 0 0 15px rgba(255, 255, 255, 0.1);
      }
      h1 {
        font-size: 1.8rem;
        margin-bottom: 10px;
      }
      .temp-display {
        font-size: 2.5rem;
        font-weight: bold;
        color: #03DAC6;
        margin: 10px 0;
      }
      #chart-container {
        height: 300px;
        margin-top: 20px;
      }
    </style>
  </head>
  
  <body>
    <div class="container">
      <h1>ESP32 Temperature Monitor</h1>
      <p>Current Temperature:</p>
      <p class="temp-display"><span id="temp">%TEMPERATUREC%</span> °C</p>
      <div id="chart-container"></div>
    </div>
  
    <script>
      console.log("Highcharts is loading...");
      
      var chart = Highcharts.chart('chart-container', {
        chart: { type: 'spline', backgroundColor: '#333' },
        title: { text: 'Live Temperature Data', style: { color: '#fff' } },
        series: [{ name: 'Temperature', data: [], color: '#FF9800' }],
        xAxis: { type: 'datetime', labels: { style: { color: '#fff' } } },
        yAxis: { 
          title: { text: 'Temperature (°C)', style: { color: '#fff' } },
          labels: { style: { color: '#fff' } },
          gridLineColor: '#555'
        },
        legend: { itemStyle: { color: '#fff' } },
        plotOptions: { series: { marker: { enabled: true } } },
        credits: { enabled: false }
      });
  
      function updateTemperature() {
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function() {
          if (this.readyState == 4 && this.status == 200) {
            var temp = parseFloat(this.responseText);
            console.log("New Temperature:", temp);
            document.getElementById("temp").innerHTML = temp.toFixed(2);
  
            var x = (new Date()).getTime();
            chart.series[0].addPoint([x, temp], true, chart.series[0].data.length > 30);
          }
        };
        xhttp.open("GET", "/temperaturec", true);
        xhttp.send();
      }
  
      setInterval(updateTemperature, 5000);
    </script>
  
  </body>
  </html>
  )rawliteral";
void setup(){
  Serial.begin(115200);
  sensors.begin();
  WiFiManager wifiManager;
  wifiManager.autoConnect("ESP32_AP");
  Serial.println("Connected to WiFi");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  server.on("/getlog", HTTP_GET, [](AsyncWebServerRequest *request) {
    File logFile = LITTLEFS.open("/temperature_log.txt", "r");
    if (!logFile) {
        request->send(500, "text/plain", "⚠️ Log file not found!");
        return;
    }

    String logData = "";
    while (logFile.available()) {
        logData += logFile.readStringUntil('\n') + "\n";
    }
    logFile.close();

    request->send(200, "text/plain", logData);});

  if (!LITTLEFS.begin(true)) {
    Serial.println("LITTLEFS Mount Failed");
    return;
  }
  printLogFile();

  checkLogFile(); 

  temperatureC = readDSTemperatureC();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html, processor); });
  server.on("/temperaturec", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String currentTemp = readDSTemperatureC();  // Get latest reading
              request->send(200, "text/plain", currentTemp); });
  server.begin();
}

void loop(){
  if (millis() - lastLogTime >= logInterval) {
    Serial.println("🔄 Updating temperature...");
    temperatureC = readDSTemperatureC();  // Ensure global variable updates
    logTemperature();
    lastLogTime = millis();
  }
}