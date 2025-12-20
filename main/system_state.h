#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

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
    STATE_OUT_OF_SERVICE
} system_state_t;

// Message Types for Queues
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
    
    // Admin / Enrollment Flow (NEW)
    MSG_START_ENROLL,       // Tell FP task to start registering
    MSG_ENROLL_STEP_1,      // UI: Place finger 1st time
    MSG_ENROLL_STEP_2,      // UI: Place finger 2nd time
    MSG_ENROLL_SUCCESS,     // Registration success
    MSG_ENROLL_FAIL         // Registration failed
} message_type_t;

// Message Structures
typedef struct {
    message_type_t type;
    union {
        struct {
            uint16_t fingerprint_id;
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

        // New structure for enrollment data
        struct {
            uint16_t enroll_id;
        } enroll;

    } data;
} system_message_t;

// Global Queue Handles (declared in main.c)
extern QueueHandle_t g_ui_queue;
extern QueueHandle_t g_fingerprint_queue;
extern QueueHandle_t g_keypad_queue;
extern QueueHandle_t g_audio_queue;
extern QueueHandle_t g_network_queue;

// Global Event Group (declared in main.c)
extern EventGroupHandle_t g_system_events;

// Current System State (declared in main.c)
extern volatile system_state_t g_current_state;

#endif // SYSTEM_STATE_H