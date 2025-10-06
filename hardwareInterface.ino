
/* Hardware Interface module */

#include "ESP32Serial.h" /* Do NOT use the standard HardwareSerial.h, because this has stability issue.
                            The LoadProhibited error in Hardwareserial is also discussed here:
                            https://github.com/espressif/arduino-esp32/issues/1070 (also with EXCVADDR: 0x0000000c)
                            and https://github.com/espressif/arduino-esp32/issues/991. There the workaround is: do
                            not use Arduino-ESP32's HardwareSerial and used esp-idf's uart library instead by
                            including driver/uart.h. There is a library as workaround: https://github.com/9a4gl/ESP32Serial.
                            Copied the ESP32Serial.cpp and .h from this, and using this instead of
                            the Arduinos HardwareSerial. */

ESP32Serial mySerial(1); /* 0 would be the Serial which is used for Serial.print. 1 is the Serial2. 
                               The pins can be freely chosen, see below. */
/* TXD for display is IO17 according to the data sheet of the WT32-ETH01. */
/* RXD (unused) is IO5 according to the data sheet of the WT32-ETH01. */
#define TX2_PIN 17
#define RX2_PIN 5


void hardwareInterface_showOnDisplay(String s1, String s2, String s3) {
  /* shows three lines on the display */
  String s;
  //char cs[] = "lcOtto\n"; 
  /* To address the display, each line starts with "lc" (short for "LCD").
     Each line ends with an carriage return ("\n"). */
  s = "lc" + s1 + "\n" + "lc" + s2 + "\n" + "lc" + s3 + "\n";
  //log_v("%s", s.c_str()); 
  mySerial.print(s.c_str());
  //mySerial.flush();
}



void hardwareInterface_initDisplay(void) {
  //mySerial.setTxBufferSize(256);
  mySerial.begin(19200, SERIAL_8N1, RX2_PIN, TX2_PIN);
  //s = "lc" + s1 + "\n" + "lc" + s2 + "\n" + "lc" + s3 + "\n"  
  mySerial.println("lcHello :-)\nlcfrom\nlcWT32-ETH");
}

int hardwareInterface_sanityCheck() {
  return mySerial.checkPort();
}
