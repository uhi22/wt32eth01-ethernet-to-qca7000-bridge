
//extern int uart_write_bytes(int uart_num, const void* src, size_t size);

/* Logging verbosity settings */
#define VERBOSE_INIT_ETH
#define VERBOSE_QCA7000

/* Ethernet */
#define MY_ETH_TRANSMIT_BUFFER_LEN 1000
#define MY_ETH_RECEIVE_BUFFER_LEN 1000
extern uint32_t nTotalEthReceiveBytes; /* total number of bytes which has been received from the ethernet port */
extern uint32_t nTotalTransmittedBytes;
extern uint8_t mytransmitbuffer[MY_ETH_TRANSMIT_BUFFER_LEN];
extern uint16_t mytransmitbufferLen; /* The number of used bytes in the ethernet transmit buffer */
extern uint8_t myreceivebuffer[MY_ETH_RECEIVE_BUFFER_LEN];
extern uint16_t myreceivebufferLen;
extern uint8_t myMAC[6];
extern uint8_t nMaxInMyEthernetReceiveCallback, nInMyEthernetReceiveCallback;
extern uint8_t isEthLinkUp;

/* Ethernet-to-QCA hand-off (myEthernetReceiveCallback(), which runs in the esp_eth
   driver's own task -> task30ms()/routeReceivedDataFromEthernetToQca(), the main loop,
   which owns all SPI access to the QCA): a small fixed-depth ring buffer (depth 2 =
   double buffering, so one frame can be queued while a previous one is still being sent
   over SPI - two back-to-back frames survive without loss). Always accessed under
   qcaTxMux, a short spinlock held only for a memcpy/index update, never across the
   actual SPI transfer. If the queue is full, a new frame is dropped rather than
   overwriting a slot still being read - losing a frame is fine, corrupting one being
   copied out is not. */
#define ETH_TO_QCA_QUEUE_DEPTH 2
extern portMUX_TYPE qcaTxMux;
extern uint8_t ethToQcaQueueBuffer[ETH_TO_QCA_QUEUE_DEPTH][MY_ETH_TRANSMIT_BUFFER_LEN];
extern uint16_t ethToQcaQueueBufferLen[ETH_TO_QCA_QUEUE_DEPTH];
extern uint8_t ethToQcaWriteIdx;
extern uint8_t ethToQcaReadIdx;
extern uint8_t ethToQcaPendingCount;
extern uint32_t nDroppedEthFramesForQca;
extern uint32_t nFramesEthToQca; /* frames actually sent to the QCA via SPI */
extern uint32_t nFramesQcaToEth; /* frames actually sent to the Ethernet port */

/* QCA buffers */
extern uint8_t mySpiEthtransmitbuffer[MY_ETH_TRANSMIT_BUFFER_LEN];
extern uint16_t mySpiEthtransmitbufferLen; /* The number of used bytes in the ethernet transmit buffer */
extern uint8_t mySpiEthreceivebuffer[MY_ETH_RECEIVE_BUFFER_LEN];
extern uint16_t mySpiEthreceivebufferLen;

/* from ETH.h */
//Dedicated GPIOs for RMII
#define ETH_RMII_TX_EN  21
#define ETH_RMII_TX0    19
#define ETH_RMII_TX1    22
#define ETH_RMII_RX0    25
#define ETH_RMII_RX1_EN 26
#define ETH_RMII_CRS_DV 27

/* functions */
#if defined(__cplusplus)
extern "C"
{
#endif
void addToTrace_chararray(char *s);
#if defined(__cplusplus)
}
#endif


/* some snippets from ETH.h and ETH.cpp */
#include "esp_system.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_eth_phy.h"
#include "esp_eth_mac.h"
/* does it make sense to include the periman??? */
#include "esp32-hal-periman.h"

/* from working ethernet example code in oct 2025 */
#define ETH_PHY_TYPE    ETH_PHY_LAN8720
#define ETH_PHY_ADDR    1
#define ETH_PHY_POWER   16
#define ETH_PHY_MDC     23
#define ETH_PHY_MDIO    18
#define ETH_CLK_MODE    ETH_CLOCK_GPIO0_IN
#define ETH_RMII_TX_EN  21
#define ETH_RMII_TX0    19
#define ETH_RMII_TX1    22
#define ETH_RMII_RX_ER  13
#define ETH_RMII_RX0    25
#define ETH_RMII_RX1    26
#define ETH_RMII_CRS_DV 27



#ifndef ETH_CLK_MODE
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
#endif

typedef enum { ETH_CLOCK_GPIO0_IN, ETH_CLOCK_GPIO0_OUT, ETH_CLOCK_GPIO16_OUT, ETH_CLOCK_GPIO17_OUT } eth_clock_mode_t;



