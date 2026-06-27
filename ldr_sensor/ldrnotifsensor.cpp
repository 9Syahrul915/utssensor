#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "../secrets.h"

// ============ KONFIGURASI WiFi ============
const char* WIFI_SSID     = WIFI_SSID_VAL;
const char* WIFI_PASSWORD  = WIFI_PASSWORD_VAL;

// ============ KONFIGURASI TELEGRAM ============
const char* TELEGRAM_BOT_TOKEN = TELEGRAM_BOT_TOKEN_VAL;
const char* TELEGRAM_CHAT_ID   = TELEGRAM_CHAT_ID_VAL;

// ============ KONFIGURASI GITHUB ============
const bool  ENABLE_GITHUB_LOG = true;
const char* GITHUB_TOKEN   = GITHUB_TOKEN_VAL;
const char* GITHUB_OWNER   = GITHUB_OWNER_VAL;
const char* GITHUB_REPO    = GITHUB_REPO_VAL;

// ============ KONFIGURASI SENSOR LDR ============
#define LDR_PIN         A0  // ESP8266 hanya memiliki satu pin analog (A0)
#define LED_INDICATOR   2   // LED onboard ESP8266 (GPIO2 / D4)
const char* SENSOR_LOCATION  = "Lampu Kamar";

// ============ KONFIGURASI WAKTU ============
const unsigned long REPORT_INTERVAL_MS = 600000;  // 10 menit (600.000 ms)
unsigned long lastReportTime           = 0;
String lastStatus                      = "UNKNOWN";

// ============ NTP TIME ============
const char* ntpServer        = "pool.ntp.org";
const long  gmtOffset        = 28800;  // GMT+8 WITA
const int   daylightOffset   = 0;

// ============ DEKLARASI FUNGSI ============
void connectWiFi();
bool sendTelegramNotification(String status, int adcVal);
bool sendGitHubLog(String status);
String getFormattedTime();
String determineStatus(int adcVal);

// ======================================================
//                        SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n==========================================");
  Serial.println("  ESP8266 Sensor LDR + Telegram + GitHub");
  Serial.println("  Mode: Pemantau Cahaya Kamar (4 Level)");
  Serial.println("==========================================");

  pinMode(LED_INDICATOR, OUTPUT);
  digitalWrite(LED_INDICATOR, HIGH); // On ESP8266, D4 LED is Active LOW (HIGH = Mati)

  connectWiFi();

  // Sinkronisasi Waktu NTP
  configTime(gmtOffset, daylightOffset, ntpServer);
  Serial.print("⏰ Sinkronisasi waktu...");
  time_t now = time(nullptr);
  int retry = 0;
  while (now < 86400 && retry < 30) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    retry++;
  }
  Serial.println(" Selesai!");

  digitalWrite(LED_INDICATOR, HIGH); // Pastikan mati setelah koneksi sukses
  Serial.println("✅ Sistem siap! Memulai pemantauan LDR...\n");
}

// ======================================================
//                      LOOP UTAMA
// ======================================================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Baca nilai analog dari sensor LDR (0 - 1023 pada ESP8266)
  int adcVal = analogRead(LDR_PIN);
  String currentStatus = determineStatus(adcVal);

  // Set LED Indikator: menyala jika GELAP (bahaya), mati jika kondisi aman lainnya
  // Catatan: LED bawaan ESP8266 D4 adalah Active LOW
  if (currentStatus == "Gelap") {
    digitalWrite(LED_INDICATOR, LOW);  // Menyala
  } else {
    digitalWrite(LED_INDICATOR, HIGH); // Mati
  }

  unsigned long now = millis();

  // Deteksi transisi instan ketika kondisi berubah dari aman menjadi GELAP (Bahaya)
  bool statusChangedToDanger = (currentStatus == "Gelap" && lastStatus != "Gelap" && lastStatus != "UNKNOWN");

  if (lastStatus == "UNKNOWN") {
    // Pembacaan awal
    lastStatus = currentStatus;
    Serial.printf("📝 Pembacaan awal LDR: %d (%s)\n", adcVal, currentStatus.c_str());
    if (ENABLE_GITHUB_LOG) {
      sendGitHubLog(currentStatus);
    }
    if (currentStatus == "Gelap") {
      sendTelegramNotification(currentStatus, adcVal);
    }
    lastReportTime = now;
  }
  else if (now - lastReportTime >= REPORT_INTERVAL_MS || statusChangedToDanger) {
    if (statusChangedToDanger) {
      Serial.printf("🚨 TRANSISI DETEKSI: Kamar Gelap! Nilai ADC: %d. Mengirim peringatan instan...\n", adcVal);
    } else {
      Serial.printf("⏰ Laporan berkala LDR (10 menit): %d (%s)\n", adcVal, currentStatus.c_str());
    }

    // Kirim log ke GitHub
    if (ENABLE_GITHUB_LOG) {
      bool githubOK = sendGitHubLog(currentStatus);
      if (githubOK) {
        Serial.printf("✅ GitHub: Log '%s' berhasil dikirim.\n", currentStatus.c_str());
      } else {
        Serial.println("⚠️ GitHub: Gagal mengirim log.");
      }
    }

    // Kirim Telegram hanya jika GELAP (Bahaya)
    if (currentStatus == "Gelap") {
      bool telegramOK = sendTelegramNotification(currentStatus, adcVal);
      if (telegramOK) {
        Serial.println("✅ Telegram: Notifikasi bahaya terkirim!");
      } else {
        Serial.println("❌ Telegram: Gagal mengirim notifikasi.");
      }
    }

    Serial.println();
    lastReportTime = now;
    lastStatus = currentStatus;
  }

  delay(1000); // Periksa sensor setiap 1 detik
}

// ======================================================
//              FUNGSI KLASIFIKASI CAHAYA (ESP8266 Scale 10-bit)
// ======================================================
String determineStatus(int adcVal) {
  // Batasan disesuaikan dari skala 12-bit (0-4095) ke 10-bit (0-1023)
  if (adcVal >= 750) {
    return "Gelap";
  } else if (adcVal >= 375) {
    return "Remang-remang";
  } else if (adcVal >= 200) {
    return "Terang";
  } else {
    return "Sangat Terang";
  }
}

// ======================================================
//         ⚡ KIRIM NOTIFIKASI TELEGRAM (BAHAYA)
// ======================================================
bool sendTelegramNotification(String status, int adcVal) {
  if (status != "Gelap") {
    return false;
  }

  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  HTTPClient http;

  String url = "http://api.telegram.org/bot";
  url += TELEGRAM_BOT_TOKEN;
  url += "/sendMessage";

  http.begin(*client, url);
  http.addHeader("Content-Type", "application/json");

  String timestamp = getFormattedTime();

  // Pesan peringatan lampu kamar mati/gelap (bahaya)
  String message = "🚨 *PERINGATAN: KAMAR GELAP (BAHAYA)!*\\n";
  message += "━━━━━━━━━━━━━━━━━━━━\\n";
  message += "⚠️ Status: GELAP (Bahaya)\\n";
  message += "📊 Nilai ADC LDR: " + String(adcVal) + " (0-1023)\\n";
  message += "📍 Lokasi: " + String(SENSOR_LOCATION) + "\\n";
  message += "🕐 Waktu: " + timestamp + "\\n";
  message += "━━━━━━━━━━━━━━━━━━━━\\n";
  message += "💡 Sensor mendeteksi lampu mati. Harap periksa kamar!";

  // JSON payload
  String payload = "{";
  payload += "\"chat_id\":\"" + String(TELEGRAM_CHAT_ID) + "\",";
  payload += "\"text\":\"" + message + "\",";
  payload += "\"parse_mode\":\"Markdown\"";
  payload += "}";

  int httpCode = http.POST(payload);
  http.end();

  return (httpCode == 200);
}

// ======================================================
//         📝 LOG DATA KE GITHUB DISPATCHES
// ======================================================
bool sendGitHubLog(String status) {
  std::unique_ptr<BearSSL::WiFiClientSecure> client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  HTTPClient http;

  String url = "https://api.github.com/repos/";
  url += GITHUB_OWNER;
  url += "/";
  url += GITHUB_REPO;
  url += "/dispatches";

  http.begin(*client, url);
  http.addHeader("Authorization", String("token ") + GITHUB_TOKEN);
  http.addHeader("Accept", "application/vnd.github.v3+json");
  http.addHeader("User-Agent", "ESP8266-Client");
  http.addHeader("Content-Type", "application/json");

  String timestamp = getFormattedTime();

  String payload = "{";
  payload += "\"event_type\":\"ldr-alert\",";
  payload += "\"client_payload\":{";
  payload += "\"status\":\"" + status + "\",";
  payload += "\"location\":\"" + String(SENSOR_LOCATION) + "\",";
  payload += "\"timestamp\":\"" + timestamp + "\"";
  payload += "}}";

  int httpCode = http.POST(payload);
  http.end();

  return (httpCode == 204);
}

// ======================================================
//              KONEKSI WiFi
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
//              FORMAT WAKTU WIB (ESP8266-compatible)
// ======================================================
String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);
  if (now < 86400) {
    return "Waktu belum sinkron";
  }

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S WIB", timeinfo);
  return String(buffer);
}
