#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SCREEN_ADDRESS 0x3D
// #define SCREEN_ADDRESS 0x3C

#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void drawCenteredText(const String &text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
}

void setup() {
  Serial.begin(9600);

  // Inisialisasi display dengan alamat I2C yang sudah didefinisikan
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Alokasi SSD1306 gagal"));
    for(;;);
  }

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("PUS Tigagajah", 10);

  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  drawCenteredText("00.00 KM/JAM", 38);

  display.display();
}

void loop() {
  // Kosong untuk saat ini
}
