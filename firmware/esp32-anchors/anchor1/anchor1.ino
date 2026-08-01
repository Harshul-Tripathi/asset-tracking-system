/*
 * UWB Anchor 1 - UDP to STM32 Hub
 */
#include <SPI.h>
#include "dw3000.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFi.h>
#include <WiFiUdp.h>

#define PIN_RST 27
#define PIN_IRQ 34
#define PIN_SS  5

#define RNG_DELAY_MS                500  // Delay between pings
#define TX_ANT_DLY                  16385
#define RX_ANT_DLY                  16385
#define ALL_MSG_COMMON_LEN          10
#define ALL_MSG_SN_IDX              2
#define RESP_MSG_POLL_RX_TS_IDX     10
#define RESP_MSG_RESP_TX_TS_IDX     14
#define POLL_TX_TO_RESP_RX_DLY_UUS  300
#define RESP_RX_TIMEOUT_UUS         3000

// Filter settings
#define FILTER_SIZE     5
#define MAX_JUMP_M      0.5
#define MAX_VALID_M     20.0

const char* ssid = "UWB_HUB";
const char* password = "12345678";

WiFiUDP udp;
const int udpPort = 5000;

static dwt_config_t config = {
    5, DWT_PLEN_128, DWT_PAC8, 9, 9, 1,
    DWT_BR_6M8, DWT_PHRMODE_STD, DWT_PHRRATE_STD,
    (129 + 8 - 8), DWT_STS_MODE_OFF, DWT_STS_LEN_64, DWT_PDOA_M0
};

static uint8_t tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W','A','V','1', 0xE0, 0, 0};
static uint8_t rx_resp_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'V','E','W','1', 0xE1, 0,0,0,0,0,0,0,0,0,0};

static uint8_t frame_seq_nb = 0;
static uint8_t rx_buffer[20];
static uint32_t status_reg = 0;
static double distance = 0;

// Filter state
static double dist_history[FILTER_SIZE] = {0};
static int hist_idx    = 0;
static int hist_filled = 0;
static double last_valid = -1.0;

extern dwt_txconfig_t txconfig_options;

int reject_count  = 0;

// Returns filtered distance, or -1 if rejected
double applyFilter(double raw) {
    if (raw <= 0 || raw > MAX_VALID_M) {
        reject_count++;
        return -1.0;
    }
    if (last_valid > 0 && fabs(raw - last_valid) > MAX_JUMP_M) {
        reject_count++;
        return -1.0;
    }
    last_valid = raw;

    dist_history[hist_idx] = raw;
    hist_idx = (hist_idx + 1) % FILTER_SIZE;
    if (hist_filled < FILTER_SIZE) hist_filled++;

    double sum = 0;
    for (int i = 0; i < hist_filled; i++) sum += dist_history[i];
    return sum / hist_filled;
}

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // disable brownout
    Serial.begin(115200);
    delay(1000);

    Serial.println("=====================================");
    Serial.println("         I AM ANCHOR 1");
    Serial.println("=====================================");

    Serial.println("Connecting to STM32 Wi-Fi Hub (OPEN/STATIC IP)...");
    
    // Erase old credentials in case it's trying to use the old password
    WiFi.disconnect(true, true); 
    delay(100);
    
    WiFi.mode(WIFI_STA);
    
    IPAddress local_IP(10, 19, 96, 11);
    IPAddress gateway(10, 19, 96, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.config(local_IP, gateway, subnet);
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to Hub Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // MUST initialize UDP socket!
    udp.begin(udpPort);

    UART_init();
    spiBegin(PIN_IRQ, PIN_RST);
    spiSelect(PIN_SS);
    delay(2);

    uint32_t dev_id = dwt_readdevid();
    Serial.print("BU03 Device ID: 0x");
    Serial.println(dev_id, HEX);

    while (!dwt_checkidlerc()) {
        Serial.println("IDLE FAILED, retrying...");
        delay(10);
    }
    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
        Serial.println("INIT FAILED"); while(1);
    }
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);
    if (dwt_configure(&config)) {
        Serial.println("CONFIG FAILED"); while(1);
    }
    dwt_configuretxrf(&txconfig_options);
    dwt_setrxantennadelay(RX_ANT_DLY);
    dwt_settxantennadelay(TX_ANT_DLY);
    dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
    dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);
    dwt_setlnapamode(DWT_LNA_ENABLE);   // PA removed, LNA only

    Serial.println("=====================================");
    Serial.println("  ANCHOR 1 READY (UDP CLIENT)");
    Serial.println("=====================================");
}

void loop() {
    tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
    dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_TXFRS_BIT_MASK);
    dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0);
    dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);
    dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED);

    while (!((status_reg = dwt_read32bitreg(SYS_STATUS_ID)) &
             (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR)));

    frame_seq_nb++;

    if (status_reg & SYS_STATUS_RXFCG_BIT_MASK) {
        uint32_t frame_len;
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);

        frame_len = dwt_read32bitreg(RX_FINFO_ID) & RXFLEN_MASK;
        if (frame_len <= sizeof(rx_buffer)) {
            dwt_readrxdata(rx_buffer, frame_len, 0);
            rx_buffer[ALL_MSG_SN_IDX] = 0;

            if (memcmp(rx_buffer, rx_resp_msg, ALL_MSG_COMMON_LEN) == 0) {
                uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
                int32_t rtd_init, rtd_resp;
                float clockOffsetRatio;

                poll_tx_ts = dwt_readtxtimestamplo32();
                resp_rx_ts = dwt_readrxtimestamplo32();
                clockOffsetRatio = ((float)dwt_readclockoffset()) / (uint32_t)(1 << 26);

                resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
                resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

                rtd_init = resp_rx_ts - poll_tx_ts;
                rtd_resp = resp_tx_ts - poll_rx_ts;
                double tof = ((rtd_init - rtd_resp * (1.0 - clockOffsetRatio)) / 2.0) * DWT_TIME_UNITS;
                distance = tof * SPEED_OF_LIGHT;

                double filtered = applyFilter(distance);

                if (filtered >= 0) {
                    // Send to STM32 Hub via UDP
                    if (WiFi.status() == WL_CONNECTED) {
                        udp.beginPacket(WiFi.gatewayIP(), udpPort);
                        char msg[32];
                        sprintf(msg, "ANCHOR1: %.2f", filtered);
                        udp.print(msg);
                        udp.endPacket();
                    }
                    
                    Serial.print("ANCHOR 1 DIST: ");
                    Serial.print(filtered, 2);
                    Serial.println(" m");
                }
            }
        }
    } else {
        dwt_write32bitreg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
    }

    // Add slight random jitter to prevent continuous collisions with Anchor 2 and Hub
    delay(RNG_DELAY_MS + random(0, 100));
}
