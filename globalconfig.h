
//extern int uart_write_bytes(int uart_num, const void* src, size_t size);

/* Logging verbosity settings */
//#define VERBOSE_INIT_ETH

/* Ethernet */
#define MY_ETH_TRANSMIT_BUFFER_LEN 250
#define MY_ETH_RECEIVE_BUFFER_LEN 250
extern uint32_t nTotalEthReceiveBytes; /* total number of bytes which has been received from the ethernet port */
extern uint32_t nTotalTransmittedBytes;
extern uint8_t mytransmitbuffer[MY_ETH_TRANSMIT_BUFFER_LEN];
extern uint8_t mytransmitbufferLen; /* The number of used bytes in the ethernet transmit buffer */
extern uint8_t myreceivebuffer[MY_ETH_RECEIVE_BUFFER_LEN];
extern uint16_t myreceivebufferLen;
extern uint8_t myMAC[6];
extern uint8_t nMaxInMyEthernetReceiveCallback, nInMyEthernetReceiveCallback;
extern uint8_t isEthLinkUp;



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

#define ETH_PHY_ADDR 1 /* from pins_arduino.h of wt32-eth01 */
#define ETH_PHY_TYPE ETH_PHY_LAN8720

#ifndef ETH_PHY_MDC
#define ETH_PHY_MDC 23
#endif

#ifndef ETH_PHY_MDIO
#define ETH_PHY_MDIO 18
#endif

#ifndef ETH_CLK_MODE
#define ETH_CLK_MODE ETH_CLOCK_GPIO0_IN
#endif

typedef enum { ETH_CLOCK_GPIO0_IN, ETH_CLOCK_GPIO0_OUT, ETH_CLOCK_GPIO16_OUT, ETH_CLOCK_GPIO17_OUT } eth_clock_mode_t;



