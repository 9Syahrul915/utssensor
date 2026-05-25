#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// ============ KONFIGURASI WiFi ============
const char* WIFI_SSID     = "py";
const char* WIFI_PASSWORD  = "11111111";

// ============ KONFIGURASI TELEGRAM (LANGSUNG) ============
const char* TELEGRAM_BOT_TOKEN = "8981733792:AAETc1u-Fq8MBsVDSupV1LWt67O4rk7ZNgg";
const char* TELEGRAM_CHAT_ID   = "7710360988";

// ============ KONFIGURASI GITHUB (OPSIONAL - UNTUK LOG) ============
const bool  ENABLE_GITHUB_LOG = true;  // Set false jika tidak mau log ke GitHub
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
  Serial.println("  Mode: Notifikasi INSTAN ke Telegram");
  Serial.println("==========================================");

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(LED_INDICATOR, OUTPUT);
  digitalWrite(LED_INDICATOR, LOW);

  connectWiFi();

  configTime(gmtOffset, daylightOffset, ntpServer);
  Serial.println("⏰ Sinkronisasi waktu...");
  delay(2000);

  // Kirim pesan startup ke Telegram
  sendTelegramNotification("STARTUP");
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
      Serial.printf("🚨 ANOMALI TERDETEKSI! \n");

      digitalWrite(LED_INDICATOR, HIGH);

      // ⚡ STEP 1: Kirim ke Telegram LANGSUNG (instan!)
      bool telegramOK = sendTelegramNotification("TERDETEKSI");
      if (telegramOK) {
        Serial.println("✅ Telegram: Notifikasi terkirim!");
      } else {
        Serial.println("❌ Telegram: Gagal mengirim!");
      }

      // 📝 STEP 2: Log ke GitHub (background, opsional)
      if (ENABLE_GITHUB_LOG) {
        bool githubOK = sendGitHubLog("TERDETEKSI");
        if (githubOK) {
          Serial.println("✅ GitHub: Log tersimpan.");
        } else {
          Serial.println("⚠️ GitHub: Gagal log (tidak masalah).");
        }
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
//     ⚡ KIRIM NOTIFIKASI LANGSUNG KE TELEGRAM (INSTAN)
// ======================================================
bool sendTelegramNotification(String status) {
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

  // Buat pesan
  String message;

  if (status == "STARTUP") {
    message = "🟢 *SISTEM AKTIF*\\n";
    message += "━━━━━━━━━━━━━━━━━━━━\\n";
    message += "📡 ESP32 Sensor IR telah menyala\\n";
    message += "📍 Lokasi: " + String(SENSOR_LOCATION) + "\\n";
    message += "🕐 Waktu: " + timestamp + "\\n";
    message += "━━━━━━━━━━━━━━━━━━━━\\n";
    message += "✅ Siap menerima deteksi sensor";
  } else {
    message = "🚨 *ALERT: ANOMALI TERDETEKSI!*\\n";
    message += "━━━━━━━━━━━━━━━━━━━━\\n";
    message += "📡 Status: " + status + "\\n";
    message += "📍 Lokasi: " + String(SENSOR_LOCATION) + "\\n";
    message += "🕐 Waktu: " + timestamp + "\\n";
    message += "━━━━━━━━━━━━━━━━━━━━\\n";
    message += "⚠️ Segera cek lokasi sensor!";
  }

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