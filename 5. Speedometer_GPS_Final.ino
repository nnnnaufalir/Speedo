// ======================================================================
// BAGIAN 1: PENGATURAN & DEKLARASI GLOBAL
// Menggabungkan library, konfigurasi LCD, dan deklarasi GPS
// ======================================================================

#include <Wire.h>
#include <U8g2lib.h>
#include <U8x8lib.h>
#include <SoftwareSerial.h>

// --- Konfigurasi LCD (sesuai yang Anda berikan) ---
U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/10, /* reset=*/8);

// --- Konfigurasi GPS (sesuai yang Anda berikan) ---
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

// Variabel global untuk menyimpan status & kecepatan
double currentSpeedKmh = 0.0;
unsigned long lastGpsPacketTime = 0;
bool gpsDataReceived = false;

// ======================================================================
// BAGIAN 2: FUNGSI PARSING & PENGOLAHAN DATA GPS
// Fungsi ini adalah jantung dari parser.
// Mengembalikan 'true' jika ada paket data baru yang valid.
// ======================================================================

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

double getGroundSpeed() {
  return gpsData.gSpeed / 1000.0;
}  // Hasil dalam m/s
bool isLocationValid() {
  return (gpsData.flags & 0x01) && (gpsData.fixType >= 3);
}
double convertToKmPerHour(double speed_ms) {
  return speed_ms * 3.6;
}

// ======================================================================
// BAGIAN 3: FUNGSI TAMPILAN LCD
// Disesuaikan dari kode LCD Anda yang berhasil tampil
// ======================================================================

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

// ======================================================================
// BAGIAN 4: FUNGSI UTAMA ARDUINO
// ======================================================================

void setup() {
  // --- Inisialisasi Serial untuk Debugging ---
  Serial.begin(9600);
  Serial.println("===============================");
  Serial.println("GPS Speedometer - PUS Tigagajah");
  Serial.println("===============================");
  Serial.println("Menunggu data dari modul GPS...");

  // --- Inisialisasi GPS ---
  gpsSerial.begin(115200);  // Sesuai kode dasar Anda

  // --- Inisialisasi LCD ---
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFontDirection(0);
}

void loop() {
  // Selalu coba baca data dari GPS
  if (parseUbx()) {
    lastGpsPacketTime = millis();  // Catat waktu terakhir dapat paket valid
    gpsDataReceived = true;

    // Cetak info ke Serial Monitor untuk debugging
    Serial.println("--- Paket UBX Valid Diterima ---");
    Serial.print("Jumlah Satelit: ");
    Serial.println(gpsData.numSV);

    if (isLocationValid()) {
      currentSpeedKmh = convertToKmPerHour(getGroundSpeed());
      Serial.print("Status: LOKASI VALID. Kecepatan: ");
      Serial.print(currentSpeedKmh);
      Serial.println(" km/j");
    } else {
      currentSpeedKmh = 0.0;
      Serial.println("Status: Lokasi belum valid (Fix belum 3D). Kecepatan di-set 0.");
    }
    Serial.println("---------------------------------");
  }

  else {
    Serial.println("Gagal Mengambil data GPS");
    Serial.println("---------------------------------");
  }

  // Cek jika sinyal GPS hilang (tidak ada paket data selama > 3 detik)
  if (gpsDataReceived && (millis() - lastGpsPacketTime > 3000)) {
    Serial.println("ALERT: Sinyal GPS hilang. Kecepatan di-set 0.");
    currentSpeedKmh = 0.0;
    lastGpsPacketTime = millis();  // Reset timer untuk menghindari spam alert
  }

  Serial.println("Looping Utama Berjalan");
  Serial.println("---------------------------------");

  // --- Selalu gambar ulang tampilan di LCD pada setiap loop ---
  u8g2.firstPage();
  do {
    drawStatusBox();
    drawSpeedBox(currentSpeedKmh);
  } while (u8g2.nextPage());

  delay(10);
}
