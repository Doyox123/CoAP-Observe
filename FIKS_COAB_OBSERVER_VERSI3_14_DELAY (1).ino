#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <time.h>  

// ================= WiFi Config =================
const char* ssid = "iPhone";
const char* password = "hhhhhhh12";

// ================= CoAP Server =================
IPAddress server_ip(202, 10, 47, 135); 
int port = 5683;
const char* resource_observe_put = "sensor/observe";

// ================= Float Switch Pins =================
#define FS1 4
#define FS2 16
#define FS3 14
#define FS4 27
#define FS5 26

// ================= INA219 Sensor =================
Adafruit_INA219 ina219;

// ================= CoAP Setup =================
WiFiUDP Udp;
Coap coap(Udp);

// Variabel
const float POWER_TOLERANCE = 0.05;
bool pendingResponse = false;

int currentLevel = 0;
int previousLevel = -1;
float power_W = 0;

// ================= Callback Response =================
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[CoAP Response]");
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = '\0';
  Serial.print("Respon: ");
  Serial.println(p);
  pendingResponse = false;
}

// ================= NTP: Ambil UNIX Time dalam ms =================
unsigned long long getUnixTimeMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  unsigned long long millisec = (unsigned long long)tv.tv_sec * 1000LL + (tv.tv_usec / 1000);
  return millisec;
}

// ================= Baca Float Switch =================
void bacaLevelAir() {
  if (digitalRead(FS5) == HIGH) currentLevel = 50;
  else if (digitalRead(FS4) == HIGH) currentLevel = 40;
  else if (digitalRead(FS3) == HIGH) currentLevel = 30;
  else if (digitalRead(FS2) == HIGH) currentLevel = 20;
  else if (digitalRead(FS1) == HIGH) currentLevel = 10;
  else currentLevel = 0;
}

// ================= Baca INA219 =================
void bacaDataINA219() {
  float shuntvoltage = ina219.getShuntVoltage_mV();
  float busvoltage   = ina219.getBusVoltage_V();
  float current_mA   = ina219.getCurrent_mA();
  float loadvoltage  = busvoltage + (shuntvoltage / 1000);
  power_W = (loadvoltage * current_mA) / 1000.0;

  if (isnan(power_W) || isinf(power_W)) power_W = 0;
}

// ================= Kirim Data =================
void kirimDataKeServer() {
  if (pendingResponse) return;

  unsigned long long t_send = getUnixTimeMs();   // <===== timestamp ms

  String payload = "{\"distance\":" + String(currentLevel) +
                   ",\"power\":" + String(power_W, 3) +
                   ",\"t_send\":" + String((unsigned long long)t_send) + "}";

  Serial.print("Kirim: ");
  Serial.println(payload);

  coap.put(server_ip, port, resource_observe_put, payload.c_str());
  pendingResponse = true;

  previousLevel = currentLevel;
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);

  // Float Switch pin
  pinMode(FS1, INPUT_PULLUP);
  pinMode(FS2, INPUT_PULLUP);
  pinMode(FS3, INPUT_PULLUP);
  pinMode(FS4, INPUT_PULLUP);
  pinMode(FS5, INPUT_PULLUP);

  // INA219
  Wire.begin();
  ina219.begin();
  ina219.setCalibration_32V_2A();

  // WiFi Connect
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // ========== NTP Time Synchronization ==========
  configTime(7 * 3600, 0, "pool.ntp.org", "time.google.com");  // GMT+7
  Serial.println("Menunggu sinkron waktu...");
  delay(2000);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.println("NTP Time Synchronized:");
    Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S");
  } else {
    Serial.println("Gagal sinkron NTP!");
  }

  // CoAP
  coap.response(callback_response);
  coap.start();
}

// ================= Loop =================
void loop() {
  bacaLevelAir();
  bacaDataINA219();

  bool levelBerubah = (currentLevel != previousLevel);

  if (!pendingResponse && levelBerubah) {
    kirimDataKeServer();
  }

  coap.loop();
  delay(100);
}
