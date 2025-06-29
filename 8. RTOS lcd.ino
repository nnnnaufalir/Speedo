#include <Wire.h>
#include <U8g2lib.h>
#include <U8x8lib.h>

#if !defined(ESP32)
#include <Arduino_FreeRTOS.h>
#include <semphr.h>
#endif

U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/10, /* reset=*/8);

String statusDisplay = "Ready";
int speedDisplay = 0;

// Fungsi ini sekarang menerima data status sebagai argumen
void drawStatusSection() {
  u8g2.drawBox(0, 0, 128, 32);
  u8g2.setColorIndex(0);

  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(2, 12, "Status:");
  u8g2.drawStr(2, 26, statusDisplay.c_str());
  u8g2.drawHLine(0, 30, 128);
}

// Fungsi ini sekarang menerima data kecepatan sebagai argumen
void drawSpeedSection() {
  char speedBuffer[10];

  dtostrf(speedDisplay, 4, 2, speedBuffer);
  u8g2.setColorIndex(1);
  u8g2.setFont(u8g2_font_logisoso22_tn);
  u8g2.drawStr(2, 58, speedBuffer);

  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(85, 58, "km/j");
  u8g2.drawFrame(0, 32, 128, 32);
}

void displayUpdateTask(void* parameter) {
  for (;;) {
    speedDisplay++;
    u8g2.firstPage();
    do {
      drawStatusSection();
      drawSpeedSection();
    } while (u8g2.nextPage());

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void setup() {
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFontDirection(0);

  // Buat task untuk memperbarui display
  xTaskCreate(
    displayUpdateTask,
    "Display Updater",
    2048,  // Task display butuh stack lebih besar
    NULL,
    4,  // Prioritas lebih tinggi agar display responsif
    NULL);
}

// Loop() tidak digunakan lagi oleh scheduler FreeRTOS
void loop() {}
