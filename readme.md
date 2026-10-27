# Ethernet to SPI bridge using WT32-ETH01

## Goal

This project intends to use the ESP32-based WT32-ETH01 board
to build a bridge between Ethernet and the SPI of
a Qualcomm QCA7000/7005.

## News / Change history / functional status

### 2026-10-27 Latency: round trip ~50 ms -> expected a few ms (compiled, NOT yet flashed/measured)

Measured with tcpdump on the host side of the bridge (60 GET_SW requests, all answered): request-to-reply
min 22 / median 50 / p90 62 / max 160 ms. Causes found in the code, and the changes:
- Data path ran on the 30 ms tick (a request waited 0-30 ms, the answer up to 30 ms more). Now
  `spiService()` runs on every `loop()` pass: Ethernet->QCA queue drained immediately, QCA polled every 500 us.
- Serial output in the data path (a hex dump of every sent frame built with `String` concatenation, ~18 ms per
  60 bytes at 115200 baud; two lines per received SPI read) removed; `VERBOSE_QCA7000` is off by default.
- SPI: 1 MHz byte-by-byte -> 2 MHz with `writeBytes()`/`transferBytes()` bursts (4 MHz worked but the edges were marginal with series resistors) (`spiClk` in `qca7000.ino`; the
  QCA7000 allows up to 12 MHz, raise it if the wiring holds).
- Buffers 1000 -> 1536 bytes (full 1514-byte frames), Ethernet->QCA queue depth 2 -> 8.
- Bug fixed: the start-of-frame check used `=` instead of `==` (`p[4]=0xAA`), so it always passed and overwrote the
  buffer; frames were accepted on the length check alone. Now compared for real, plus bounds checks (frame inside
  the data read, read size limited to the receive buffer).
- Activity LED (IO17, QCA->host traffic): on-time per SPI read 150 ms -> 20 ms (`LED_ACTIVITY_MS`), timed every
  loop pass instead of in the 30 ms tick, so single frames show as short flashes and bursts no longer merge.
- `task5s()` serial line now also prints `maxSpiServiceUs`, the longest single receive-and-forward.

### 2026-09-09 Ethernet-to-SPI routing implemented, and a real promiscuous-mode bug fixed

Ethernet->QCA routing is implemented: `myEthernetReceiveCallback()` (in the esp_eth
driver's own task) hands received frames to `routeReceivedDataFromEthernetToQca()` (the
main loop, which owns all SPI access) via a small depth-2 ring buffer
(`ethToQcaQueueBuffer`/`ethToQcaQueueBufferLen`/`ethToQcaWriteIdx`/`ethToQcaReadIdx`/
`ethToQcaPendingCount`), guarded by a short spinlock (`qcaTxMux`) held only for the
memcpy/index update, never across the actual SPI transfer - so a burst of back-to-back
Ethernet frames can queue one deep while a previous one is in flight, and the queue drops
a new frame rather than overwriting one still being read (losing a frame is fine,
corrupting one is not). `MY_ETH_TRANSMIT_BUFFER_LEN`/`MY_ETH_RECEIVE_BUFFER_LEN` bumped
from 250 to 1000 bytes to comfortably cover normal background OS traffic (DHCP, NetBIOS,
etc.) instead of truncating it.

**Real bug found and fixed**: the board's Ethernet MAC was never put into promiscuous
mode, so it silently dropped every frame not addressed to its own interface MAC or
broadcast - meaning no unicast-addressed traffic (e.g. any HomePlugAV command targeting
the QCA's own MAC, as opposed to a broadcast query) could ever reach the QCA through this
bridge, regardless of what's on the other side of the Ethernet link. Fixed in
`initEth()` (`ethernet.ino`) via `esp_eth_ioctl(eth_handle, ETH_CMD_S_PROMISCUOUS,
&promiscuous)` right after `esp_eth_start()`. This was initially mistaken for a
switch/hub problem (broadcast frames passed through fine, masking the real cause) before
being traced to the missing ioctl call.

Also added per-direction frame counters (`nFramesEthToQca`/`nFramesQcaToEth`, plus the
existing `nDroppedEthFramesForQca`), printed to serial every 5 seconds via a new
`task5s()` - useful for confirming traffic is actually flowing in both directions during
debugging, rather than inferring it from higher-level tool behavior alone.

Confirmed working end-to-end: used from a Raspberry Pi running a patched `plctool`
(`open-plc-utils`) over this bridge to reach a real QCA7000 DUT, with real
request/response HomePlugAV exchanges observed on both directions' counters.

## Todos
- [x] find pins for SPI
- [x] implement routing from Ethernet to SPI
- [x] implement routing from SPI to Ethernet
- [ ] consider raising `MY_ETH_TRANSMIT_BUFFER_LEN`/`MY_ETH_RECEIVE_BUFFER_LEN` further, or handling frames near the full 1518-byte Ethernet MTU, if a use case needs it

## References

- Ref1 Discussion about SPI pins on WT32-ETH01: https://community.home-assistant.io/t/wt32-eth01-and-pt100-max31865-spi-pins/807472/3
