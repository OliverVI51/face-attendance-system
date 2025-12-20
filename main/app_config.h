#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// Wi-Fi Configuration
#define WIFI_SSID "Debian-phy_AP"
#define WIFI_PASSWORD "00000000"
#define WIFI_MAXIMUM_RETRY 10

// HTTP Configuration
#define HTTP_SERVER_URL "http://debian-phy.lan:8063/attendance"
#define HTTP_TIMEOUT_MS 5000
#define HTTP_RETRY_COUNT 3

// NTP Configuration
#define NTP_SERVER "debian-phy.lan"
#define NTP_SYNC_INTERVAL_SEC 3600
#define TIMEZONE "EET-2"  // Africa/Cairo (UTC+2)

// System Timing
#define OUT_OF_SERVICE_TIMEOUT_SEC 120
#define FINGERPRINT_TIMEOUT_SEC 10

// Admin Configuration
#define ADMIN_PIN "000000"
#define MAX_FINGERPRINTS 20

// GPIO Definitions - UART
#define UART0_TX_PIN 43
#define UART0_RX_PIN 44

#define UART1_TX_PIN 17  // Fingerprint
#define UART1_RX_PIN 18
#define FINGERPRINT_UART UART_NUM_1
#define FINGERPRINT_BAUD 57600

#define UART2_TX_PIN 41  // MP3
#define UART2_RX_PIN 42
#define MP3_UART UART_NUM_2
#define MP3_BAUD 9600

// GPIO Definitions - SPI Display
#define LCD_MOSI_PIN 11
#define LCD_SCLK_PIN 12
#define LCD_CS_PIN 10
#define LCD_DC_PIN 9
#define LCD_RST_PIN 46
#define LCD_BL_PIN 45
#define LCD_SPI_HOST SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_H_RES 320
#define LCD_V_RES 172

// GPIO Definitions - Keypad
#define KEYPAD_ROW1_PIN 1
#define KEYPAD_ROW2_PIN 2
#define KEYPAD_ROW3_PIN 21
#define KEYPAD_ROW4_PIN 4
#define KEYPAD_COL1_PIN 5
#define KEYPAD_COL2_PIN 6
#define KEYPAD_COL3_PIN 7
#define KEYPAD_COL4_PIN 8
#define KEYPAD_SCAN_INTERVAL_MS 25

// Audio Files
#define AUDIO_SUCCESS 1
#define AUDIO_FAILURE 2
#define AUDIO_BEEP 3
#define AUDIO_OUT_OF_SERVICE 4
#define BOOT_CHECK_REQUIRED_MP3_COUNT   4

#define BOOT_CHECK_TIMEOUT_MS           20000   // Timeout for hardware responses

// FreeRTOS Task Priorities
#define PRIORITY_UI_TASK 5
#define PRIORITY_FINGERPRINT_TASK 6
#define PRIORITY_KEYPAD_TASK 4
#define PRIORITY_AUDIO_TASK 3
#define PRIORITY_NETWORK_TASK 7
#define PRIORITY_TIME_SYNC_TASK 2

// FreeRTOS Stack Sizes
#define STACK_SIZE_UI_TASK 16384
#define STACK_SIZE_FINGERPRINT_TASK 6144
#define STACK_SIZE_KEYPAD_TASK 4096
#define STACK_SIZE_AUDIO_TASK 4096
#define STACK_SIZE_NETWORK_TASK 8192
#define STACK_SIZE_TIME_SYNC_TASK 4096

#endif // APP_CONFIG_H