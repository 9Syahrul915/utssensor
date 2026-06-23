#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// ============ KONFIGURASI WiFi ============
const char* WIFI_SSID     = "py";
const char* WIFI_PASSWORD  = "11111111";

// ============ KONFIGURASI GITHUB ============
const char* GITHUB_TOKEN   = "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";
const char* GITHUB_OWNER   = "9Syahrul915";
const char* GITHUB_REPO    = "utssensor";

// ============ KONFIGURASI SENSOR ============
#define IR_SENSOR_PIN   14
#define LED_INDICATOR   2
const char* SENSOR_LOCATION  = "Gerbang Depan";

// ============ KONFIGURASI WAKTU ============
const long  COOLDOWN_MS       = 1;  // 1 detik cooldown
unsigned long lastAlertTime   = 0;

// ============ NTP TIME ============
const char* ntpServer        = "pool.ntp.org";
const long  gmtOffset        = 28800;  // GMT+8 WITA
const int   daylightOffset   = 0;

// ============ DEKLARASI FUNGSI ============
void connectWiFi();
bool sendGitHubDispatch(String status);
String getFormattedTime();


// ======================================================
//                        SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n==========================================");
  Serial.println("  ESP32 Sensor IR + WhatsApp (Fonnte)");
  Serial.println("  Mode: GitHub Dispatch -> Fonnte WA");
  Serial.println("==========================================");
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(LED_INDICATOR, OUTPUT);
  digitalWrite(LED_INDICATOR, LOW);
  connectWiFi();
  configTime(gmtOffset, daylightOffset, ntpServer);
  Serial.println("⏰ Sinkronisasi waktu...");
  delay(2000);


  // Kirim notifikasi startup via GitHub -> WhatsApp
  bool startupOK = sendGitHubDispatch("STARTUP");
  if (startupOK) {
    Serial.println("✅ Startup dispatch terkirim ke GitHub!");
  } else {
    Serial.println("⚠️ Gagal kirim startup dispatch.");
  }
  Serial.println("✅ Sistem siap! Menunggu deteksi sensor...\n");
}


// ======================================================
//                      LOOP UTAMA
// ======================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  int sensorValue = digitalRead(IR_SENSOR_PIN);
  if (sensorValue == LOW) {
    unsigned long now = millis();
    if (now - lastAlertTime > COOLDOWN_MS) {
      Serial.println("🚨 ANOMALI TERDETEKSI!");
      digitalWrite(LED_INDICATOR, HIGH);
      // ⚡ Kirim dispatch ke GitHub -> GitHub Actions -> Fonnte -> WhatsApp
      bool dispatchOK = sendGitHubDispatch("TERDETEKSI");
      if (dispatchOK) {
        Serial.println("✅ GitHub Dispatch terkirim! WA akan dikirim via Fonnte.");
      } else {
        Serial.println("❌ Gagal mengirim GitHub Dispatch!");
      }
      Serial.println();
      lastAlertTime = now;
      delay(2000);
      digitalWrite(LED_INDICATOR, LOW);
    } else {
      long remaining = (COOLDOWN_MS - (now - lastAlertTime)) / 1000;
      Serial.printf("⏳ Cooldown: %ld detik lagi\n", remaining);
      delay(1000);
    }
  }
  delay(200);
}

// ======================================================
//    KIRIM DISPATCH KE GITHUB (TRIGGER GITHUB ACTIONS)
// ======================================================
bool sendGitHubDispatch(String status) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.github.com/repos/";
  url += GITHUB_OWNER;
  url += "/";
  url += GITHUB_REPO;
  url += "/dispatches";

  http.begin(client, url);
  http.addHeader("Authorization", String("token ") + GITHUB_TOKEN);
  http.addHeader("Accept", "application/vnd.github.v3+json");
  http.addHeader("Content-Type", "application/json");
  String timestamp = getFormattedTime();
  String payload = "{";
  payload += "\"event_type\":\"sensor-alert\",";
  payload += "\"client_payload\":{";
  payload += "\"status\":\"" + status + "\",";
  payload += "\"location\":\"" + String(SENSOR_LOCATION) + "\",";
  payload += "\"timestamp\":\"" + timestamp + "\"";
  payload += "}}";

  Serial.printf("📤 Mengirim dispatch ke GitHub...\n");
  Serial.printf("   URL: %s\n", url.c_str());
  int httpCode = http.POST(payload);
  String response = http.getString();
  Serial.printf("📬 GitHub Response: %d\n", httpCode);
  if (httpCode != 204) {
    Serial.printf("   Response: %s\n", response.c_str());
  }
  http.end();
  // GitHub dispatch mengembalikan 204 No Content jika sukses
  return (httpCode == 204);
}


// ======================================================
//              FUNGSI KONEKSI WiFi
// ======================================================
void connectWiFi() {
  Serial.printf("📶 Menghubungkan ke WiFi: %s ", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 30) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" TERHUBUNG!");
    Serial.printf("📍 IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println(" GAGAL!");
    Serial.println("⚠️ Restart dalam 5 detik...");
    delay(5000);
    ESP.restart();
  }
}
// ======================================================
//          FUNGSI DAPATKAN WAKTU FORMATTED
// ======================================================
String getFormattedTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Waktu tidak tersedia";
  }
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S WITA", &timeinfo);
  return String(buffer);
}
