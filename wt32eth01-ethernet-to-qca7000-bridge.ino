/* This is the main Arduino file of the project. */
/* Developed in Arduino IDE 2.2.1 */

/* Modularization concept:
- The wt32eth01-ethernet-to-qca7000-bridge.ino is the main Arduino file of the project.
- Some other .ino files are present, and the Arduino IDE will merge all the .ino into a
  single cpp file before compiling. That's why, all the .ino share the same global context.
- Some other files may be used, which are "hidden" in the src folder. These are not shown in the Arduino IDE,
  but the Arduino IDE "knows" them and will compile and link them. You may want to use
  an other editor (e.g. Notepad++) for editing them.
- We define "global variables" as the data sharing concept. The header file "globalconfig.h"
  declare the public data and public functions.
- Using a mix of cpp and c modules works; the only requirement is to use the special "extern C" syntax in
  the header files.
*/

#include "globalconfig.h"
//#include "src/somethingother.h"

/**********************************************************/

#define PIN_LED 2 /* The IO2 is used for an LED. This LED is externally added to the WT32-ETH01 board. */
//#define PIN_STATE_C 4 /* The IO4 is used to change the CP line to state C. High=StateC, Low=StateB */ 
//#define PIN_POWER_RELAIS 14 /* IO14 for the power relay */
uint32_t currentTime;
uint32_t lastTime1s;
uint32_t lastTime30ms;
uint32_t nCycles30ms;
uint8_t ledState;
uint32_t initialHeapSpace;
uint32_t eatenHeapSpace;
String globalLine1;
String globalLine2;
String globalLine3;
uint16_t counterForDisplayUpdate;


void sanityCheck(String info) {
  int r;
  r= hardwareInterface_sanityCheck();
  if (eatenHeapSpace>10000) {
    /* if something is eating the heap, this is a fatal error. */
    addToTrace("ERROR: Sanity check failed due to heap space check.");
    r = -10;
  }
  if (r!=0) {
      addToTrace(String("ERROR: Sanity check failed ") + String(r) + " " + info);
      delay(2000); /* Todo: we should make a reset here. */
  }
}

/**********************************************************/
/* The logging macros and functions */
#undef log_v
#undef log_e
#define log_v(format, ...) log_printf(ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__)
#define log_e(format, ...) log_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)

void addToTrace_chararray(char *s) {
  log_v("%s", s);  
}

void addToTrace(String strTrace) {
  //Serial.println(strTrace);  
  log_v("%s", strTrace.c_str());  
}

void showAsHex(uint8_t *arr, uint16_t len, char *info) {
 char strTmp[10];
 #define MAX_RESULT_LEN 700
 char strResult[MAX_RESULT_LEN];
 uint16_t i;
 sprintf(strResult, "%s has %d bytes:", info, len);
 for (i=0; i<len; i++) {
  sprintf(strTmp, "%02hx ", arr[i]);
  if (strlen(strResult)<MAX_RESULT_LEN-10) {  
    strcat(strResult, strTmp);
  } else {
    /* does not fit. Just ignore the remaining bytes. */
  }
 }
 addToTrace_chararray(strResult);
} 

/**********************************************************/
/* The global status printer */
void publishStatus(String line1, String line2 = "", String line3 = "") {
  globalLine1=line1;
  globalLine2=line2;
  globalLine3=line3;
}  

void cyclicLcdUpdate(void) {
  uint32_t t;
  uint16_t minutes, seconds;
  String strMinutes, strSeconds, strLine3extended;
  if (counterForDisplayUpdate>0) {
    counterForDisplayUpdate--;  
  } else {
    /* show the uptime in the third line */  
    t = millis()/1000;
    minutes = t / 60;
    seconds = t - (minutes*60);
    strMinutes = String(minutes);
    strSeconds = String(seconds);  
    if (strMinutes.length()<2) strMinutes = "0" + strMinutes;
    if (strSeconds.length()<2) strSeconds = "0" + strSeconds;
    strLine3extended = globalLine3 + " " + strMinutes + ":" + strSeconds;
    hardwareInterface_showOnDisplay(globalLine1, globalLine2, strLine3extended);
    counterForDisplayUpdate=15; /* 15*30ms=450ms until forced cyclic update of the LCD */  
  }
}

void cleanTransmitBuffer(void) {
  /* fill the complete ethernet transmit buffer with 0x00 */
  int i;
  for (i=0; i<MY_ETH_TRANSMIT_BUFFER_LEN; i++) {
    mytransmitbuffer[i]=0;
  }
}

void sendDemoMessageToEthernet(void) {
    mytransmitbufferLen = 60;
    cleanTransmitBuffer();
    // Destination MAC
    mytransmitbuffer[0] = 0xff;
    mytransmitbuffer[1] = 0xff;
    mytransmitbuffer[2] = 0xff;
    mytransmitbuffer[3] = 0xff;
    mytransmitbuffer[4] = 0xff;
    mytransmitbuffer[5] = 0xff;
    // Source MAC
    mytransmitbuffer[6] = 0x11;
    mytransmitbuffer[7] = 0x22;
    mytransmitbuffer[8] = 0x33;
    mytransmitbuffer[9] = 0x44;
    mytransmitbuffer[10] = 0x55;
    mytransmitbuffer[11] = 0x66;
    // Protocol
    mytransmitbuffer[12]=0xBE; // Protocol
    mytransmitbuffer[13]=0xEF; //
    mytransmitbuffer[14]='d';
    mytransmitbuffer[15]='e'; //
    mytransmitbuffer[16]='m'; // 
    mytransmitbuffer[17]='o'; //
    mytransmitbuffer[18]=' '; // 
    mytransmitbuffer[19]='D'; // 
    mytransmitbuffer[20]='E'; //
    mytransmitbuffer[21]='M'; // 
    mytransmitbuffer[22]='O'; // 
    mytransmitbuffer[23]='D'; // 
    mytransmitbuffer[24]='E'; //
    mytransmitbuffer[25]='M'; // 
    mytransmitbuffer[26]='O'; // 
    mytransmitbuffer[27]=' '; // 
    mytransmitbuffer[28]='D'; // 
    mytransmitbuffer[29]='E'; //
    mytransmitbuffer[30]='M'; // 
    mytransmitbuffer[31]='O'; // 
    myEthTransmit();               
}

/**********************************************************/
/* The tasks */

/* This task runs each 30ms. */
void task30ms(void) {
  nCycles30ms++;
  //cyclicLcdUpdate();
  //sanityCheck("cyclic30ms");
}

/* This task runs once a second. */
void task1s(void) {
  if (ledState==0) {
    digitalWrite(PIN_LED,HIGH);
    //Serial.println("LED on");
    ledState = 1;
  } else {
    digitalWrite(PIN_LED,LOW);
    //Serial.println("LED off");
    ledState = 0;
  }
  //log_v("nTotalEthReceiveBytes=%ld, nCycles30ms=%ld", nTotalEthReceiveBytes, nCycles30ms);
  //log_v("nTotalEthReceiveBytes=%ld, nMaxInMyEthernetReceiveCallback=%d, nTcpPacketsReceived=%d", nTotalEthReceiveBytes, nMaxInMyEthernetReceiveCallback, nTcpPacketsReceived);
  //log_v("nTotalTransmittedBytes=%ld", nTotalTransmittedBytes);
  //tcp_testSendData(); /* just for testing, send something with TCP. */
  //sendTestFrame(); /* just for testing, send something on the Ethernet. */
  eatenHeapSpace = initialHeapSpace - ESP.getFreeHeap();
  //Serial.println("EatenHeapSpace=" + String(eatenHeapSpace) + " uwe_rxCounter=" + String(uwe_rxCounter) + " uwe_rxMallocAccumulated=" + String(uwe_rxMallocAccumulated) );
  if (eatenHeapSpace>1000) {
    /* if we lost more than 1000 bytes on heap, print a waring message: */
    Serial.println("WARNING: EatenHeapSpace=" + String(eatenHeapSpace));
  }
  Serial.print("rx bytes");
  Serial.println(nTotalEthReceiveBytes);
  sendDemoMessageToEthernet();
  demoQCA7000();
}

/**********************************************************/
/* The Arduino standard entry points */

void setup() {
  Serial.begin(115200);
  Serial.println("wt32eth01 SPI bridge started.");
  hardwareInterface_initDisplay();
  delay(800); /* wait until the display is up */  
  hardwareInterface_showOnDisplay("2025-10-09", "Hello", "World");
  // Set pin mode
  pinMode(PIN_LED,OUTPUT);
  delay(500); /* wait for power inrush */
  log_v("Initializing the QCA7000...");
  qca7000setup();
  log_v("done.");
  delay(500); /* wait for power inrush */
  if (!initEth()) {
    log_v("Error: Ethernet init failed.");
    hardwareInterface_showOnDisplay("Error", "initEth failed", "check pwr");
    while (1);
  }

  /* The time for the tasks starts here. */
  currentTime = millis();
  lastTime30ms = currentTime;
  lastTime1s = currentTime;
  log_v("Setup finished.");
  initialHeapSpace=ESP.getFreeHeap();
}

void loop() {
  /* a simple scheduler which calls the cyclic tasks depending on system time */
  currentTime = millis();
  if ((currentTime - lastTime30ms)>30) {
    lastTime30ms += 30;
    task30ms();
  }
  if ((currentTime - lastTime1s)>1000) {
    lastTime1s += 1000;
    task1s();
  }
}
