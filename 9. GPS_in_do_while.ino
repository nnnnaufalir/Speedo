#include <SoftwareSerial.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <U8x8lib.h>

// Atur pin yang akan digunakan untuk SoftwareSerial
SoftwareSerial gpsSerial(4, 5);  // RX, TX

// --- Konfigurasi LCD (sesuai yang Anda berikan) ---
U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/10, /* reset=*/8);

// Variabel global untuk menyimpan status & kecepatan
double currentSpeedKmh = 0.0;

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

// Buat satu variabel global untuk menampung data GPS
GpsData gpsData;

// Variabel untuk State Machine parser
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

// Fungsi ini adalah jantung dari parser.
// Mengembalikan 'true' jika ada paket data baru yang valid.
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
        ((byte*)&gpsData)[payloadCounter] = b;  // Isi struct byte per byte
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
double convertToKmPerHour(double speed_ms) {
  return speed_ms * 3.6;
}

void setup() {

  Serial.begin(9600);
  gpsSerial.begin(115200);

  Serial.println("Parser GPS UBX-NAV-PVT dalam satu file .ino");
  Serial.println("Menunggu data GPS...");

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFontDirection(0);
}

void loop() {
  u8g2.clearBuffer();
  // --- Selalu gambar ulang tampilan di LCD pada setiap loop ---
  if (parseUbx()) {
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
      currentSpeedKmh = convertToKmPerHour(getGroundSpeed());
      Serial.print("Status: LOKASI VALID. Kecepatan: ");
      Serial.print(currentSpeedKmh);
      Serial.println(" km/j");
    } else {
      Serial.println("  Lokasi Belum Valid (Fix Type < 3D)");
    }

    Serial.print("  Satelit: ");
    Serial.println(gpsData.numSV);
    Serial.println("---------------------------------");
  }

  drawStatusBox();
  drawSpeedBox(currentSpeedKmh);
  u8g2.sendBuffer();

  delay(10);
}

void drawStatusBox() {
  u8g2.drawBox(0, 0, 128, 32);
  u8g2.setColorIndex(0);  // Warna tulisan jadi "transparan" (warna background)

  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.drawStr(12, 22, "PUS Tigagajah");

  u8g2.setColorIndex(1);  // Kembalikan warna ke normal untuk bagian lain
}

void drawSpeedBox(double speed) {
  char speedBuffer[6];  // Buffer untuk "xx,yy\0"

  // Pisahkan bagian integer dan desimal
  int integerPart = (int)speed;
  int fractionalPart = (int)((speed - integerPart) * 100);

  // Format string dengan 2 digit angka, nol di depan, dan koma
  // Misal: 5.7 km/j -> "05,70"
  sprintf(speedBuffer, "%02d,%02d", integerPart, fractionalPart);

  u8g2.setColorIndex(1);  // Pastikan warna tulisan normal (putih)

  // Gambar nilai kecepatan dengan font besar
  u8g2.setFont(u8g2_font_logisoso22_tn);
  u8g2.drawStr(20, 58, speedBuffer);

  // Gambar satuan "KM/J"
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(85, 58, "KM/J");

  // Gambar bingkai di bagian bawah
  u8g2.drawFrame(0, 32, 128, 32);
}
