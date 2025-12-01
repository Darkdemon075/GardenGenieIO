#include <Wire.h> //TDS Module Library
#include <LiquidCrystal_I2C.h> //LCD Module Library
#include <OneWire.h> //Temperature Input Module Library
#include <DallasTemperature.h> // Temperature Output Module Library

// pins
const int TRIG_PIN = 10; //USC Input
const int ECHO_PIN = 11; //USC Output
const int RELAY_PIN = 7; //Motor State Switch
const int TDS_PIN = A0; //TDS Reader
#define ONE_WIRE_BUS 4 //Temperature Reader 

// LCD
#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// temp sensor
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// settings
const bool RELAY_ACTIVE_LOW = true;
const float ON_DISTANCE_CM  = 8.0;  // pump ON when distance > 8cm (low water)
const float OFF_DISTANCE_CM = 4.0;  // pump OFF when distance < 4cm (filled)

//TDS setup
const int TDS_SAMPLES = 5;
const float VREF = 5.0;
const int DRY_THRESHOLD_ADC = 8;
float CALIB_FACTOR = 0.5;

// LCD timing
const unsigned long SCREEN_A_MS = 6500UL;
const unsigned long SCREEN_B_MS = 4500UL;
unsigned long screenStart = 0;
int screenPhase = 0; // 0=A,1=B

// state
bool pumpOn = false;
int latestTds = 0;
float latestDist = -1.0;
float latestTemp = NAN;

void setup() {
  Serial.begin(9600); // telemetry to ESP32
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);

  sensors.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Garden Genie");
  lcd.setCursor(0,1); lcd.print("Live Mode");
  delay(900);
  lcd.clear();

  screenStart = millis();
  screenPhase = 0;
}

void loop() {
  latestDist = readDistanceCM();
  latestTds = readTdsPpmRounded();
  latestTemp = readTemperatureC();

  // pump control simple hysteresis
  if (latestDist >= 0) {
    if (!pumpOn && latestDist > ON_DISTANCE_CM) {
      setRelay(true); pumpOn = true;
    } else if (pumpOn && latestDist < OFF_DISTANCE_CM) {
      setRelay(false); pumpOn = false;
    }
  }

  // send telemetry (newline terminated)
  Serial.print("TDS:"); Serial.print(latestTds);
  Serial.print(";DIST:");
  if (latestDist < 0) Serial.print("NAN"); else Serial.print(latestDist, 2);
  Serial.print(";PUMP:"); Serial.print(pumpOn ? "ON" : "OFF");
  Serial.print(";TEMP:");
  if (isnan(latestTemp)) Serial.print("NAN"); else Serial.print(latestTemp, 1);
  Serial.println();

  // LCD screens
  unsigned long now = millis();
  if (screenPhase == 0) {
    // Screen A
    String line1 = (isPlantHealthy(latestTemp, latestTds) ? "Plant is Happy" : "Plant is Sad");
    String line2 = waterLevelText(latestDist, pumpOn);
    showTwoLines(line1, line2);
    if (now - screenStart >= SCREEN_A_MS) { screenPhase = 1; screenStart = now; }
  } else {
    // Screen B
    String l1 = "Water Quality:" + String(latestTds);
    String l2 = "Water Temp:";
    if (isnan(latestTemp)) l2 += " --C"; else l2 += " " + String(latestTemp, 1) + "C";
    showTwoLines(l1, l2);
    if (now - screenStart >= SCREEN_B_MS) { screenPhase = 0; screenStart = now; }
  }

  delay(500); // adjust responsiveness
}

// ---------- helpers ----------

bool isPlantHealthy(float tempC, int tds) {
  if (isnan(tempC)) return false;
  bool tempOK = (tempC >= 18.0 && tempC <= 25.0);
  bool tdsOK = (tds >= 50 && tds <= 200);
  return tempOK && tdsOK;
}

String waterLevelText(float dist, bool pump) {
  if (dist < 0) return "Water Lvl: N/A";
  if (dist > ON_DISTANCE_CM) {
    return pump ? "Auto Refilling" : "Water Lvl LOW";
  } else {
    return "Water Lvl NML";
  }
}

void showTwoLines(const String &aRaw, const String &bRaw) {
  String a = aRaw; String b = bRaw;
  if (a.length() > LCD_COLS) a = a.substring(0, LCD_COLS);
  while (a.length() < LCD_COLS) a += ' ';
  if (b.length() > LCD_COLS) b = b.substring(0, LCD_COLS);
  while (b.length() < LCD_COLS) b += ' ';
  lcd.setCursor(0,0); lcd.print(a);
  lcd.setCursor(0,1); lcd.print(b);
}

float readDistanceCM() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (duration == 0) return -1.0;
  return duration / 58.0;
}

void setRelay(bool on) {
  if (RELAY_ACTIVE_LOW) digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  else digitalWrite(RELAY_PIN, on ? HIGH : LOW);
}

int readTdsPpmRounded() {
  long sum = 0;
  for (int i = 0; i < TDS_SAMPLES; ++i) { sum += analogRead(TDS_PIN); delay(6); }
  float analogAvg = sum / (float)TDS_SAMPLES;
  if (analogAvg <= DRY_THRESHOLD_ADC) return 0;
  float voltage = analogAvg * (VREF / 1024.0);
  float tds = (133.42 * pow(voltage, 3) - 255.86 * pow(voltage, 2) + 857.39 * voltage) * CALIB_FACTOR;
  if (tds < 0) tds = 0;
  return (int)(tds + 0.5);
}

float readTemperatureC() {
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C) return NAN;
  return t;
}


//thank you namaste jai hind