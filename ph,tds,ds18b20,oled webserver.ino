#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

#define ONE_WIRE_BUS 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const int PH_PIN = 34;
#define TdsSensorPin 35
#define VREF 3.3
#define SCOUNT 30

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); // GMT+7

float suhuDS18B20 = 0.0;
float Po = 0;
float PH_step;
float tdsValue;
int nilai_analog_PH;
double TeganganPh;
int analogBuffer[SCOUNT];
int analogBufferTemp[SCOUNT];
int analogBufferIndex = 0;
int copyIndex = 0;
float averageVoltage = 0;
float temperature = 25;

float PH4 = 3.1; // Tegangan saat pH 4
float PH7 = 2.5; // Tegangan saat pH 7

const char* ssid = "Iphone (2)";
const char* password = "Alka200318";
WebServer server(80);

// Struktur dan array untuk menyimpan history
#define MAX_HISTORY 10
struct SensorData {
  String waktu;
  float ph;
  float tds;
  float suhu;
};
SensorData history[MAX_HISTORY];
int historyIndex = 0;
int lastSavedHour = -1; // Untuk menyimpan data hanya saat jam berubah

int getMedianNum(int bArray[], int iFilterLen) {
  int bTab[iFilterLen];
  for (byte i = 0; i < iFilterLen; i++) bTab[i] = bArray[i];
  int i, j, bTemp;
  for (j = 0; j < iFilterLen - 1; j++) {
    for (i = 0; i < iFilterLen - j - 1; i++) {
      if (bTab[i] > bTab[i + 1]) {
        bTemp = bTab[i];
        bTab[i] = bTab[i + 1];
        bTab[i + 1] = bTemp;
      }
    }
  }
  if ((iFilterLen & 1) > 0) return bTab[(iFilterLen - 1) / 2];
  else return (bTab[iFilterLen / 2] + bTab[iFilterLen / 2 - 1]) / 2;
}

void simpanHistory() {
  if (!timeClient.update()) timeClient.forceUpdate();
  String waktuSekarang = timeClient.getFormattedTime();
  history[historyIndex] = { waktuSekarang, Po, tdsValue, suhuDS18B20 };
  historyIndex = (historyIndex + 1) % MAX_HISTORY;
}

void handleRoot() {
  char msg[3000];
  snprintf(msg, 3000,
    "<!DOCTYPE html>\
    <html lang='en'>\
    <head>\
      <meta charset='UTF-8'>\
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>\
      <meta http-equiv='refresh' content='5'>\
      <title>ESP32 Monitoring</title>\
      <link rel='stylesheet' href='https://use.fontawesome.com/releases/v5.7.2/css/all.css'>\
      <style>\
        body { font-family: Arial, sans-serif; background: #f4f4f4; margin: 0; padding: 0;}\
        header { background: #2196F3; color: white; padding: 20px; text-align: center; box-shadow: 0 2px 4px rgba(0,0,0,0.2);}\
        .container { display: flex; flex-wrap: wrap; justify-content: center; margin: 20px; }\
        .card { background: white; border-radius: 15px; padding: 20px; margin: 10px; width: 250px; text-align: center; box-shadow: 0 4px 8px rgba(0,0,0,0.1); transition: transform 0.2s ease-in-out;}\
        .card:hover { transform: translateY(-5px); box-shadow: 0 8px 16px rgba(0,0,0,0.15);}\
        .card i { font-size: 4rem; margin-bottom: 10px; }\
        .value { font-size: 2.5rem; font-weight: bold; color: #333; }\
        table { border-collapse: collapse; margin: 20px auto 40px; width: 90%; max-width: 800px; background: white; box-shadow: 0 4px 8px rgba(0,0,0,0.1); border-radius: 8px; overflow: hidden;}\
        th, td { border-bottom: 1px solid #ddd; padding: 12px 15px; text-align: center; color: #555; }\
        th { background-color: #2196F3; color: white; font-weight: 600; position: sticky; top: 0; }\
        tr:hover { background-color: #f1f1f1; }\
        h3 { text-align: center; color: #2196F3; margin-top: 40px; font-weight: 600; }\
        @media (max-width: 600px) {\
          .container { flex-direction: column; align-items: center; }\
          .card { width: 90%; max-width: 300px; }\
          table { width: 100%; }\
        }\
      </style>\
    </head>\
    <body>\
      <header><h2>Monitoring pH, TDS, & Suhu</h2></header>\
      <div class='container'>\
        <div class='card'>\
          <i class='fas fa-tint' style='color:#00add6;'></i>\
          <div class='value'>%.2f ppm</div><div>TDS</div></div>\
        <div class='card'>\
          <i class='fas fa-flask' style='color:#00c853;'></i>\
          <div class='value'>%.2f</div><div>pH Air</div></div>\
        <div class='card'>\
          <i class='fas fa-thermometer-half' style='color:#ff3d00;'></i>\
          <div class='value'>%.2f &deg;C</div><div>Suhu Air</div></div>\
      </div>", tdsValue, Po, suhuDS18B20);

  String table = "<h3>Riwayat Pembacaan Tiap Jam</h3><table>\
<tr><th>Waktu</th><th>pH</th><th>TDS (ppm)</th><th>Suhu (&deg;C)</th></tr>";
  for (int i = 0; i < MAX_HISTORY; i++) {
    int idx = (historyIndex + i) % MAX_HISTORY;
    if (history[idx].waktu != "") {
      table += "<tr><td>" + history[idx].waktu + "</td><td>" +
               String(history[idx].ph, 2) + "</td><td>" +
               String(history[idx].tds, 0) + "</td><td>" +
               String(history[idx].suhu, 1) + "</td></tr>";
    }
  }
  table += "</table></body></html>";

  String fullPage = String(msg) + table;
  server.send(200, "text/html", fullPage);
}

void setup() {
  pinMode(PH_PIN, INPUT);
  pinMode(TdsSensorPin, INPUT);
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED gagal");
    while (true);
  }
  sensors.begin();
  display.display();
  delay(1000);
  display.clearDisplay();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\nWiFi terhubung");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32")) Serial.println("mDNS dimulai");

  timeClient.begin();
  timeClient.update();

  server.on("/", handleRoot);
  server.begin();
  Serial.println("Server dimulai");
}

void loop() {
  sensors.requestTemperatures();
  suhuDS18B20 = sensors.getTempCByIndex(0);
  temperature = suhuDS18B20;

  nilai_analog_PH = analogRead(PH_PIN);
  TeganganPh = VREF / 4095.0 * nilai_analog_PH;
  PH_step = (PH4 - PH7) / 3.0;
  Po = 7.00 + ((PH7 - TeganganPh) / PH_step);

  static unsigned long analogSampleTimepoint = millis();
  if (millis() - analogSampleTimepoint > 40U) {
    analogSampleTimepoint = millis();
    analogBuffer[analogBufferIndex] = analogRead(TdsSensorPin);
    analogBufferIndex++;
    if (analogBufferIndex == SCOUNT) analogBufferIndex = 0;
  }

  static unsigned long printTimepoint = millis();
  if (millis() - printTimepoint > 800U) {
    printTimepoint = millis();
    for (copyIndex = 0; copyIndex < SCOUNT; copyIndex++)
      analogBufferTemp[copyIndex] = analogBuffer[copyIndex];
    averageVoltage = getMedianNum(analogBufferTemp, SCOUNT) * VREF / 4096.0;
    float compensationCoefficient = 1.0 + 0.02 * (temperature - 25.0);
    float compensationVoltage = averageVoltage / compensationCoefficient;
    tdsValue = (133.42 * pow(compensationVoltage, 3)
              - 255.86 * pow(compensationVoltage, 2)
              + 857.39 * compensationVoltage) * 0.5;
  }

  // Simpan history tiap 1 jam berdasarkan jam NTP
  timeClient.update();
  int currentHour = timeClient.getHours();
  if (currentHour != lastSavedHour) {
    lastSavedHour = currentHour;
    simpanHistory();
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);  display.print("pH: ");   display.println(Po, 2);
  display.setCursor(0, 10); display.print("TDS: ");  display.print(tdsValue, 0); display.println(" ppm");
  display.setCursor(0, 20); display.print("Suhu: "); display.print(suhuDS18B20, 1); display.println(" C");
  display.setCursor(0, 32); display.print("IP: ");   display.println(WiFi.localIP());
  display.display();

  server.handleClient();
  delay(1000);
}
