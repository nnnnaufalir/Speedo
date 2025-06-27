#include <Wire.h>
#include <U8g2lib.h>
#include <U8x8lib.h>
const char DEGREE_SYMBOL[] = { 0xB0, '\0' };
U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/10, /* reset=*/8);

String statusDisplay = "nyoba v2";
int speedDisplay = 0;

void setup() {
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFontDirection(0);
}

void loop() {
  speedDisplay++;
  u8g2.firstPage();
  do {
    tulisanputih();
    tulisan();
  } while (u8g2.nextPage());

  delay(1000);
}

void tulisanputih() {
  u8g2.drawBox(0, 0, 128, 32);
  u8g2.setColorIndex(0);

  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(2, 12, "Status:");
  u8g2.drawStr(2, 26, statusDisplay.c_str());
  u8g2.drawHLine(0, 30, 128);  // Garis pemisah
}

void tulisan() {
  char speedBuffer[10];  // Buffer untuk menampung string kecepatan
    // Konversi nilai double ke char array (string C), format: (nilai, lebar minimal, desimal, buffer)
  dtostrf(speedDisplay, 4, 2, speedBuffer);
  u8g2.setColorIndex(1);
  u8g2.setFont(u8g2_font_logisoso22_tn);  // Font lebih besar untuk kecepatan
  u8g2.drawStr(2, 58, speedBuffer);

  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(85, 58, "km/j");
  u8g2.drawFrame(0, 32, 128, 32);
}
