//JSR
#include <WiFi.h>
#include <WebServer.h>

#define RX_PIN 16
#define TX_PIN 17
#define UART_BAUD 9600

const char* AP_SSID = "GardenGenie-AP";
const char* AP_PASS = "gardengenie123";

WebServer server(80);

// thresholds (same logic)
const float TEMP_MIN = 18.0;
const float TEMP_MAX = 25.0;
const int TDS_MIN = 50;
const int TDS_MAX = 200;
const float ON_DISTANCE_CM  = 8.0;
const float OFF_DISTANCE_CM = 4.0;

// telemetry storage
volatile int latestTds = 0;
volatile float latestDist = -1.0;
volatile float latestTemp = NAN;
volatile bool latestPumpOn = false;
String latestPlant = "Unknown";
String latestLevel = "Unknown";

const char* htmlPage = R"rawliteral(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Garden Genie — Dashboard Preview</title>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;700&display=swap" rel="stylesheet">
  <script>
/* Live data mode: fetch real telemetry from /data and update UI + charts */

let labels = [], tdsHistory = [], distHistory = [];

// Chart setup (reuse existing canvas IDs)
const tdsCtx = document.getElementById('tdsChart').getContext('2d');
const distCtx = document.getElementById('distChart').getContext('2d');

const tdsChart = new Chart(tdsCtx,{
  type:'line',
  data:{labels:labels,datasets:[{label:'TDS',data:tdsHistory,fill:true,tension:0.3}]},
  options:{animation:false,plugins:{legend:{display:false}},scales:{y:{beginAtZero:true}}}
});

const distChart = new Chart(distCtx,{
  type:'line',
  data:{labels:labels,datasets:[{label:'Distance',data:distHistory,fill:true,tension:0.3}]},
  options:{animation:false,plugins:{legend:{display:false}},scales:{y:{beginAtZero:true}}}
});

async function fetchAndRender(){
  try {
    const res = await fetch('/data', {cache:'no-store'});
    if (!res.ok) throw new Error('HTTP ' + res.status);
    const j = await res.json();

    // read values (guard for null)
    const tds = (typeof j.tds === 'number') ? j.tds : 0;
    const dist = (typeof j.dist === 'number') ? j.dist : null;
    const temp = (j.temp === null || typeof j.temp !== 'number') ? null : j.temp;
    const pump = !!j.pump;
    const plant = j.plant || '--';
    const level = j.level || '--';

    // update numeric UI
    document.getElementById("tdsVal").textContent = tds + " ppm";
    document.getElementById("distanceVal").textContent = (dist===null ? '--' : dist.toFixed(2)) + " cm";
    document.getElementById("tempVal").textContent = (temp===null ? '--' : temp.toFixed(1)) + " °C";
    document.getElementById("pumpText").textContent = pump ? "ON" : "OFF";
    document.getElementById("motorDot").classList.toggle("on", pump);
    document.getElementById("levelText").textContent = level;
    document.getElementById("lastUpdate").textContent = new Date().toLocaleTimeString();
    document.getElementById("pumpText").style.color = pump ? 'var(--accent)' : '#b22222';

    // update charts
    const now = new Date().toLocaleTimeString();
    labels.push(now); if (labels.length>30) labels.shift();

    tdsHistory.push(tds); if (tdsHistory.length>30) tdsHistory.shift();
    distHistory.push(dist===null ? 0 : dist); if (distHistory.length>30) distHistory.shift();

    tdsChart.update();
    distChart.update();

  } catch (err) {
    console.warn('fetch error', err);
    // show offline hint
    document.getElementById("lastUpdate").textContent = 'offline';
  }
}

fetchAndRender();
setInterval(fetchAndRender, 1000);
</script>

  <style>
    :root{
      --bg:#071022; --muted:#94a3b8;
      --accent:#10b981; --accent2:#06b6d4; --danger:#ef4444;
      font-family:'Inter',system-ui;
    }
    body{margin:0;background:linear-gradient(180deg,#071022,#05101b);color:#e6eef7}
    .wrap{max-width:780px;margin:auto;padding:14px}
    header{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}
    .brand{display:flex;gap:10px;align-items:center}
    .logo{width:48px;height:48px;border-radius:12px;background:linear-gradient(135deg,var(--accent),var(--accent2));
          display:flex;align-items:center;justify-content:center;font-weight:800;color:#022}
    h1{margin:0;font-size:1.1rem}
    .sub{color:var(--muted);font-size:.8rem}
    .grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
    .card{background:rgba(255,255,255,0.03);padding:12px;border-radius:12px}
    .big{grid-column:1/-1;display:flex;gap:12px;align-items:center}
    .reading{flex:1}
    .label{color:var(--muted);font-size:.85rem}
    .value{font-size:1.6rem;font-weight:800;margin-top:6px}
    .small{color:var(--muted);font-size:.8rem}
    .dot{width:44px;height:44px;border-radius:10px;background:rgba(255,255,255,0.1)}
    .on{background:linear-gradient(135deg,var(--accent),var(--accent2))!important}
    .chart-wrap{height:170px;padding:6px}
    footer{text-align:center;color:var(--muted);margin-top:12px;font-size:.8rem}
  </style>
</head>
<body>
  <div class="wrap">

    <header>
      <div class="brand">
        <div class="logo">GG</div>
        <div>
          <h1>Garden Genie</h1>
          <div class="sub">Preview with demo data</div>
        </div>
      </div>
      <div id="lastUpdate" class="sub">—</div>
    </header>

    <section class="grid">

      <div class="card big">
        <div style="width:120px;display:flex;flex-direction:column;align-items:center">
          <div id="motorDot" class="dot"></div>
          <div class="small" style="margin-top:8px;text-align:center">Pump</div>
        </div>

        <div class="reading">
          <div class="label">Water Distance</div>
          <div id="distanceVal" class="value">-- cm</div>
          <div class="small">Thresholds: Low > 4.5 cm, Stop < 10 cm</div>
        </div>

        <div class="reading">
          <div class="label">TDS</div>
          <div id="tdsVal" class="value">-- ppm</div>
          <div class="small">Water mineral content</div>
        </div>

        <div class="reading">
          <div class="label">Temperature</div>
          <div id="tempVal" class="value">-- °C</div>
        </div>
      </div>

      <div class="card">
        <div class="label">Water Level</div>
        <div id="levelText" style="margin-top:10px;font-weight:700;font-size:1.05rem">—</div>
      </div>

      <div class="card">
        <div class="label">Pump Status</div>
        <div id="pumpText" style="margin-top:10px;font-weight:700;font-size:1.05rem">—</div>
      </div>

      <div class="card">
        <div class="label">TDS Trend</div>
        <div class="chart-wrap"><canvas id="tdsChart"></canvas></div>
      </div>

      <div class="card">
        <div class="label">Distance Trend</div>
        <div class="chart-wrap"><canvas id="distChart"></canvas></div>
      </div>

    </section>

    <footer>Preview mode uses demo sensor readings.</footer>
  </div>

  <script>
    // ----- MOCK DATA PREVIEW MODE -----
    let labels = [], tdsHistory = [], distHistory = [];

    const tdsCtx = document.getElementById('tdsChart').getContext('2d');
    const distCtx = document.getElementById('distChart').getContext('2d');

    const tdsChart = new Chart(tdsCtx,{
      type:'line',
      data:{labels:labels,datasets:[{label:'TDS',data:tdsHistory,fill:true,tension:0.3}]},
      options:{animation:false,plugins:{legend:{display:false}}}
    });

    const distChart = new Chart(distCtx,{
      type:'line',
      data:{labels:labels,datasets:[{label:'Distance',data:distHistory,fill:true,tension:0.3}]},
      options:{animation:false,plugins:{legend:{display:false}}}
    });

    function mockData(){
      return {
        tds: Math.round(250 + Math.random()*150),
        dist: +(5 + Math.random()*10).toFixed(2),
        temp: +(22 + Math.random()*5).toFixed(1),
        motor: Math.random() > 0.5
      };
    }

    function updateUI(){
      const d = mockData();

      // labels
      const now = new Date().toLocaleTimeString();
      labels.push(now); if(labels.length>20) labels.shift();

      tdsHistory.push(d.tds); if(tdsHistory.length>20) tdsHistory.shift();
      distHistory.push(d.dist); if(distHistory.length>20) distHistory.shift();

      document.getElementById("tdsVal").textContent = d.tds + " ppm";
      document.getElementById("distanceVal").textContent = d.dist + " cm";
      document.getElementById("tempVal").textContent = d.temp + " °C";

      document.getElementById("pumpText").textContent = d.motor ? "ON" : "OFF";
      document.getElementById("motorDot").classList.toggle("on", d.motor);

      const lvl = d.dist > 10 ? "Low" : "OK";
      document.getElementById("levelText").textContent = lvl;

      document.getElementById("lastUpdate").textContent = now;

      tdsChart.update();
      distChart.update();
    }

    setInterval(updateUI, 1500);
    updateUI();
  </script>
</body>
</html>

)rawliteral";

void handleRoot(){ server.send(200, "text/html", htmlPage); }

void handleData(){
  String s = "{";
  s += "\"tds\":" + String(latestTds) + ",";
  s += "\"dist\":" + String(latestDist, 2) + ",";
  if (isnan(latestTemp)) s += "\"temp\":null,"; else s += "\"temp\":" + String(latestTemp, 1) + ",";
  s += "\"pump\":" + String(latestPumpOn ? "true" : "false") + ",";
  s += "\"plant\":\"" + latestPlant + "\",";
  s += "\"level\":\"" + latestLevel + "\"";
  s += "}";
  server.send(200, "application/json", s);
}

String readLineFromSerial2() {
  static String line = "";
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\n') {
      String tmp = line; line = ""; return tmp;
    } else if (c == '\r') {
      // ignore
    } else {
      line += c;
      if (line.length() > 400) { line = ""; return ""; }
    }
  }
  return "";
}

void parseTelemetryLine(const String &ln) {
  int idx;
  idx = ln.indexOf("TDS:");
  if (idx >= 0) { int s = idx+4; int e = ln.indexOf(';', s); if (e < 0) e = ln.length(); latestTds = ln.substring(s,e).toInt(); }
  idx = ln.indexOf("DIST:");
  if (idx >= 0) { int s = idx+5; int e = ln.indexOf(';', s); if (e < 0) e = ln.length(); latestDist = ln.substring(s,e).toFloat(); }
  idx = ln.indexOf("PUMP:");
  if (idx >= 0) { int s = idx+5; int e = ln.indexOf(';', s); if (e < 0) e = ln.length(); String p = ln.substring(s,e); latestPumpOn = (p == "ON"); }
  idx = ln.indexOf("TEMP:");
  if (idx >= 0) { int s = idx+5; int e = ln.indexOf(';', s); if (e < 0) e = ln.length(); String t = ln.substring(s,e); if (t == "NAN" || t.length()==0) latestTemp = NAN; else latestTemp = t.toFloat(); }

  // derived
  bool tempOK = (!isnan(latestTemp) && latestTemp >= TEMP_MIN && latestTemp <= TEMP_MAX);
  bool tdsOK = (latestTds >= TDS_MIN && latestTds <= TDS_MAX);
  latestPlant = (tempOK && tdsOK) ? "Happy" : "Sad";

  if (latestDist < 0) latestLevel = "Unknown";
  else if (latestDist > ON_DISTANCE_CM) latestLevel = latestPumpOn ? "Auto Refilling" : "Water Level LOW";
  else latestLevel = "Water Level NORMAL";
}

void setup(){
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.println("Serial2 started on RX=16");

  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP: "); Serial.println(IP);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP server started");
}

void loop(){
  server.handleClient();
  String ln = readLineFromSerial2();
  if (ln.length() > 0) {
    ln.trim();
    if (ln.length() > 0) {
      Serial.print("RECV: "); Serial.println(ln);
      parseTelemetryLine(ln);
    }
  }
  delay(10);
}
