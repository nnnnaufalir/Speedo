#include <Wire.h>
#include <U8g2lib.h>
#include <U8x8lib.h>
// Untuk papan seperti Arduino Uno/Mega, sertakan pustaka FreeRTOS.
// Untuk ESP32, FreeRTOS sudah terintegrasi, jadi baris ini tidak perlu.
#if !defined(ESP32)
#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#endif

// Inisialisasi layar U8g2 (tidak berubah)
U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/ 10, /* reset=*/ 8);

// ======================================================================
// 1. SUMBER DAYA BERSAMA & SINKRONISASI
// ======================================================================

// Variabel data yang akan ditampilkan, sekarang dilindungi oleh mutex
String statusDisplay = "Ready";
int speedDisplay = 0;

// Handle untuk Mutex
SemaphoreHandle_t displayDataMutex;

// ======================================================================
// 2. FUNGSI GAMBAR (dimodifikasi untuk menerima parameter)
// ======================================================================

// Fungsi ini sekarang menerima data status sebagai argumen
void drawStatusSection(const char* status) {
  u8g2.drawBox(0, 0, 128, 32);
  u8g2.setColorIndex(0); // Teks warna terbalik (hitam di atas kotak putih)

  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(2, 12, "Status:");
  u8g2.drawStr(2, 26, status);
  u8g2.drawHLine(0, 30, 128);
}

// Fungsi ini sekarang menerima data kecepatan sebagai argumen
void drawSpeedSection(int speed) {
  char speedBuffer[10];
  // Konversi nilai int ke string C-style
  sprintf(speedBuffer, "%d", speed); 

  u8g2.setColorIndex(1); // Teks normal (putih di atas background hitam)
  u8g2.setFont(u8g2_font_logisoso22_tn);
  u8g2.drawStr(2, 58, speedBuffer);

  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(85, 58, "km/j");
  u8g2.drawFrame(0, 32, 128, 32);
}

// ======================================================================
// 3. DEFINISI TASK UNTUK RTOS
// ======================================================================

/**
 * @brief Task untuk memperbarui data (misal: dari sensor).
 * Berjalan secara independen dari task display.
 */
void dataUpdateTask(void *parameter) {
  for (;;) {
    // Ambil alih mutex sebelum memodifikasi data bersama
    if (xSemaphoreTake(displayDataMutex, portMAX_DELAY) == pdTRUE) {
      speedDisplay++; // Simulasikan data kecepatan yang terus bertambah
      if (speedDisplay > 999) {
        speedDisplay = 0;
      }
      // Lepaskan mutex setelah selesai
      xSemaphoreGive(displayDataMutex);
    }
    // Update data setiap 500 milidetik
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/**
 * @brief Task untuk menggambar ulang seluruh layar secara periodik.
 */
void displayUpdateTask(void *parameter) {
  for (;;) {
    // Variabel lokal untuk menyimpan salinan data yang aman
    String localStatus;
    int localSpeed;

    // Ambil alih mutex untuk menyalin data bersama ke variabel lokal
    if (xSemaphoreTake(displayDataMutex, portMAX_DELAY) == pdTRUE) {
      localStatus = statusDisplay;
      localSpeed = speedDisplay;
      // Segera lepaskan mutex agar tidak memblokir task lain terlalu lama
      xSemaphoreGive(displayDataMutex);
    }

    // Proses penggambaran menggunakan data lokal yang sudah aman
    u8g2.firstPage();
    do {
      drawStatusSection(localStatus.c_str());
      drawSpeedSection(localSpeed);
    } while (u8g2.nextPage());

    // Tunggu 1 detik sebelum menggambar ulang layar
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ======================================================================
// 4. FUNGSI UTAMA ARDUINO
// ======================================================================

void setup() {
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFontDirection(0);

  // Buat Mutex untuk melindungi akses ke variabel display
  displayDataMutex = xSemaphoreCreateMutex();

  if (displayDataMutex != NULL) {
    // Buat task untuk memperbarui data
    xTaskCreate(
      dataUpdateTask,
      "Data Updater",
      1024, // Ukuran stack
      NULL,
      1,    // Prioritas
      NULL
    );

    // Buat task untuk memperbarui display
    xTaskCreate(
      displayUpdateTask,
      "Display Updater",
      2048, // Task display butuh stack lebih besar
      NULL,
      2,    // Prioritas lebih tinggi agar display responsif
      NULL
    );
  }
}

// Loop() tidak digunakan lagi oleh scheduler FreeRTOS
void loop() {}
