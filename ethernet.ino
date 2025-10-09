  
/* The Ethernet low-level stuff */

/* Version problem: The struct eth_mac_config_t (and most likely a lot more) have been changed by Espressif.
   Original version of the raw ethernet which has been working with https://github.com/uhi22/ccs32 in 2023 does not compile
   anymore with the newer Arduino/IDF version in 2025.
   The espressif side of the new version is located here:
   C:\Users\uwemi\AppData\Local\Arduino15\packages\esp32\tools\esp32-arduino-libs\idf-release_v5.1-bd2b9390ef\esp32\include\esp_eth\include
   and
   C:\Users\uwemi\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.0.2\libraries\Ethernet\src
*/ 

static eth_clock_mode_t eth_clock_mode = ETH_CLK_MODE;
esp_eth_handle_t eth_handle;
uint8_t isEthLinkUp;
uint8_t nFailedEthTransmissions;

/*********************************************/

uint32_t nTotalEthReceiveBytes; /* total number of bytes which has been received from the ethernet port */
uint32_t nTotalTransmittedBytes;
uint8_t mytransmitbuffer[MY_ETH_TRANSMIT_BUFFER_LEN];
uint8_t mytransmitbufferLen=0; /* The number of used bytes in the ethernet transmit buffer */
uint8_t myreceivebuffer[MY_ETH_RECEIVE_BUFFER_LEN];
uint16_t myreceivebufferLen;
uint8_t myMAC[6] = {0xDC, 0x0e, 0xa1, 0x11, 0x67, 0x09}; /* just a default MAC address. Will be overwritten by the PHY MAC. */
uint8_t nMaxInMyEthernetReceiveCallback, nInMyEthernetReceiveCallback;
uint16_t nTcpPacketsReceived;

/* based on template in WiFiGeneric.cpp, function _arduino_event_cb() */
void myEthernetEventCallback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_CONNECTED) {
		log_v("Ethernet Link Up");
    isEthLinkUp = 1; 
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_DISCONNECTED) {
		log_v("Ethernet Link Down");
    isEthLinkUp = 0; 
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_START) {
		log_v("Ethernet Started");
    isEthLinkUp = 0; 
	} else if (event_base == ETH_EVENT && event_id == ETHERNET_EVENT_STOP) {
		log_v("Ethernet Stopped");
    isEthLinkUp = 0; 
  }    
}

/* The receive function, which is called by the esp32-ethernet-driver. */
esp_err_t myEthernetReceiveCallback(esp_eth_handle_t hdl, uint8_t *buffer, uint32_t length, void *priv) {
  nInMyEthernetReceiveCallback++;
  if (nInMyEthernetReceiveCallback>nMaxInMyEthernetReceiveCallback) nMaxInMyEthernetReceiveCallback = nInMyEthernetReceiveCallback;
  nTotalEthReceiveBytes+=length;
  /* We received an ethernet package. Determine its type, and dispatch it to the related handler. */
  //uint16_t etherType = getEtherType(buffer);
  //Serial.println("EtherType" + String(etherType, HEX) + " size " + String(length));   
  uint32_t L;
  sanityCheck("Start of eth rx");
  L=length;
  if (L>=MY_ETH_RECEIVE_BUFFER_LEN) {
    addToTrace("Ethernet rx: limiting " + String(L) + " to " + String(MY_ETH_RECEIVE_BUFFER_LEN) + " bytes");
    L=MY_ETH_RECEIVE_BUFFER_LEN;
  }
  memcpy(myreceivebuffer, buffer, L); /* possible improvement: copy of buffer is not really necessary. We could directly work
                                         on the buffer provided by the ethernet driver. This was allocated especially for each
                                         single received message, and will be present until the application frees it. */ 
  myreceivebufferLen=L;
  sanityCheck("Step2 of eth rx"); 
  //showAsHex(myreceivebuffer, myreceivebufferLen, "eth.myreceivebuffer");   
  //if (etherType == 0x88E1) { /* it is a HomePlug message */
  //  Serial.println("Its a HomePlug message.");
    //evaluateReceivedHomeplugPacket();
  //} else if (etherType == 0x86dd) { /* it is an IPv6 frame */
  //  Serial.println("Its a IPv6 message.");
    //ipv6_evaluateReceivedPacket();
  //} else {
    //Serial.println("Other message.");
  //}
  sanityCheck("End of eth rx");
  nInMyEthernetReceiveCallback--;
  free(buffer); /* We need to free the buffer, because the driver will NOT do this (at least in Arduino 2.0.4 with esp-idf4.4.4) */
  return ESP_OK;       
}

/* The Ethernet transmit function. */
void myEthTransmit(void) {
  uint16_t retval;
  nTotalTransmittedBytes += mytransmitbufferLen;
  showAsHex(mytransmitbuffer, mytransmitbufferLen, "myEthTransmit");
  retval = esp_eth_transmit(eth_handle, mytransmitbuffer, mytransmitbufferLen);
  if (retval!=ESP_OK) {
    addToTrace("esp_eth_transmit went wrong, " + String(retval));
    nFailedEthTransmissions++;  
    addToTrace("In total " + String(nFailedEthTransmissions) + " failed transmissions.");
    if (nFailedEthTransmissions>=4) {
      addToTrace("Restarting the Ethernet driver");
      (void)esp_eth_stop(eth_handle); 
      (void)esp_eth_start(eth_handle);      
    }      
  } else {
    nFailedEthTransmissions=0;
  }
}

/* The Ethernet initialization function.
   Based on code snippets from ETH.cpp ETHClass::begin */
bool initEth(void) {
  uint8_t i;
  int rc;
  uint8_t phy_addr=ETH_PHY_ADDR;
  int power=ETH_PHY_POWER;
  int mdc=ETH_PHY_MDC;
  int mdio=ETH_PHY_MDIO;
  int pin_mcd, pin_mdio, pin_rmii_clock, pin_power;

  #ifdef VERBOSE_INIT_ETH
    log_v("This is initEth.");
  #endif  
   #ifdef VERBOSE_INIT_ETH
    log_v("periman de-init section.");
  #endif

  eth_esp32_emac_config_t mac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
  mac_config.clock_config.rmii.clock_mode = (eth_clock_mode) ? EMAC_CLK_OUT : EMAC_CLK_EXT_IN;
  mac_config.clock_config.rmii.clock_gpio = (1 == eth_clock_mode) ? EMAC_APPL_CLK_OUT_GPIO : (2 == eth_clock_mode) ? EMAC_CLK_OUT_GPIO : (3 == eth_clock_mode) ? EMAC_CLK_OUT_180_GPIO : EMAC_CLK_IN_GPIO;
  mac_config.smi_mdc_gpio_num = digitalPinToGPIONumber(mdc); /* new */
  mac_config.smi_mdio_gpio_num = digitalPinToGPIONumber(mdio); 

  pin_mcd = digitalPinToGPIONumber(mdc);
  pin_mdio = digitalPinToGPIONumber(mdio);
  pin_rmii_clock = mac_config.clock_config.rmii.clock_gpio;
  pin_power = digitalPinToGPIONumber(power);
  
  if (!perimanClearPinBus(pin_rmii_clock)) {
    return false;
  }
  if (!perimanClearPinBus(pin_mcd)) {
    return false;
  }
  if (!perimanClearPinBus(pin_mdio)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_TX_EN)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_TX0)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_TX1)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_RX0)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_RX1_EN)) {
    return false;
  }
  if (!perimanClearPinBus(ETH_RMII_CRS_DV)) {
    return false;
  }
  if (pin_power != -1) {
    if (!perimanClearPinBus(pin_power)) {
      return false;
    }
  }

  eth_mac_config_t eth_mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_mac_config.sw_reset_timeout_ms = 1000;

  #ifdef VERBOSE_INIT_ETH
    log_v("calling esp_eth_mac_new_esp32");
  #endif      

  esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&mac_config, &eth_mac_config);
  if (mac == NULL) {
    log_e("esp_eth_mac_new_esp32 failed");
    return false;
  }
  #ifdef VERBOSE_INIT_ETH
    log_v("done");
  #endif  



  //todo porting mac_config.rx_task_stack_size = 4096; /* discussed in https://esp32.com/viewtopic.php?t=26603. Default 2048 may cause stack canary failure. */
  
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr = phy_addr;
  phy_config.reset_gpio_num = pin_power;
  esp_eth_phy_t *phy = NULL;
    
  #ifdef VERBOSE_INIT_ETH    
    log_v("calling esp_eth_phy_new_lan87xx");
  #endif      
  phy = esp_eth_phy_new_lan87xx(&phy_config);
  if(phy == NULL){
    log_e("esp_eth_phy_new_lan87xx failed");
    return false;
  }
  #ifdef VERBOSE_INIT_ETH    
    log_v("done");
  #endif
  
  eth_handle = NULL;
  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
  #ifdef VERBOSE_INIT_ETH
    log_v("Calling esp_eth_driver_install.");
  #endif
  
  if(esp_eth_driver_install(&eth_config, &eth_handle) != ESP_OK || eth_handle == NULL){
    log_e("esp_eth_driver_install failed");
    return false;
  }
  #ifdef VERBOSE_INIT_ETH
    log_v("done");
  #endif

  #ifdef VERBOSE_INIT_ETH    
    log_v("creating event loop");
  #endif
  esp_err_t err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
  	log_e("esp_event_loop_create_default failed!");
    return false;
  }
  //log_v("done");

  /* registering the event callback from the ethernet driver */
  #ifdef VERBOSE_INIT_ETH
    log_v("registering the event callback from the ethernet driver.");
  #endif
  rc = esp_event_handler_instance_register(ETH_EVENT, ESP_EVENT_ANY_ID, &myEthernetEventCallback, NULL, NULL);
  //log_v("returned %d", rc);    
  if(rc) {
        log_e("event_handler_instance_register for ETH_EVENT Failed! rc %d", rc);
        return false;
  }
  //log_v("done");

  #ifdef VERBOSE_INIT_ETH    
    log_v("registering the receive callback from the ethernet driver.");
  #endif
  rc = esp_eth_update_input_path(eth_handle, &myEthernetReceiveCallback, NULL);
  //log_v("returned %d", rc);    
  if(rc) {
      log_e("esp_eth_update_input_path Failed! rc %d", rc);
      return false;
  }
  //log_v("done");

  /* starting the ethernet driver in standalone mode, means without TCP/IP etc. */
  #ifdef VERBOSE_INIT_ETH
    log_v("starting the ethernet driver in standalone mode.");
  #endif  
  if(esp_eth_start(eth_handle) != ESP_OK){
    log_e("esp_eth_start failed");
    return false;
  }
  //log_v("esp_eth_start done");

  //log_v("requesting MAC."); 
  rc = esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, myMAC);
  log_v("myMAC %hx:%hx:%hx:%hx:%hx:%hx", myMAC[0], myMAC[1], myMAC[2], myMAC[3], myMAC[4], myMAC[5]); 
        
  // holds a few milliseconds to enter into a good state
  // FIX ME -- adresses issue https://github.com/espressif/arduino-esp32/issues/5733
  delay(50);
  return true; 
}

