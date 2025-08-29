#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>

const char* ssid = "buntut v2";
const char* password = "987654321";

const int flowSensorPin = 26;
volatile int pulseCount = 0;
float calibrationFactor = 7.5;

float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres;
unsigned long oldTime;

#define MAX_HISTORY 10
float usageHistory[MAX_HISTORY];
int historyIndex = 0;

unsigned long lastReportTime = 0;
const unsigned long reportInterval = 90000; // .... Menit

AsyncWebServer server(80);
Preferences preferences;

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void addToHistory(float newValue) {
  if (historyIndex < MAX_HISTORY) {
    usageHistory[historyIndex++] = newValue;
  } else {
    for (int i = 1; i < MAX_HISTORY; i++) {
      usageHistory[i - 1] = usageHistory[i];
    }
    usageHistory[MAX_HISTORY - 1] = newValue;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(flowSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected. IP: " + WiFi.localIP().toString());

  preferences.begin("waterflow", false);
  totalMilliLitres = preferences.getULong("total", 0);

  oldTime = millis();
  lastReportTime = millis();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='0;url=/monitor'></head><body></body></html>");
  });

  server.on("/monitor", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>IoT Waterflow Monitor</title>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <script src='https://cdn.jsdelivr.net/npm/chart.js'></script>
  <style>
    body { margin: 0; font-family: 'Segoe UI', sans-serif; background: #e8f0ff; }
    header { background: #005fba; color: white; display: flex; align-items: center; justify-content: center; padding: 10px; }
    header img { height: 45px; margin: 0 10px; }
    header h1 { margin: 0; font-size: 1.5em; color: white; flex-grow: 1; text-align: center; }
    .container { max-width: 900px; margin: auto; padding: 20px; }
    .card { background: white; border-radius: 15px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
    .card h2 { margin-top: 0; font-size: 1.2em; display: flex; align-items: center; }
    .card h2 i { margin-right: 10px; color: #005fba; }
    button { padding: 10px 15px; background: #005fba; color: white; border: none; border-radius: 8px; cursor: pointer; margin: 5px 0; width: 100%; }
    .csv-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    canvas { max-width: 100%; }
    .customer-id {
      background: linear-gradient(135deg, #4a90e2, #005fba); color: white; text-align: center;
      border-radius: 20px; box-shadow: 0 6px 15px rgba(0, 95, 186, 0.4); padding: 25px 0;
      margin-bottom: 25px; font-weight: 700; font-size: 2rem; letter-spacing: 3px; user-select: none;
    }
    .customer-id h2 { font-size: 1.3rem; margin-bottom: 10px; display: flex; justify-content: center; align-items: center; gap: 10px; color: #cbe6ff; font-weight: 600; }
    .customer-id h2 i { font-size: 1.7rem; color: #ffdd57; }
    .id-number { font-family: 'Courier New', Courier, monospace; font-size: 2.5rem; letter-spacing: 5px; }
  </style>
  <link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css'/>
</head>
<body>
  <header>
    <img src='https://akuntansi.polinema.ac.id/wp-content/uploads/2024/02/logo_polinema.png'>
    <h1>IoT Waterflow Monitor</h1>
    <img src='https://smart.sdsi.co.id/wp-content/uploads/sites/5/2023/01/LOGO-PDAM-copy.png'>
  </header>

  <div class='container'>
    <div class='card customer-id' ondblclick="resetTotal()">
      <h2><i class='fas fa-id-card'></i> ID Pelanggan</h2>
      <div class="id-number">10000000</div>
    </div>

    <div class='card'>
      <h2><i class='fas fa-tachometer-alt'></i> Flow Rate</h2>
      <p><strong id='flowrate'>0</strong> L/min</p>
    </div>

    <div class='card'>
      <h2><i class='fas fa-water'></i> Total Volume</h2>
      <p><strong id='total_l'>0</strong> L</p>
      <p><strong id='total_m3'>0</strong> m&sup3;</p>
    </div>

    <div class='card'>
      <h2><i class='fas fa-chart-line'></i> Grafik Pemakaian (10 data terakhir)</h2>
      <canvas id='flowChart'></canvas>
    </div>

    <div class='card'>
      <h2><i class='fas fa-file-export'></i> Laporan & CSV</h2>
      <div class='csv-grid'>
        <button onclick='sendReport()'><i class='fas fa-paper-plane'></i> Kirim ke Spreadsheet</button>
        <button onclick="window.open('https://docs.google.com/spreadsheets/d/18pbNWbG-zkWf3ZrigFWbVjf7slf8Qtviqr81ImBJHio', '_blank')"><i class='fas fa-table'></i> Lihat Spreadsheet</button>
        <button onclick="window.open('https://docs.google.com/spreadsheets/d/18pbNWbG-zkWf3ZrigFWbVjf7slf8Qtviqr81ImBJHio/edit?gid=1313276760#gid=1313276760=Ringkasan Harian', '_blank')"><i class='fas fa-calendar-day'></i> Ringkasan Harian</button>
        <button onclick="window.open('https://docs.google.com/spreadsheets/d/18pbNWbG-zkWf3ZrigFWbVjf7slf8Qtviqr81ImBJHio/edit?gid=142746720#gid=142746720=Ringkasan Bulanan', '_blank')"><i class='fas fa-calendar-alt'></i> Ringkasan Bulanan</button>
      </div>
    </div>
  </div>

  <script>
    let chart;
    async function updateData() {
      const res = await fetch('/data');
      const json = await res.json();
      document.getElementById('flowrate').innerText = json.flowRate.toFixed(3);
      document.getElementById('total_l').innerText = json.totalLiters.toFixed(3);
      document.getElementById('total_m3').innerText = (json.totalLiters / 1000).toFixed(5);

      const ctx = document.getElementById('flowChart').getContext('2d');
      if (!chart) {
        chart = new Chart(ctx, {
          type: 'line',
          data: {
            labels: json.labels,
            datasets: [{
              label: 'Total Liter (L)',
              data: json.history,
              borderColor: '#005fba',
              tension: 0.2,
              fill: false
            }]
          },
          options: {
            responsive: true,
            animation: false,
            scales: { y: { beginAtZero: true } }
          }
        });
      } else {
        chart.data.labels = json.labels;
        chart.data.datasets[0].data = json.history;
        chart.update();
      }
    }

    async function resetTotal() {
      await fetch('/reset');
      updateData(); // Update grafik & angka setelah reset
    }

    async function sendReport() {
      const res = await fetch('/report');
      const txt = await res.text();
      alert(txt);
    }

    setInterval(updateData, 2000);
    updateData();
  </script>
</body>
</html>
    )rawliteral";
    request->send(200, "text/html", html);
  });

  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "{";
    json += "\"flowRate\":" + String(flowRate, 3) + ",";
    json += "\"totalLiters\":" + String(totalMilliLitres / 1000.0, 3) + ",";
    json += "\"labels\":[";
    for (int i = 0; i < historyIndex; i++) {
      json += "\"Data " + String(i + 1) + "\"";
      if (i < historyIndex - 1) json += ",";
    }
    json += "],\"history\":[";
    for (int i = 0; i < historyIndex; i++) {
      json += String(usageHistory[i], 2);
      if (i < historyIndex - 1) json += ",";
    }
    json += "]}";
    request->send(200, "application/json", json);
  });

  server.on("/report", HTTP_GET, [](AsyncWebServerRequest *request){
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String url = "https://script.google.com/macros/s/AKfycbzR9FONN1acRGVGBHzp9nT4vkrWDoVdSqg7GTj0dpVvB0lVi7hRvpzXnozyZy9WROEh/exec";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData = "flowRate=" + String(flowRate, 3) + "&totalLiters=" + String(totalMilliLitres / 1000.0, 3);
    int httpCode = http.POST(postData);
    if (httpCode > 0) {
      request->send(200, "text/plain", "Laporan terkirim!");
    } else {
      request->send(200, "text/plain", "Gagal mengirim laporan.");
    }
    http.end();
  });

  // Tambahan: Reset total volume
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
    totalMilliLitres = 0;
    preferences.putULong("total", 0);
    historyIndex = 0;
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}

void loop() {
  if ((millis() - oldTime) > 1000) {
    detachInterrupt(digitalPinToInterrupt(flowSensorPin));
    flowRate = ((1000.0 / (millis() - oldTime)) * pulseCount) / calibrationFactor;
    oldTime = millis();
    flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;
    pulseCount = 0;
    addToHistory(totalMilliLitres / 1000.0);
    preferences.putULong("total", totalMilliLitres);
    attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
  }

  if (millis() - lastReportTime > reportInterval) {
    lastReportTime = millis();
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    String url = "https://script.google.com/macros/s/AKfycbzR9FONN1acRGVGBHzp9nT4vkrWDoVdSqg7GTj0dpVvB0lVi7hRvpzXnozyZy9WROEh/exec";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData = "flowRate=" + String(flowRate, 3) + "&totalLiters=" + String(totalMilliLitres / 1000.0, 3);
    int httpCode = http.POST(postData);
    if (httpCode > 0) {
      Serial.println("Laporan otomatis terkirim.");
    } else {
      Serial.println("Gagal kirim laporan otomatis.");
    }
    http.end();
  }
}
