#include <U8g2lib.h>
#include <Wire.h>
#include <Arduino.h>

/*
  Koneksi Pin ESP32:
  Layar ST7920 (SPI):
  - CS: GPIO 5, RST: GPIO 4, SCK: GPIO 18, MOSI: GPIO 23
  Modul GPS (UART2):
  - RX2: GPIO 16 (ke TX GPS), TX2: GPIO 17 (ke RX GPS)
*/

// Inisialisasi Layar
U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/ 5, /* reset=*/ 4);

// Inisialisasi GPS menggunakan HardwareSerial 2
HardwareSerial gpsSerial(2);

// Variabel global untuk menyimpan kecepatan
double currentSpeedKmh = 0.0;
bool locationIsValid = false;

// --- Salin/Tempel semua kode parser UBX dari sini ---
struct GpsData {
  uint32_t iTOW; uint16_t year; uint8_t month; uint8_t day; uint8_t hour;
  uint8_t min; uint8_t sec; uint8_t valid; uint32_t tAcc; int32_t nano;
  uint8_t fixType; uint8_t flags; uint8_t flags2; uint8_t numSV;
  int32_t lon; int32_t lat; int32_t height; int32_t hMSL; uint32_t hAcc;
  uint32_t vAcc; int32_t velN; int32_t velE; int32_t velD; int32_t gSpeed;
  int32_t headMot; uint32_t sAcc; uint32_t headAcc; uint16_t pDOP;
  uint8_t flags3[2]; uint8_t reserved0[4]; int32_t headVeh;
  int16_t magDec; uint16_t magAcc;
};
GpsData gpsData;

enum GpsState {
  STATE_WAIT_SYNC1, STATE_WAIT_SYNC2, STATE_WAIT_CLASS, STATE_WAIT_ID,
  STATE_WAIT_LEN1, STATE_WAIT_LEN2, STATE_PAYLOAD, STATE_CHECKSUM
};
GpsState currentState = STATE_WAIT_SYNC1;

bool parseUbx() {
  uint8_t msgClass, msgId; uint16_t msgLen; uint16_t payloadCounter;
  uint8_t ck_a, ck_b;
  while (gpsSerial.available() > 0) {
    byte b = gpsSerial.read();
    switch (currentState) {
      case STATE_WAIT_SYNC1: if (b == 0xB5) currentState = STATE_WAIT_SYNC2; break;
      case STATE_WAIT_SYNC2: if (b == 0x62) currentState = STATE_WAIT_CLASS; else currentState = STATE_WAIT_SYNC1; break;
      case STATE_WAIT_CLASS: msgClass = b; ck_a = 0; ck_b = 0; ck_a += b; ck_b += ck_a; currentState = STATE_WAIT_ID; break;
      case STATE_WAIT_ID: msgId = b; ck_a += b; ck_b += ck_a; currentState = STATE_WAIT_LEN1; break;
      case STATE_WAIT_LEN1: msgLen = b; ck_a += b; ck_b += ck_a; currentState = STATE_WAIT_LEN2; break;
      case STATE_WAIT_LEN2:
        msgLen |= (uint16_t)b << 8; ck_a += b; ck_b += ck_a;
        if (msgClass == 0x01 && msgId == 0x07 && msgLen == sizeof(GpsData)) {
          currentState = STATE_PAYLOAD; payloadCounter = 0;
        } else { currentState = STATE_WAIT_SYNC1; }
        break;
      case STATE_PAYLOAD:
        ((byte*)&gpsData)[payloadCounter] = b; ck_a += b; ck_b += ck_a; payloadCounter++;
        if (payloadCounter >= msgLen) { currentState = STATE_CHECKSUM; }
        break;
      case STATE_CHECKSUM:
        currentState = STATE_WAIT_SYNC1;
        if (b == ck_a && gpsSerial.read() == ck_b) { return true; }
        return false;
    }
  }
  return false;
}

double getGroundSpeed() { return gpsData.gSpeed / 1000.0; }
bool isLocationValid() { return (gpsData.flags & 0x01) && (gpsData.fixType >= 3); }
double convertToKmPerHour(double speed_ms) { return speed_ms * 3.6; }
// --- Akhir dari kode parser UBX ---

void drawStatusBox() {
  u8g2.drawBox(0, 0, 128, 32);
  u8g2.setColorIndex(0);
  u8g2.setFont(u8g2_font_helvB12_tf);
  u8g2.drawStr(12, 22, "PUS Tigagajah");
  u8g2.setColorIndex(1);
}

void drawSpeedBox(double speed) {
  char speedBuffer[6];
  int integerPart = (int)speed;
  int fractionalPart = (int)((speed - integerPart) * 100);
  sprintf(speedBuffer, "%02d,%02d", integerPart, fractionalPart);

  u8g2.setColorIndex(1);
  u8g2.setFont(u8g2_font_logisoso22_tn);
  u8g2.drawStr(20, 58, speedBuffer);
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(85, 58, "KM/J");
  u8g2.drawFrame(0, 32, 128, 32);
}

void drawSearchingScreen() {
    u8g2.setFont(u8g2_font_helvB10_tf);
    u8g2.drawStr(15, 28, "Mencari sinyal");
    u8g2.drawStr(45, 48, "GPS...");
}

void setup() {
  Serial.begin(9600);
  gpsSerial.begin(115200, SERIAL_8N1, 16, 17);
  
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFontDirection(0);

  Serial.println("Speedometer GPS UBX - ESP32");
}

void loop() {
  if (parseUbx()) {
    locationIsValid = isLocationValid();
    if (locationIsValid) {
      currentSpeedKmh = convertToKmPerHour(getGroundSpeed());
    }
    // Cetak ke Serial Monitor untuk debug
    Serial.printf("Status: %s, Kecepatan: %.2f km/j, Satelit: %d\n",
                  locationIsValid ? "VALID" : "TIDAK VALID",
                  currentSpeedKmh,
                  gpsData.numSV);
  }

  // Gambar ke layar setiap saat
  u8g2.firstPage();
  do {
    if (locationIsValid) {
      drawStatusBox();
      drawSpeedBox(currentSpeedKmh);
    } else {
      // Tampilkan layar "mencari sinyal" jika lokasi belum valid
      drawSearchingScreen();
    }
  } while (u8g2.nextPage());

  delay(10);
}
