#include<Wire.h>
#include <U8g2lib.h>
#include <U8x8lib.h>
const char DEGREE_SYMBOL[] = { 0xB0, '\0' };
U8G2_ST7920_128X64_1_HW_SPI u8g2(U8G2_R0, /* CS=*/ 10, /* reset=*/ 8);

void setup() {
u8g2.begin();
u8g2.enableUTF8Print();
u8g2.setFontDirection(0); 

}

void loop() {
  
u8g2.firstPage();
       do{       
        tulisanputih();
        tulisan();
       }while( u8g2.nextPage() );
}

void tulisanputih(){
u8g2.setFont(u8g2_font_helvB10_tf);
u8g2.drawBox(0,0,128,32); 
u8g2.setColorIndex(0);
u8g2.drawStr( 46, 20, "NYOBA BANG");
}

void tulisan(){
u8g2.setFont(u8g2_font_7x13B_tr );
u8g2.setColorIndex(1);
u8g2.drawStr( 38, 53, "00,00 KpJ");
u8g2.drawFrame(0,32,128,32);    

}
