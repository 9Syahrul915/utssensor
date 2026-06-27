#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// ============ KONFIGURASI WiFi ============
const char* WIFI_SSID     = "Home
const char* WIFI_PASSWORD  = "";

// ============ KONFIGURASI TELEGRAM (LANGSUNG) ============
const char* TELEGRAM_BOT_TOKEN = "";
const char* TELEGRAM_CHAT_ID   = "";

// ============ KONFIGURASI GITHUB (OPSIONAL - UNTUK LOG) ============
const bool  ENABLE_GITHUB_LOG = true;  // Set false jika tidak mau log ke GitHub
const char* GITHUB_TOKEN   = "";
const char* GITHUB_OWNER   = "9Syahrul915";
const char* GITHUB_REPO    = "utssensor";

// ============ KONFIGURASI SENSOR ============
#define IR_SENSOR_PIN   14
#define LED_INDICATOR   2
const char* SENSOR_LOCATION  = "Brankas Uang";

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
bool sendTelegramNotification(String status);
bool sendGitHubLog(String status);
String getFormattedTime();

// ======================================================
//                        SETUP
// ======================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n==========================================");
  Serial.println("  ESP32 Sensor IR + Telegram + GitHub");
  Serial.println("  Mode: Pemantau Keamanan Brankas Uang");
  Serial.println("==========================================");

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(LED_INDICATOR, OUTPUT);
  digitalWrite(LED_INDICATOR, LOW);

  connectWiFi();

  configTime(gmtOffset, daylightOffset, ntpServer);
  Serial.println("⏰ Sinkronisasi waktu...");
  delay(2000);

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
  String currentStatus = (sensorValue == LOW) ? "AMAN" : "BAHAYA";

  // Set LED Indikator: menyala jika BAHAYA (brankas terbuka), mati jika AMAN (tertutup)
  if (currentStatus == "BAHAYA") {
    digitalWrite(LED_INDICATOR, HIGH);
  } else {
    digitalWrite(LED_INDICATOR, LOW);
  }

  unsigned long now = millis();

  // Deteksi transisi instan dari AMAN ke BAHAYA (pintu dibuka tiba-tiba)
  bool statusChangedToDanger = (currentStatus == "BAHAYA" && lastStatus == "AMAN");

  if (lastStatus == "UNKNOWN") {
    // Inisialisasi status awal pada pembacaan pertama
    lastStatus = currentStatus;
    Serial.printf("📝 Pembacaan awal: %s\n", currentStatus.c_str());
    if (ENABLE_GITHUB_LOG) {
      sendGitHubLog(currentStatus);
    }
    if (currentStatus == "BAHAYA") {
      sendTelegramNotification(currentStatus);
    }
    lastReportTime = now;
  }
  else if (now - lastReportTime >= REPORT_INTERVAL_MS || statusChangedToDanger) {
    if (statusChangedToDanger) {
      Serial.println("🚨 TRANSISI DETEKSI: Brankas Terbuka! Mengirim peringatan instan...");
    } else {
      Serial.printf("⏰ Laporan berkala (10 menit): Brankas %s\n", currentStatus.c_str());
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

    // Kirim Telegram hanya jika BAHAYA
    if (currentStatus == "BAHAYA") {
      bool telegramOK = sendTelegramNotification(currentStatus);
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
//     ⚡ KIRIM NOTIFIKASI LANGSUNG KE TELEGRAM (INSTAN)
// ======================================================
bool sendTelegramNotification(String status) {
  if (status != "BAHAYA") {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  String url = "https://api.telegram.org/bot";
  url += TELEGRAM_BOT_TOKEN;
  url += "/sendMessage";

  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  // Dapatkan waktu
  String timestamp = getFormattedTime();

  // Buat pesan khusus brankas terbuka
  String message = "🚨 *PERINGATAN: BRANKAS TERBUKA!*\\n";
  message += "━━━━━━━━━━━━━━━━━━━━\\n";
  message += "⚠️ Status: BAHAYA (Pintu Terbuka)\\n";
  message += "📍 Lokasi: " + String(SENSOR_LOCATION) + "\\n";
  message += "🕐 Waktu: " + timestamp + "\\n";
  message += "━━━━━━━━━━━━━━━━━━━━\\n";
  message += "🔴 Segera periksa brankas uang Anda!";

  // JSON payload
  String payload = "{";
  payload += "\"chat_id\":\"" + String(TELEGRAM_CHAT_ID) + "\",";
  payload += "\"text\":\"" + message + "\",";
  payload += "\"parse_mode\":\"Markdown\"";
  payload += "}";

  int httpCode = http.POST(payload);
  String response = http.getString();

  Serial.printf("📬 Telegram Response: %d\n", httpCode);

  http.end();

  return (httpCode == 200);
}

// ======================================================
//         📝 LOG DATA KE GITHUB (OPSIONAL)
// ======================================================
bool sendGitHubLog(String status) {
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

  int httpCode = http.POST(payload);
  http.end();

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
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S WIB", &timeinfo);
  return String(buffer);
}
