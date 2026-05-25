/*
 * =====================================================
 *  ESP32 + Sensor Infrared + GitHub Actions + Telegram
 * =====================================================
 *  Alur: Sensor IR → ESP32 → GitHub API → GitHub Actions → Telegram
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ============ KONFIGURASI WiFi ============
const char* WIFI_SSID     = "py";
const char* WIFI_PASSWORD  = "11111111";

// ============ KONFIGURASI GITHUB ============
const char* GITHUB_TOKEN   = "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";
const char* GITHUB_OWNER   = "9Syahrul915";
const char* GITHUB_REPO    = "utssensor";

// ============ KONFIGURASI SENSOR ============
#define IR_SENSOR_PIN   14        // Pin sensor infrared (sesuaikan)
#define LED_INDICATOR   2         // LED bawaan ESP32
const char* SENSOR_LOCATION = "Gerbang masuk";  // Lokasi sensor

// ============ KONFIGURASI WAKTU ============
const long  COOLDOWN_MS    = 3000;   // Jeda minimal 30 detik antar notifikasi
unsigned long lastAlertTime = 0;

// ============ NTP TIME ============
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset = 28800;  // GMT+8
const int   daylightOffset = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\n🚀 ESP32 Sensor Infrared + GitHub + Telegram");
  Serial.println("============================================");

  // Setup pin
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(LED_INDICATOR, OUTPUT);
  digitalWrite(LED_INDICATOR, LOW);

  // Koneksi WiFi
  connectWiFi();

  // Setup NTP (waktu)
  configTime(gmtOffset, daylightOffset, ntpServer);
  Serial.println("⏰ Sinkronisasi waktu NTP...");
  delay(2000);

  Serial.println("✅ Sistem siap! Menunggu deteksi sensor...\n");
}

void loop() {
  // Pastikan WiFi tetap terhubung
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Baca sensor infrared
  int sensorValue = digitalRead(IR_SENSOR_PIN);

  if (sensorValue == LOW) {  // LOW = objek terdeteksi (tergantung sensor)
    unsigned long now = millis();
    
    // Cek cooldown agar tidak spam notifikasi
    if (now - lastAlertTime > COOLDOWN_MS) {
      Serial.println("🚨 OBJEK TERDETEKSI!");
      
      // Nyalakan LED indikator
      digitalWrite(LED_INDICATOR, HIGH);
      
      // Kirim alert ke GitHub → Telegram
      bool success = sendGitHubAlert("TERDETEKSI");
      
      if (success) {
        Serial.println("✅ Alert berhasil dikirim ke GitHub!");
        Serial.println("📱 Notifikasi Telegram akan segera muncul...\n");
        lastAlertTime = now;
      } else {
        Serial.println("❌ Gagal mengirim alert!\n");
      }
      
      // Matikan LED setelah beberapa detik
      delay(3000);
      digitalWrite(LED_INDICATOR, LOW);
    } else {
      long remaining = (COOLDOWN_MS - (now - lastAlertTime)) / 1000;
      Serial.printf("⏳ Cooldown aktif. Tunggu %ld detik lagi.\n", remaining);
      delay(1000);
    }
  }

  delay(200);  // Delay pembacaan sensor
}

// ============ FUNGSI KONEKSI WiFi ============
void connectWiFi() {
  Serial.printf("📶 Menghubungkan ke WiFi: %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 30) {
    delay(500);
    Serial.print(".");
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf