
/* QCA7000 / QCA7005 PLC modem driver at ESP32.
 original source was from: github.com/uhi22/ccs32berta.
 modified for https://github.com/uhi22/wt32eth01-ethernet-to-qca7000-bridge
*/

#include <SPI.h>

/* The QCA7000 is connected via SPI to the ESP32. */
/* SPI pins of the ESP32 HSPI. Using the VSPI crashes the ethernet on WT32-ETH01. */
/* Proposal for the SPI pin use on WT32-ETH01 from
https://community.home-assistant.io/t/wt32-eth01-and-pt100-max31865-spi-pins/807472/4
*/

#define QCA_SPI_CLOCK 14 /* GPIO14 */
#define QCA_SPI_MISO 15  /* GPIO15 */
#define QCA_SPI_MOSI 12  /* GPIO12 */
#define QCA_SPI_SS 4     /* GPIO4 */

SPIClass * qcaspi = NULL;
//static const int spiClk = 2000000; // 2 MHz
static const int spiClk = 500000; // 500kHz


uint8_t mySpiRxBuffer[4000];
uint8_t mySpiTxBuffer[300];
uint32_t nSpiTotalTransmittedBytes;

uint8_t mySpiEthtransmitbuffer[MY_ETH_TRANSMIT_BUFFER_LEN];
uint16_t mySpiEthtransmitbufferLen; /* The number of used bytes in the ethernet transmit buffer */
uint8_t mySpiEthreceivebuffer[MY_ETH_RECEIVE_BUFFER_LEN];
uint16_t mySpiEthreceivebufferLen;


void composeGetSwReq(void) {
  uint8_t i;
	/* GET_SW.REQ request, as used by the win10 laptop */
    mySpiEthtransmitbufferLen = 60;
    for (i=0; i<60; i++) { mySpiEthtransmitbuffer[i] = 0; }
    /* Destination MAC */
    mySpiEthtransmitbuffer[0] = 0xff;
    mySpiEthtransmitbuffer[1] = 0xff;
    mySpiEthtransmitbuffer[2] = 0xff;
    mySpiEthtransmitbuffer[3] = 0xff;
    mySpiEthtransmitbuffer[4] = 0xff;
    mySpiEthtransmitbuffer[5] = 0xff;
    /* Source MAC */
    mySpiEthtransmitbuffer[6] = 0xFE;
    mySpiEthtransmitbuffer[7] = 0xED;
    mySpiEthtransmitbuffer[8] = 0xBE;
    mySpiEthtransmitbuffer[9] = 0xEF;
    mySpiEthtransmitbuffer[10] = 0x55;
    mySpiEthtransmitbuffer[11] = 0x66;
    /* Protocol */
    mySpiEthtransmitbuffer[12]=0x88; // Protocol HomeplugAV
    mySpiEthtransmitbuffer[13]=0xE1; //
    mySpiEthtransmitbuffer[14]=0x00; // version
    mySpiEthtransmitbuffer[15]=0x00; // GET_SW.REQ
    mySpiEthtransmitbuffer[16]=0xA0; // 
    mySpiEthtransmitbuffer[17]=0x00; // Vendor OUI
    mySpiEthtransmitbuffer[18]=0xB0; // 
    mySpiEthtransmitbuffer[19]=0x52; //  
}


void qca7000setup() {
  /* initialise instance of the SPIClass attached to VSPI */
  qcaspi = new SPIClass(HSPI);
  qcaspi->begin(QCA_SPI_CLOCK, QCA_SPI_MISO, QCA_SPI_MOSI, QCA_SPI_SS);
  /* set up slave select pins as outputs as the Arduino API doesn't handle
     automatically pulling SS low */
  pinMode(qcaspi->pinSS(), OUTPUT);
}

void spiQCA7000DemoReadSignature(void) {
  /* Demo for reading the signature of the QCA7000. This should show AA55. */
  uint16_t sig;
  qcaspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE3));
  digitalWrite(qcaspi->pinSS(), LOW);
  (void)qcaspi->transfer(0xDA); /* Read, internal, reg 1A (SIGNATURE) */
  (void)qcaspi->transfer(0x00);
  sig = qcaspi->transfer(0x00);
  sig <<= 8;
  sig += qcaspi->transfer(0x00);
  digitalWrite(qcaspi->pinSS(), HIGH);
  qcaspi->endTransaction();
  Serial.println("SIGNATURE (should be AA 55) " + String(sig, HEX));  /* should be AA 55  */
}

void spiQCA7000DemoReadWRBUF_SPC_AVA(void) {
  /* Demo for reading the available write buffer size from the QCA7000 */
  int i;
  qcaspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE3));
  digitalWrite(qcaspi->pinSS(), LOW);
  mySpiRxBuffer[0] = qcaspi->transfer(0xC2);
  mySpiRxBuffer[1] = qcaspi->transfer(0x00);
  mySpiRxBuffer[2] = qcaspi->transfer(0x00);
  mySpiRxBuffer[3] = qcaspi->transfer(0x00);
  digitalWrite(qcaspi->pinSS(), HIGH);
  qcaspi->endTransaction();
  String s;
  s = "WRBUF_SPC_AVA: ";
  for (i=0; i<4; i++) {
    s = s + String(mySpiRxBuffer[i], HEX) + " ";
  }
  Serial.println(s);  
}


void spiQCA7000DemoWriteBFR_SIZE(uint16_t n) {
  /* Demo for writing the write buffer size to the QCA7000 */
  qcaspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE3));
  digitalWrite(qcaspi->pinSS(), LOW);
  (void) qcaspi->transfer(0x41); /* 0x41 is write, internal, reg 1 */
  (void) qcaspi->transfer(0x00);
  (void) qcaspi->transfer(n>>8);
  (void) qcaspi->transfer(n);
  digitalWrite(qcaspi->pinSS(), HIGH);
  qcaspi->endTransaction(); 
}


uint16_t spiQCA7000DemoReadRDBUF_BYTE_AVA(void) {
  /* Demo for retrieving the amount of available received data from QCA7000 */
  uint16_t n;
  qcaspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE3));
  digitalWrite(qcaspi->pinSS(), LOW);
  (void)qcaspi->transfer(0xC3); /* 0xC3 is read, internal, reg 3 RDBUF_BYTE_AVA */
  (void)qcaspi->transfer(0x00);
  n = qcaspi->transfer(0x00); /* upper byte of the size */
  n<<=8;  
  n+=qcaspi->transfer(0x00); /* lower byte of the size */
  digitalWrite(qcaspi->pinSS(), HIGH);
  qcaspi->endTransaction();

  Serial.println("RDBUF_BYTE_AVA: " + String(n));  
  return n;
}


void QCA7000checkRxDataAndDistribute(int16_t availbytes) {
  uint16_t  L1, L2;
  uint8_t *p;
  uint8_t  blDone = 0; 
  uint8_t counterOfEthFramesInSpiFrame;
  counterOfEthFramesInSpiFrame = 0;
  p= mySpiRxBuffer;
  while (!blDone) {  /* The SPI receive buffer may contain multiple Ethernet frames. Run trough all. */
      /* the SpiRxBuffer contains more than the ethernet frame:
        4 byte length
        4 byte start of frame AA AA AA AA
        2 byte frame length, little endian
        2 byte reserved 00 00
        payload
        2 byte End of frame, 55 55 */
      /* The higher 2 bytes of the len are assumed to be 0. */
      /* The lower two bytes of the "outer" len, big endian: */       
      L1 = p[2]; L1<<=8; L1+=p[3];
      /* The "inner" len, little endian. */
      L2 = p[9]; L2<<=8; L2+=p[8];
      if ((p[4]=0xAA) && (p[5]=0xAA) && (p[6]=0xAA) && (p[7]=0xAA) 
            && (L2+10==L1)) {
          counterOfEthFramesInSpiFrame++;
          /* The start of frame and the two length informations are plausible. Copy the payload to the eth receive buffer. */
          mySpiEthreceivebufferLen = L2;
          /* but limit the length, to avoid buffer overflow */       
          if (mySpiEthreceivebufferLen > MY_ETH_RECEIVE_BUFFER_LEN) {
              mySpiEthreceivebufferLen = MY_ETH_RECEIVE_BUFFER_LEN;
          }
          memcpy(mySpiEthreceivebuffer, &p[12], mySpiEthreceivebufferLen);
          /* We received an ethernet package. Determine its type, and dispatch it to the related handler. */
          #ifdef VERBOSE_QCA7000
            showAsHex(mySpiEthreceivebuffer, mySpiEthreceivebufferLen, "eth.mySpiEthreceivebuffer");
          #endif
          routeReceivedDataFromQcaToEthernet();
          availbytes = availbytes - L1 - 4;
          p+= L1+4;
          //Serial.println("Avail after first run:" + String(availbytes));
          if (availbytes>10) { /*
            Serial.println("There is more data."); 
            Serial.print(String(p[0], HEX) + " ");
            Serial.print(String(p[1], HEX) + " ");
            Serial.print(String(p[2], HEX) + " ");
            Serial.print(String(p[3], HEX) + " ");
            Serial.print(String(p[4], HEX) + " ");
            Serial.print(String(p[5], HEX) + " ");
            Serial.print(String(p[6], HEX) + " ");
            Serial.print(String(p[7], HEX) + " ");
            Serial.print(String(p[8], HEX) + " ");
            Serial.print(String(p[9], HEX) + " ");
            */
          } else {
            blDone=1;
          }
    } else {
        /* no valid header -> end */
        blDone=1;      
    }         
  }
  #ifdef VERBOSE_QCA7000
    Serial.println("QCA7000: The SPI frame contained " + String(counterOfEthFramesInSpiFrame) + " ETH frames.");
  #endif
}

void spiQCA7000checkForReceivedData(void) {
  /* checks whether the QCA7000 indicates received data, and if yes, fetches the data. */
  uint16_t availBytes;
  uint16_t i;
  availBytes = spiQCA7000DemoReadRDBUF_BYTE_AVA();
  if (availBytes>0) {
    #ifdef VERBOSE_QCA7000
      Serial.println("QCA7000 avail RX bytes: " + String(availBytes));
    #endif
    /* If the QCA indicates that the receive buffer contains data, the following procedure
    is necessary to get the data (according to https://chargebyte.com/assets/Downloads/an4_rev5.pdf)
       - write the BFR SIZE, this sets the length of data to be read via external read
       - start an external read and receive as much data as set in SPI REG BFR SIZE before */
    spiQCA7000DemoWriteBFR_SIZE(availBytes);
    qcaspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE3));
    digitalWrite(qcaspi->pinSS(), LOW);
    (void)qcaspi->transfer(0x80); /* 0x80 is read, external */
    (void)qcaspi->transfer(0x00);
    for (i=0; i<availBytes; i++) {
      mySpiRxBuffer[i] = qcaspi->transfer(0x00); /* loop over all the receive data */
    }
    digitalWrite(qcaspi->pinSS(), HIGH);
    qcaspi->endTransaction();     
    QCA7000checkRxDataAndDistribute(availBytes); /* Takes the data from the SPI rx buffer, splits it into ethernet frames and distributes them. */
  }
}

void spiQCA7000SendEthFrame(void) {
  /* to send an ETH frame, we need two steps:
     1. Write the BFR_SIZE (internal reg 1)
     2. Write external, preamble, size, data */
/* Example (from CCM)
  The SlacParamReq has 60 "bytes on wire" (in the Ethernet terminology).
  The   BFR_SIZE is set to 0x46 (command 41 00 00 46). This is 70 decimal.
  The transmit command on SPI is
  00 00  
  AA AA AA AA
  3C 00 00 00    (where 3C is 60, matches the "bytes on wire")
  <60 bytes payload>
  55 55 After the footer, the frame is finished according to the qca linux driver implementation.
  xx yy But the Hyundai CCM sends two bytes more, either 00 00 or FE 80 or E1 FF or other. Most likely not relevant.
  Protocol explanation from https://chargebyte.com/assets/Downloads/an4_rev5.pdf 
*/
  /* Todo:
    1. Check whether the available transmit buffer size is big enough to get the intended frame.
       If not, this is an error situation, and we need to instruct the QCA to heal, e.g. by resetting it.
  */
  String s;
  Serial.println("mySpiEthtransmitbufferLen " + String(mySpiEthtransmitbufferLen));
  spiQCA7000DemoWriteBFR_SIZE(mySpiEthtransmitbufferLen+10); /* The size in the BFR_SIZE is 10 bytes more than in the size after the preamble below (in the original CCM trace) */
  mySpiTxBuffer[0] = 0x00; /* external write command */
  mySpiTxBuffer[1] = 0x00;
  mySpiTxBuffer[2] = 0xAA; /* Start of frame */
  mySpiTxBuffer[3] = 0xAA;
  mySpiTxBuffer[4] = 0xAA;
  mySpiTxBuffer[5] = 0xAA;
  mySpiTxBuffer[6] = (uint8_t)mySpiEthtransmitbufferLen; /* LSB of the length */
  mySpiTxBuffer[7] = mySpiEthtransmitbufferLen>>8; /* MSB of the length */
  mySpiTxBuffer[8] = 0x00; /* to bytes reserved, 0x00 */
  mySpiTxBuffer[9] = 0x00;
  memcpy(&mySpiTxBuffer[10], mySpiEthtransmitbuffer, mySpiEthtransmitbufferLen); /* the ethernet frame */
  mySpiTxBuffer[10+mySpiEthtransmitbufferLen] = 0x55; /* End of frame, 2 bytes with 0x55. Aka QcaFrmCreateFooter in the linux driver */
  mySpiTxBuffer[11+mySpiEthtransmitbufferLen] = 0x55;
  int i;
  qcaspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE3));
  digitalWrite(qcaspi->pinSS(), LOW);
  for (i=0; i<12+mySpiEthtransmitbufferLen; i++) {
    (void) qcaspi->transfer(mySpiTxBuffer[i]);
    s = s + String(mySpiTxBuffer[i], HEX) + " ";
  }
  digitalWrite(qcaspi->pinSS(), HIGH);
  qcaspi->endTransaction();
  Serial.println(s);
}

void demoQCA7000SendSoftwareVersionRequest(void) {
  Serial.println("preparing GetSwReq");
  composeGetSwReq();
  Serial.println("sending GetSwReq");
  spiQCA7000SendEthFrame(); 
}

void demoQCA7000(void) {
  spiQCA7000DemoReadSignature();
  spiQCA7000DemoReadWRBUF_SPC_AVA();
  demoQCA7000SendSoftwareVersionRequest();
  spiQCA7000DemoReadWRBUF_SPC_AVA();
  spiQCA7000checkForReceivedData();
  spiQCA7000checkForReceivedData();
}
