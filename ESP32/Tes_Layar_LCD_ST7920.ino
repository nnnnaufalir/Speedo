#include <U8g2lib.h>
#include <Wire.h>

/*
  Koneksi Pin untuk Layar ST7920 (SPI) ke ESP32:
  - VCC -> 5V atau 3.3V
  - GND -> GND
  - CS  -> GPIO 5
  - RST -> GPIO 4
  - SCK -> GPIO 18 (SCK default)
  - MOSI-> GPIO 23 (MOSI default)
*/

// Inisialisasi U8g2 untuk ESP32 menggunakan Hardware SPI
// Pin CS diatur ke GPIO 5, dan pin Reset ke GPIO 4.
U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/ 5, /* reset=*/ 4);

void setup(void) {
  // Mulai komunikasi dengan layar
  u8g2.begin();
  u8g2.enableUTF8Print(); // Aktifkan karakter UTF8 jika diperlukan
}

void loop(void) {
  u8g2.firstPage();
  do {
    // Gambar kotak atas
    u8g2.drawBox(0, 0, 128, 32);
    u8g2.setColorIndex(0); // Warna tulisan jadi "transparan"
    u8g2.setFont(u8g2_font_helvB12_tf);
    u8g2.drawStr(20, 22, "Tes Layar");
    u8g2.setColorIndex(1); // Kembalikan warna ke normal

    // Gambar kotak bawah
    u8g2.drawFrame(0, 32, 128, 32);
    u8g2.setFont(u8g2_font_logisoso22_tn);
    u8g2.drawStr(20, 58, "12,34");
    u8g2.setFont(u8g2_font_7x13B_tr);
    u8g2.drawStr(85, 58, "KM/J");

  } while (u8g2.nextPage());

  delay(1000); // Tunggu 1 detik sebelum menggambar ulang
}
