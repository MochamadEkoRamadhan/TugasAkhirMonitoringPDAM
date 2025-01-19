#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// WiFi credentials
const char* ssid = "Warkop Santahoer";
const char* password = "vespa1234";

// Pin sensor dan LED
#define SENSOR  5
#define LED_BUILTIN 2

// Variabel untuk flow sensor
volatile byte pulseCount;
float flowRate;
unsigned int flowMilliLitres;
unsigned long totalMilliLitres = 0;
float calibrationFactor = 4.5;  // Kalibrasi sensor (ubah sesuai sensor Anda)

// Waktu dan data untuk pembacaan
unsigned long previousMillis = 0;
const int interval = 1000;  // Interval pembacaan (ms)

// Web server instance
AsyncWebServer server(80);

// Array untuk menyimpan 30 data terakhir
float flowData[30] = {0};
int dataIndex = 0;

// ISR untuk menangani pulsa dari sensor
void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

void setup() {
  // Serial Monitor
  Serial.begin(115200);

  // Setup pin
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SENSOR, INPUT_PULLUP);

  // Reset variabel
  pulseCount = 0;
  flowRate = 0.0;

  // Attach interrupt untuk sensor
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);

  // Koneksi ke WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println("IP Address: ");
  Serial.println(WiFi.localIP());

  // Web server untuk halaman utama
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html>";
    html += "<html lang='en'>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>IoT Waterflow Monitor</title>";
    html += "<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>";
    html += "<link href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0-beta3/css/all.min.css' rel='stylesheet'>";
    html += "<link href='https://fonts.googleapis.com/css2?family=Poppins:wght@400;600&display=swap' rel='stylesheet'>";
    html += "<style>";
    html += "body { font-family: 'Poppins', sans-serif; background: linear-gradient(to right, #007bff, #00d4ff); color: #333; margin: 0; padding: 0; }";
    html += "h1 { display: flex; align-items: center; justify-content: center; background-color: #fff; color: #007bff; padding: 1rem; margin: 0; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.2); }";
    html += "h1 img { height: 50px; margin-right: 10px; }";
    html += ".logo-container { display: flex; align-items: center; }";
    html += ".container { max-width: 1200px; margin: 2rem auto; padding: 0 1rem; display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }";
    html += ".card { background: #fff; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); padding: 1.5rem; text-align: center; transition: transform 0.3s; }";
    html += ".card:hover { transform: translateY(-5px); }";
    html += ".card h2 { margin: 0; font-size: 1.8rem; color: #007bff; font-weight: 600; }";
    html += ".card p { margin: 0.5rem 0; font-size: 1.2rem; color: #333; font-weight: 400; }";
    html += "canvas { max-width: 100%; margin-top: 20px; }";
    html += ".card h2 i { margin-right: 10px; }";
    html += "@media (max-width: 600px) {";
    html += "  h1 { font-size: 1.5rem; padding: 0.8rem; }";
    html += "  .card { padding: 1rem; }";
    html += "  .card h2 { font-size: 1.5rem; }";
    html += "  .card p { font-size: 1rem; }";
    html += "  .logo-container img { height: 40px; }";
    html += "}";
    html += "</style>";
    html += "</head>";
    html += "<body>";
    html += "<h1>";
    html += "<div class='logo-container'><img src='https://i0.wp.com/www.hpi.or.id/wp-content/uploads/2021/08/Logo-Polinema.png?ssl=1' alt='Polinema Logo'> Monitoring PDAM</div>";
    html += "<img src='https://industriinvilonsagita.com/wp-content/uploads/2024/05/1561962686-1.png' alt='PDAM Logo' style='height: 50px; margin-left: 20px;'>";
    html += "</h1>";
    html += "<div class='container'>";
    html += "  <div class='card'>";
    html += "    <h2><i class='fas fa-tachometer-alt' style='color: #007bff;'></i> Flow Rate</h2>";
    html += "    <p><span id='flowRate'>0.000</span> L/min</p>";
    html += "  </div>";
    html += "  <div class='card'>";
    html += "    <h2><i class='fas fa-water' style='color: #007bff;'></i> Total Volume</h2>";
    html += "    <p><span id='totalLiters'>0.000</span> L</p>";
    html += "  </div>";
    html += "</div>";
    html += "<div class='container'>";
    html += "  <div class='card' style='grid-column: span 2;'>";
    html += "    <h2><i class='fas fa-chart-line' style='color: #007bff;'></i> Grafik Pemakaian Air</h2>";
    html += "    <canvas id='flowRateChart'></canvas>";
    html += "  </div>";
    html += "</div>";
    html += "<script>";
    html += "const ctx = document.getElementById('flowRateChart').getContext('2d');";
    html += "const chart = new Chart(ctx, {";
    html += "    type: 'line',";
    html += "    data: {";
    html += "        labels: Array(30).fill('').map((_, i) => i + 1),";
    html += "        datasets: [{";
    html += "            label: 'Pemakaian Air (L/min)',";
    html += "            data: Array(30).fill(0),";
    html += "            borderColor: '#007bff',";
    html += "            backgroundColor: 'rgba(0, 123, 255, 0.2)',";
    html += "            fill: true,";
    html += "            tension: 0.3";
    html += "        }]";
    html += "    },";
    html += "    options: { responsive: true }";
    html += "});";
    html += "setInterval(() => {";
    html += "    fetch('/data').then(response => response.json()).then(data => {";
    html += "        document.getElementById('flowRate').innerText = data.flowRate;";
    html += "        document.getElementById('totalLiters').innerText = data.totalLiters;";
    html += "        chart.data.datasets[0].data.shift();";
    html += "        chart.data.datasets[0].data.push(data.flowRate);";
    html += "        chart.update();";
    html += "    });";
    html += "}, 1000);";
    html += "</script>";
    html += "</body>";
    html += "</html>";
    request->send(200, "text/html", html);
  });

  // Endpoint untuk data JSON
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"flowRate\": " + String(flowRate, 3) + ", \"totalLiters\": " + String(totalMilliLitres / 1000.0, 3) + "}";
    request->send(200, "application/json", json);
  });

  // Mulai web server
  server.begin();
}

void loop() {
  unsigned long currentMillis = millis();

  // Hitung flow rate setiap interval
  if (currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;

    byte pulse1Sec = pulseCount;
    pulseCount = 0;

    flowRate = ((1000.0 / interval) * pulse1Sec) / calibrationFactor;
    flowMilliLitres = (flowRate / 60) * 1000;
    totalMilliLitres += flowMilliLitres;

    // Simpan data ke array
    flowData[dataIndex] = flowRate;
    dataIndex = (dataIndex + 1) % 30;  // Hanya simpan 30 data terakhir

    // Debugging
    Serial.print("Flow Rate: ");
    Serial.print(flowRate, 3);
    Serial.print(" L/min\t");

    Serial.print("Total Volume: ");
    Serial.print(totalMilliLitres / 1000.0, 3);
    Serial.println(" L");
  }
}
