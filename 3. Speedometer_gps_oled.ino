// ======================================================================
// BAGIAN 1: PENGATURAN LIBRARY DAN DEKLARASI GLOBAL
// ======================================================================

// Library untuk OLED
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Library untuk GPS
#include <SoftwareSerial.h>

// --- Pengaturan OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ADDRESS 0x3C  // Alamat umum, ganti ke 0x3D jika punya Anda berbeda
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Pengaturan GPS ---
SoftwareSerial gpsSerial(4, 5);  // RX, TX

// Struktur untuk menampung data mentah dari payload UBX-NAV-PVT
struct GpsData {
  uint32_t iTOW;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint8_t valid;
  uint32_t tAcc;
  int32_t nano;
  uint8_t fixType;
  uint8_t flags;
  uint8_t flags2;
  uint8_t numSV;
  int32_t lon;
  int32_t lat;
  int32_t height;
  int32_t hMSL;
  uint32_t hAcc;
  uint32_t vAcc;
  int32_t velN;
  int32_t velE;
  int32_t velD;
  int32_t gSpeed;
  int32_t headMot;
  uint32_t sAcc;
  uint32_t headAcc;
  uint16_t pDOP;
  uint8_t flags3[2];
  uint8_t reserved0[4];
  int32_t headVeh;
  int16_t magDec;
  uint16_t magAcc;
};


GpsData gpsData;  // Variabel global untuk menampung data GPS

// Variabel untuk State Machine parser GPS
enum GpsState {
  STATE_WAIT_SYNC1,
  STATE_WAIT_SYNC2,
  STATE_WAIT_CLASS,
  STATE_WAIT_ID,
  STATE_WAIT_LEN1,
  STATE_WAIT_LEN2,
  STATE_PAYLOAD,
  STATE_CHECKSUM
};

GpsState currentState = STATE_WAIT_SYNC1;


// ======================================================================
// BAGIAN 2: FUNGSI-FUNGSI HELPER
// ======================================================================

// --- Fungsi Helper untuk OLED ---
void drawCenteredText(const String &text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

// --- Fungsi Helper untuk GPS ---
bool parseUbx() {
  uint8_t msgClass, msgId;
  uint16_t msgLen;
  uint16_t payloadCounter;
  uint8_t ck_a, ck_b;

  while (gpsSerial.available() > 0) {
    byte b = gpsSerial.read();

    switch (currentState) {
      case STATE_WAIT_SYNC1:
        if (b == 0xB5) currentState = STATE_WAIT_SYNC2;
        break;

      case STATE_WAIT_SYNC2:
        if (b == 0x62) currentState = STATE_WAIT_CLASS;
        else currentState = STATE_WAIT_SYNC1;
        break;

      case STATE_WAIT_CLASS:
        msgClass = b;
        ck_a = 0;
        ck_b = 0;  // Reset checksum
        ck_a += b;
        ck_b += ck_a;
        currentState = STATE_WAIT_ID;
        break;

      case STATE_WAIT_ID:
        msgId = b;
        ck_a += b;
        ck_b += ck_a;
        currentState = STATE_WAIT_LEN1;
        break;

      case STATE_WAIT_LEN1:
        msgLen = b;
        ck_a += b;
        ck_b += ck_a;
        currentState = STATE_WAIT_LEN2;
        break;

      case STATE_WAIT_LEN2:
        msgLen |= (uint16_t)b << 8;
        ck_a += b;
        ck_b += ck_a;

        // Hanya proses jika ini pesan NAV-PVT
        if (msgClass == 0x01 && msgId == 0x07 && msgLen == sizeof(GpsData)) {
          currentState = STATE_PAYLOAD;
          payloadCounter = 0;
        } else {
          currentState = STATE_WAIT_SYNC1;  // Bukan pesan yg kita mau, reset
        }
        break;

      case STATE_PAYLOAD:
        ((byte *)&gpsData)[payloadCounter] = b;  // Isi struct byte per byte
        ck_a += b;
        ck_b += ck_a;
        payloadCounter++;
        if (payloadCounter >= msgLen) {
          currentState = STATE_CHECKSUM;
        }
        break;

      case STATE_CHECKSUM:
        currentState = STATE_WAIT_SYNC1;  // Siap untuk paket berikutnya
        if (b == ck_a && gpsSerial.read() == ck_b) {
          return true;  // Sukses! Data valid diterima.
        }
        return false;  // Checksum gagal, data tidak valid.
    }
  }
  return false;  // Tidak ada data baru yang selesai diproses
}

// --- Fungsi Getter untuk Data GPS ---
double getLatitude() {
  return gpsData.lat / 10000000.0;
}
double getLongitude() {
  return gpsData.lon / 10000000.0;
}
double getAltitudeMSL() {
  return gpsData.hMSL / 1000.0;
}
double getGroundSpeed() {
  return gpsData.gSpeed / 1000.0;
}
bool isLocationValid() {
  return (gpsData.flags & 0x01) && (gpsData.fixType >= 3);
}

double getGroundSpeedKmh() { 
  // gSpeed dari GPS dalam mm/s. Konversi ke m/s lalu ke km/h
  return (gpsData.gSpeed / 1000.0) * 3.6; 
}



// ======================================================================
// BAGIAN 3: FUNGSI UTAMA ARDUINO
// ======================================================================

void setup() {
  Serial.begin(9600);  // Untuk debugging di Serial Monitor

  // Inisialisasi port serial untuk GPS
  // PASTIKAN baud rate ini sesuai dengan konfigurasi modul GPS Anda!
  gpsSerial.begin(115200);

  // Inisialisasi display OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Alokasi SSD1306 gagal"));
    for (;;)
      ;  // Hentikan program jika OLED tidak terdeteksi
  }

  // Tampilkan layar pembuka saat startup
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("PUS Tigagajah", 10);

  // Tampilan kecepatan awal dan satuannya dibuat terpisah agar lebih rapi
  display.setTextSize(1);
  drawCenteredText("0.00", 38);
  display.setTextSize(1);
  drawCenteredText("KM/JAM", 64 - 12);

  display.display();  // Tampilkan ke layar
}

void loop() {
  // Selalu panggil fungsi parser di setiap loop untuk memeriksa data dari GPS
  if (parseUbx()) {
    // Blok ini hanya akan berjalan jika ada data GPS baru yang valid
    Serial.println("--- Data GPS Valid Diterima ---");

    if (isLocationValid()) {
      Serial.print("  Lokasi: ");
      Serial.print(getLatitude(), 7);
      Serial.print(", ");
      Serial.println(getLongitude(), 7);
      Serial.print("  Ketinggian: ");
      Serial.print(getAltitudeMSL());
      Serial.println(" m (di atas permukaan laut)");
      Serial.print("  Kecepatan: ");
      Serial.print(getGroundSpeed());
      Serial.println(" m/s");

      // Ambil kecepatan dalam km/jam
      double speedKmh = getGroundSpeedKmh();

      // Format teks kecepatan (dua angka di belakang koma)
      String speedText = String(speedKmh, 2);

      // Hapus tampilan lama dan gambar ulang dengan data baru
      display.clearDisplay();

      // Gambar ulang teks statis di baris atas
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      drawCenteredText("PUS Tigagajah", 10);

      // Gambar ulang teks kecepatan yang dinamis
      display.setTextSize(1);
      drawCenteredText(speedText, 38);

      // Gambar ulang satuan di bawahnya
      display.setTextSize(1);
      drawCenteredText("KM/JAM", 64 - 12);

      // Kirim buffer yang sudah digambar ke layar OLED
      display.display();

      // (Opsional) Cetak ke Serial Monitor untuk debugging
      Serial.print("Kecepatan terdeteksi: ");
      Serial.print(speedKmh);
      Serial.println(" km/jam");
    } else {
      Serial.println("  Lokasi Belum Valid (Fix Type < 3D)");
      display.clearDisplay();
      display.setTextSize(1);
      drawCenteredText("Lokasi Belum Valid", 10);
      display.display();
    }
    Serial.print("  Satelit: ");
    Serial.println(gpsData.numSV);
    Serial.println("---------------------------------");
  }
}
