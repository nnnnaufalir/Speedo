#include <SoftwareSerial.h>
// Untuk papan seperti Arduino Uno/Mega, sertakan pustaka FreeRTOS.
// Untuk ESP32, FreeRTOS sudah terintegrasi, jadi baris ini tidak perlu.
#if !defined(ESP32)
#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#endif

// ======================================================================
// 1. PENGATURAN & DEKLARASI GLOBAL
// ======================================================================

// Atur pin yang akan digunakan untuk SoftwareSerial
SoftwareSerial gpsSerial(4, 5); // RX, TX

// Struktur untuk menampung data mentah dari payload UBX-NAV-PVT
// (Struktur GpsData tidak berubah, sama seperti kode asli)
struct GpsData {
  uint32_t iTOW;
  uint16_t year;
  uint8_t  month;
  uint8_t  day;
  uint8_t  hour;
  uint8_t  min;
  uint8_t  sec;
  uint8_t  valid;
  uint32_t tAcc;
  int32_t  nano;
  uint8_t  fixType;
  uint8_t  flags;
  uint8_t  flags2;
  uint8_t  numSV;
  int32_t  lon;
  int32_t  lat;
  int32_t  height;
  int32_t  hMSL;
  uint32_t hAcc;
  uint32_t vAcc;
  int32_t  velN;
  int32_t  velE;
  int32_t  velD;
  int32_t  gSpeed;
  int32_t  headMot;
  uint32_t sAcc;
  uint32_t headAcc;
  uint16_t pDOP;
  uint8_t  flags3[2];
  uint8_t  reserved0[4];
  int32_t  headVeh;
  int16_t  magDec;
  uint16_t magAcc;
};

GpsData gpsData;

// Handle untuk RTOS
SemaphoreHandle_t newDataSemaphore;
SemaphoreHandle_t gpsDataMutex;

// Variabel untuk State Machine parser (tidak berubah)
enum GpsState {
  STATE_WAIT_SYNC1, STATE_WAIT_SYNC2, STATE_WAIT_CLASS, STATE_WAIT_ID,
  STATE_WAIT_LEN1, STATE_WAIT_LEN2, STATE_PAYLOAD, STATE_CHECKSUM
};
GpsState currentState = STATE_WAIT_SYNC1;


// ======================================================================
// 2. FUNGSI HELPER (Tidak berubah dari kode asli)
// ======================================================================

bool parseUbx();
double getLatitude()   { return gpsData.lat / 10000000.0; }
double getLongitude()  { return gpsData.lon / 10000000.0; }
double getAltitudeMSL(){ return gpsData.hMSL / 1000.0; }
double getGroundSpeed(){ return gpsData.gSpeed / 1000.0; }
bool isLocationValid() { return (gpsData.flags & 0x01) && (gpsData.fixType >= 3); }

// ======================================================================
// 3. TASK UNTUK RTOS
// ======================================================================

/**
 * @brief Task untuk parsing data dari GPS secara terus-menerus.
 * Prioritas lebih tinggi untuk memastikan tidak ada data serial yang terlewat.
 */
void gpsParserTask(void *parameter) {
  for (;;) {
    if (parseUbx()) {
      // Jika data baru berhasil diparsing, berikan sinyal ke task lain
      xSemaphoreGive(newDataSemaphore);
    }
    // Beri sedikit waktu agar task lain bisa berjalan
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/**
 * @brief Task untuk menampilkan data GPS ke Serial Monitor.
 * Task ini akan "tidur" hingga ada sinyal data baru.
 */
void dataPrinterTask(void *parameter) {
  for (;;) {
    // Tunggu sinyal bahwa ada data baru. Blokir task hingga sinyal diterima.
    if (xSemaphoreTake(newDataSemaphore, portMAX_DELAY) == pdTRUE) {
      // Ambil alih mutex sebelum mengakses data bersama (gpsData)
      if (xSemaphoreTake(gpsDataMutex, portMAX_DELAY) == pdTRUE) {
        
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
        } else {
          Serial.println("  Lokasi Belum Valid (Fix Type < 3D)");
        }
        
        Serial.print("  Satelit: ");
        Serial.println(gpsData.numSV);
        Serial.println("---------------------------------");

        // Lepaskan mutex setelah selesai mengakses data
        xSemaphoreGive(gpsDataMutex);
      }
    }
  }
}

// ======================================================================
// 4. FUNGSI UTAMA ARDUINO
// ======================================================================

void setup() {
  Serial.begin(9600);
  // PENTING: Gunakan baud rate yang sesuai dengan konfigurasi modul GPS Anda
  gpsSerial.begin(115200);

  Serial.println("Parser GPS UBX-NAV-PVT - Versi RTOS");
  Serial.println("Menunggu data GPS...");

  // Buat semaphore biner untuk notifikasi data baru
  newDataSemaphore = xSemaphoreCreateBinary();
  // Buat mutex untuk melindungi data bersama
  gpsDataMutex = xSemaphoreCreateMutex();

  if (newDataSemaphore != NULL && gpsDataMutex != NULL) {
    // Buat task untuk parsing data GPS
    xTaskCreate(
      gpsParserTask,          // Fungsi task
      "GPS Parser Task",      // Nama task
      2048,                   // Ukuran stack (bytes)
      NULL,                   // Parameter task
      4,                      // Prioritas (lebih tinggi)
      NULL                    // Handle task
    );

    // Buat task untuk mencetak data
    xTaskCreate(
      dataPrinterTask,
      "Data Printer Task",
      2048,
      NULL,
      1,                      // Prioritas (lebih rendah)
      NULL
    );
  } else {
    Serial.println("Gagal membuat semaphore atau mutex. Program berhenti.");
    while(1); // Hentikan eksekusi
  }
}

// Loop() tidak digunakan lagi oleh scheduler FreeRTOS
void loop() {}

// ======================================================================
// FUNGSI PARSER (Sama seperti asli, namun dengan proteksi mutex)
// ======================================================================

bool parseUbx() {
  uint8_t  msgClass, msgId;
  uint16_t msgLen;
  uint16_t payloadCounter;
  uint8_t  ck_a = 0, ck_b = 0;

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
        ck_a = 0; ck_b = 0;
        ck_a += b; ck_b += ck_a;
        currentState = STATE_WAIT_ID;
        break;
      
      case STATE_WAIT_ID:
        msgId = b;
        ck_a += b; ck_b += ck_a;
        currentState = STATE_WAIT_LEN1;
        break;

      case STATE_WAIT_LEN1:
        msgLen = b;
        ck_a += b; ck_b += ck_a;
        currentState = STATE_WAIT_LEN2;
        break;

      case STATE_WAIT_LEN2:
        msgLen |= (uint16_t)b << 8;
        ck_a += b; ck_b += ck_a;
        
        if (msgClass == 0x01 && msgId == 0x07 && msgLen == sizeof(GpsData)) {
          currentState = STATE_PAYLOAD;
          payloadCounter = 0;
          // Ambil mutex sebelum mulai mengisi struct gpsData
          xSemaphoreTake(gpsDataMutex, portMAX_DELAY);
        } else {
          currentState = STATE_WAIT_SYNC1;
        }
        break;

      case STATE_PAYLOAD:
        ((byte*)&gpsData)[payloadCounter] = b;
        ck_a += b; ck_b += ck_a;
        payloadCounter++;
        if (payloadCounter >= msgLen) {
          currentState = STATE_CHECKSUM;
        }
        break;
      
      case STATE_CHECKSUM: {
        uint8_t received_ck_a = b;
        uint8_t received_ck_b = gpsSerial.read();
        currentState = STATE_WAIT_SYNC1;

        if (received_ck_a == ck_a && received_ck_b == ck_b) {
          // Checksum valid, lepaskan mutex dan kembalikan true
          xSemaphoreGive(gpsDataMutex);
          return true;
        } else {
          // Checksum gagal, lepaskan mutex dan kembalikan false
          xSemaphoreGive(gpsDataMutex);
          return false;
        }
      }
    }
  }
  return false;
}
