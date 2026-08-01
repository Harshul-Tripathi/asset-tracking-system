/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "port.h"
#include "deca_device_api.h"
#include "dw3000_deca_regs.h"
#include "stm32u5xx_nucleo.h"
#include "w6x_api.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include <stdarg.h>

void vLoggingPrintf(uint32_t log_level,
                    const uint8_t metadata_print,
                    const uint32_t line_number,
                    const char *const p_file_name,
                    const char *const p_format,
                    ...)
{
    va_list args;
    va_start(args, p_format);
    vprintf(p_format, args);
    va_end(args);
}
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
int32_t udp_socket = -1;       /* Receive socket (bound to port 5000, for anchor data) */
int32_t udp_send_socket = -1;  /* Send socket (ALLOCATED state, for broadcasting to PC) */
float anchor1_dist = -1.0f;
float anchor2_dist = -1.0f;

#define POLL_TX_TO_RESP_RX_DLY_UUS 140
#define RESP_RX_TIMEOUT_UUS 1000
#define ALL_MSG_SN_IDX 2
#define RESP_MSG_POLL_RX_TS_IDX 10
#define RESP_MSG_RESP_TX_TS_IDX 14
#define SPEED_OF_LIGHT 299792458.0

static dwt_config_t config = {
    5,               /* Channel number. */
    DWT_PLEN_128,    /* Preamble length. Used in TX only. */
    DWT_PAC8,        /* Preamble acquisition chunk size. Used in RX only. */
    9,               /* TX preamble code. Used in TX only. */
    9,               /* RX preamble code. Used in RX only. */
    1,               /* 0 to use standard 8 symbol SFD, 1 to use non-standard 8 symbol, 2 for non-standard 16 symbol SFD and 3 for 4z 8 symbol SDF type */
    DWT_BR_6M8,      /* Data rate. */
    DWT_PHRMODE_STD, /* PHY header mode. */
    DWT_PHRRATE_STD, /* PHY header rate. */
    (129 + 8 - 8),   /* SFD timeout (preamble length + 1 + SFD length - PAC size). Used in RX only. */
    DWT_STS_MODE_OFF,
    DWT_STS_LEN_64,  /* STS length, see allowed values in Enum dwt_sts_lengths_e */
    DWT_PDOA_M0      /* PDOA mode off */
};
static dwt_txconfig_t txconfig_options = {
    0x34,           /* PG delay. */
    0xfdfdfdfd,      /* TX power. */
    0x0             /* PG count. */
};

static uint8_t tx_poll_msg[] = {0x41, 0x88, 0, 0xCA, 0xDE, 'W', 'A', 'V', 'E', 0xE0, 0, 0};
static uint8_t rx_buffer[20];
static uint8_t frame_seq_nb = 0;

osThreadId_t hubTaskHandle;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void HubTask_Entry(void *argument);
static uint64_t get_tx_timestamp_u64(void);
static uint64_t get_rx_timestamp_u64(void);
static void resp_msg_get_ts(uint8_t *ts_field, uint32_t *ts);
/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  const osThreadAttr_t hubTask_attributes = {
    .name = "hubTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 2048 * 4
  };
  hubTaskHandle = osThreadNew(HubTask_Entry, NULL, &hubTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}
/* USER CODE BEGIN Header_StartDefaultTask */
/**
* @brief Function implementing the defaultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN defaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static uint64_t get_tx_timestamp_u64(void)
{
    uint8_t ts_tab[5];
    uint64_t ts = 0;
    dwt_readtxtimestamp(ts_tab);
    for (int i = 4; i >= 0; i--) {
        ts <<= 8;
        ts |= ts_tab[i];
    }
    return ts;
}

static uint64_t get_rx_timestamp_u64(void)
{
    uint8_t ts_tab[5];
    uint64_t ts = 0;
    dwt_readrxtimestamp(ts_tab, 0);
    for (int i = 4; i >= 0; i--) {
        ts <<= 8;
        ts |= ts_tab[i];
    }
    return ts;
}

static void resp_msg_get_ts(uint8_t *ts_field, uint32_t *ts)
{
    *ts = 0;
    for (int i = 0; i < 4; i++) {
        *ts |= (uint32_t)ts_field[i] << (i * 8);
    }
}

static void APP_wifi_cb(W6X_event_id_t event_id, void *event_args) {}
static void APP_net_cb(W6X_event_id_t event_id, void *event_args) {}
static void APP_mqtt_cb(W6X_event_id_t event_id, void *event_args) {}
static void APP_ble_cb(W6X_event_id_t event_id, void *event_args) {}
static void APP_error_cb(W6X_Status_t ret_w6x, char const *func_name) {}

void HubTask_Entry(void *argument)
{
    extern SPI_HandleTypeDef hspi2;
    extern SPI_HandleTypeDef *hcurrent_active_spi;
    extern uint16_t          pin_io_active_spi;
    
    hcurrent_active_spi = &hspi2;
    pin_io_active_spi = UWB_CS_Pin;

    printf("\r\n--- HubTask Entered ---\r\n");

    reset_DWIC();      // Toggle RST pin

    uint8_t tx_cmd[1] = {0x00}; 
    uint8_t rx_buf[4] = {0, 0, 0, 0};
    HAL_GPIO_WritePin(UWB_CS_GPIO_Port, UWB_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi2, tx_cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi2, rx_buf, 4, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(UWB_CS_GPIO_Port, UWB_CS_Pin, GPIO_PIN_SET);
    uint32_t dev_id = (rx_buf[3] << 24) | (rx_buf[2] << 16) | (rx_buf[1] << 8) | rx_buf[0];
    printf("BU03 Device ID test: 0x%08lX\r\n", dev_id);

    extern const struct dwt_probe_s dw3000_probe_interf;
    if (dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf) != DWT_SUCCESS) {
        printf("dwt_probe failed!\r\n");
        while (1) { osDelay(100); }
    }

    while (!dwt_checkidlerc()) {
        printf("IDLE FAILED, retrying...\r\n");
        osDelay(10);
    }
    
    if (dwt_initialise(DWT_DW_INIT) == DWT_ERROR) {
        printf("INIT FAILED\r\n");
        while (1) { osDelay(100); }
    }
    
    dwt_setleds(DWT_LEDS_ENABLE | DWT_LEDS_INIT_BLINK);
    
    if (dwt_configure(&config)) {
        printf("CONFIG FAILED\r\n");
        while (1) { osDelay(100); }
    }
    
    dwt_configuretxrf(&txconfig_options);
    dwt_setrxantennadelay(16385);
    dwt_settxantennadelay(16385);
    dwt_setlnapamode(DWT_LNA_ENABLE);

    dwt_setrxaftertxdelay(POLL_TX_TO_RESP_RX_DLY_UUS);
    dwt_setrxtimeout(RESP_RX_TIMEOUT_UUS);

    W6X_App_Cb_t App_cb = {0};
    App_cb.APP_wifi_cb = APP_wifi_cb;
    App_cb.APP_net_cb = APP_net_cb;
    App_cb.APP_ble_cb = APP_ble_cb;
    App_cb.APP_mqtt_cb = APP_mqtt_cb;
    App_cb.APP_error_cb = APP_error_cb;
    W6X_RegisterAppCb(&App_cb);

    printf("Initializing ST67W6X Wi-Fi...\r\n");
    W6X_Init();
    W6X_WiFi_Init();
    W6X_Net_Init();
    
    W6X_WiFi_ApConfig_t ap_config;
    memset(&ap_config, 0, sizeof(ap_config));
    strcpy((char*)ap_config.SSID, "UWB_HUB");
    strcpy((char*)ap_config.Password, "12345678");
    ap_config.Security = W6X_WIFI_AP_SECURITY_WPA2_PSK;
    ap_config.Channel = 6;
    ap_config.MaxConnections = 4;
    ap_config.Hidden = 0;
    ap_config.Protocol = W6X_WIFI_PROTOCOL_11N;

    if (W6X_WiFi_AP_Start(&ap_config) == 0) {
        printf("Soft-AP Started! SSID: %s\r\n", ap_config.SSID);
    } else {
        printf("Soft-AP Start Failed!\r\n");
    }

    uint8_t ip[4], mask[4];
    if (W6X_Net_AP_GetIPAddress(ip, mask) == 0) {
        printf("Hub IP: %d.%d.%d.%d\r\n", ip[0], ip[1], ip[2], ip[3]);
    }

    /* --- Receive socket: bound to port 5000 for incoming anchor data --- */
    udp_socket = W6X_Net_Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_socket >= 0) {
        struct sockaddr_in bind_addr;
        memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family = AF_INET;
        bind_addr.sin_port = PP_HTONS(5000);
        bind_addr.sin_addr.s_addr = 0;
        if (W6X_Net_Bind(udp_socket, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) == 0) {
            printf("UDP RX socket bound to port 5000.\r\n");
            int timeout_ms = 0;
            W6X_Net_Setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
        } else {
            printf("UDP RX Bind Failed!\r\n");
        }
    } else {
        printf("UDP RX Socket creation failed!\r\n");
    }

    /* --- Send socket: stays ALLOCATED, used only for Sendto --- */
    udp_send_socket = W6X_Net_Socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_send_socket >= 0) {
        printf("UDP TX socket created (id=%ld).\r\n", (long)udp_send_socket);
    } else {
        printf("UDP TX Socket creation failed!\r\n");
    }

    printf("Anchor (STM32) Ready! Starting to range...\r\n");

    for(;;)
    {
        if (udp_socket >= 0) {
            char udp_buf[128];
            struct sockaddr_in src_addr;
            socklen_t addr_len = sizeof(src_addr);
            ssize_t bytes;
            while ((bytes = W6X_Net_Recvfrom(udp_socket, udp_buf, sizeof(udp_buf)-1, 0, (struct sockaddr *)&src_addr, &addr_len)) > 0) {
                udp_buf[bytes] = '\0';
                if (strncmp(udp_buf, "ANCHOR1:", 8) == 0) {
                    anchor1_dist = atof(udp_buf + 8);
                } else if (strncmp(udp_buf, "ANCHOR2:", 8) == 0) {
                    anchor2_dist = atof(udp_buf + 8);
                }
            }
        }

        tx_poll_msg[ALL_MSG_SN_IDX] = frame_seq_nb;
        dwt_writetxdata(sizeof(tx_poll_msg), tx_poll_msg, 0);
        dwt_writetxfctrl(sizeof(tx_poll_msg), 0, 1);

        if (dwt_starttx(DWT_START_TX_IMMEDIATE | DWT_RESPONSE_EXPECTED) == DWT_SUCCESS) {
            uint32_t status_reg;
            while (!((status_reg = dwt_read_reg(SYS_STATUS_ID)) &
                     (SYS_STATUS_RXFCG_BIT_MASK | SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR))) { 
                osDelay(1); // Yield execution
            }

            float distance = -1.0f;

            if (status_reg & SYS_STATUS_RXFCG_BIT_MASK) {
                uint32_t frame_len = dwt_read_reg(RX_FINFO_ID) & RX_FINFO_RXFLEN_BIT_MASK;
                if (frame_len <= sizeof(rx_buffer)) {
                    dwt_readrxdata(rx_buffer, frame_len, 0);

                    if (rx_buffer[9] == 0xE1) {
                        uint32_t poll_tx_ts, resp_rx_ts, poll_rx_ts, resp_tx_ts;
                        int32_t rtd_init, rtd_resp;

                        poll_tx_ts = (uint32_t)get_tx_timestamp_u64();
                        resp_rx_ts = (uint32_t)get_rx_timestamp_u64();
                        resp_msg_get_ts(&rx_buffer[RESP_MSG_POLL_RX_TS_IDX], &poll_rx_ts);
                        resp_msg_get_ts(&rx_buffer[RESP_MSG_RESP_TX_TS_IDX], &resp_tx_ts);

                        rtd_init = resp_rx_ts - poll_tx_ts;
                        rtd_resp = resp_tx_ts - poll_rx_ts;

                        float tof = ((rtd_init - rtd_resp) / 2.0f) * DWT_TIME_UNITS;
                        distance = tof * SPEED_OF_LIGHT;
                    }
                }
                dwt_write_reg(SYS_STATUS_ID, SYS_STATUS_RXFCG_BIT_MASK);
            } else {
                dwt_write_reg(SYS_STATUS_ID, SYS_STATUS_ALL_RX_TO | SYS_STATUS_ALL_RX_ERR);
            }

            int a1_int = (int)anchor1_dist;
            int a1_frac = (int)((anchor1_dist - a1_int) * 100);
            if (a1_frac < 0) a1_frac = -a1_frac;
            
            int a2_int = (int)anchor2_dist;
            int a2_frac = (int)((anchor2_dist - a2_int) * 100);
            if (a2_frac < 0) a2_frac = -a2_frac;

            char out_buf[256];
            char a1_str[32], a2_str[32];

            if (anchor1_dist >= 0) snprintf(a1_str, sizeof(a1_str), "ANCHOR 1: %d.%02d m", a1_int, a1_frac);
            else snprintf(a1_str, sizeof(a1_str), "ANCHOR 1: N/A");

            if (anchor2_dist >= 0) snprintf(a2_str, sizeof(a2_str), "ANCHOR 2: %d.%02d m", a2_int, a2_frac);
            else snprintf(a2_str, sizeof(a2_str), "ANCHOR 2: N/A");

            int out_len;
            if (distance >= 0) {
                int d1_int = (int)distance;
                int d1_frac = (int)((distance - d1_int) * 100);
                if (d1_frac < 0) d1_frac = -d1_frac;
                out_len = snprintf(out_buf, sizeof(out_buf), "HUB distance TAG : %d.%02d m : %s : %s\r\n", d1_int, d1_frac, a1_str, a2_str);
            } else {
                out_len = snprintf(out_buf, sizeof(out_buf), "HUB distance TAG : N/A : %s : %s\r\n", a1_str, a2_str);
            }

            printf("%s", out_buf);
            
            if (udp_send_socket >= 0) {
                struct sockaddr_in dest_addr;
                memset(&dest_addr, 0, sizeof(dest_addr));
                dest_addr.sin_family = AF_INET;
                dest_addr.sin_port = PP_HTONS(5001);
                dest_addr.sin_addr.s_addr = PP_HTONL(0x0A1360FFU); /* 10.19.96.255 subnet broadcast */
                int s_ret = W6X_Net_Sendto(udp_send_socket, out_buf, out_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
                if (s_ret < 0) {
                    printf(">>> UDP TX failed! ret=%d <<<\r\n", s_ret);
                }
            }
        }
        frame_seq_nb++;
        osDelay(500);
    }
}
/* USER CODE END Application */

