#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

// Event Group Bits
#define EVENT_WIFI_CONNECTED        (1 << 0)
#define EVENT_WIFI_DISCONNECTED     (1 << 1)
#define EVENT_NTP_SYNCED            (1 << 2)
#define EVENT_HTTP_AVAILABLE        (1 << 3)
#define EVENT_OUT_OF_SERVICE        (1 << 4)
#define EVENT_BUTTON_PRESSED        (1 << 5)
#define EVENT_ADMIN_MODE            (1 << 6)

// System State
typedef enum {
    STATE_IDLE,
    STATE_FINGERPRINT_SCAN,
    STATE_SUCCESS,
    STATE_FAILURE,
    STATE_ADMIN_PIN_ENTRY,
    STATE_ADMIN_FINGERPRINT_REGISTER,
    STATE_REMOVE_USER,
    STATE_MANUAL_ATTENDANCE,
    STATE_OUT_OF_SERVICE
} system_state_t;

// Login Method Enum (NEW)
typedef enum {
    LOGIN_METHOD_FINGERPRINT,
    LOGIN_METHOD_KEYPAD
} login_method_t;

// Message Types
typedef enum {
    MSG_FINGERPRINT_DETECTED,
    MSG_FINGERPRINT_MATCHED,
    MSG_FINGERPRINT_NOT_MATCHED,
    MSG_FINGERPRINT_TIMEOUT,
    MSG_FINGERPRINT_ERROR,
    
    // Keypad & Button
    MSG_KEYPAD_KEY_PRESSED,
    MSG_BUTTON_PRESSED,
    
    // UI Updates
    MSG_DISPLAY_UPDATE,
    MSG_PLAY_AUDIO,
    
    // Network & Time
    MSG_HTTP_POST,
    MSG_HTTP_SUCCESS,
    MSG_HTTP_FAILURE,
    MSG_WIFI_STATUS,
    MSG_NTP_STATUS,
    
    // Admin / Enrollment Flow
    MSG_START_ENROLL,
    MSG_ENROLL_STEP_1,
    MSG_ENROLL_STEP_2,
    MSG_ENROLL_SUCCESS,
    MSG_ENROLL_FAIL,

    // Remove User Flow
    MSG_REQ_DELETE_USER,
    MSG_DELETE_RESULT
} message_type_t;

// Message Structures
typedef struct {
    message_type_t type;
    union {
        struct {
            uint16_t fingerprint_id;
            bool success;
            uint16_t score;
            login_method_t method;  // <--- NEW FIELD
        } fingerprint;
        
        struct {
            char key;
        } keypad;
        
        struct {
            uint8_t track_number;
        } audio;
        
        struct {
            uint16_t fingerprint_id;
            char timestamp[32];
        } http;
        
        struct {
            bool connected;
        } wifi;
        
        struct {
            bool synced;
        } ntp;

        struct {
            uint16_t enroll_id;
        } enroll;

    } data;
} system_message_t;

extern QueueHandle_t g_ui_queue;
extern QueueHandle_t g_fingerprint_queue;
extern QueueHandle_t g_keypad_queue;
extern QueueHandle_t g_audio_queue;
extern QueueHandle_t g_network_queue;
extern EventGroupHandle_t g_system_events;
extern volatile system_state_t g_current_state;

#endif // SYSTEM_STATE_H