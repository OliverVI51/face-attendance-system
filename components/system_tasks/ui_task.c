#include "ui_task.h"
#include "display_driver.h"
#include "system_state.h"
#include "app_config.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> 

static const char *TAG = "UI_TASK";

extern display_handle_t g_display_handle;
volatile system_state_t g_current_state = STATE_IDLE; 

// --- Screen Drawing Functions ---

static void draw_idle_screen(display_handle_t display) {
    display_clear(display, COLOR_BLACK);
    display_draw_text_large(display, 40, 20, "ATTENDANCE", COLOR_WHITE, COLOR_BLACK);
    display_draw_text_large(display, 50, 50, "SYSTEM", COLOR_WHITE, COLOR_BLACK);
    display_draw_text(display, 30, 80, "Press A for FP OR", COLOR_CYAN, COLOR_BLACK);
    display_draw_text(display, 30, 110, "B for keypad", COLOR_CYAN, COLOR_BLACK);
    display_draw_text(display, 30, 140, "Press # for Admin", COLOR_DARKGRAY, COLOR_BLACK);
}

static void draw_scanning_screen(display_handle_t display) {
    display_clear(display, COLOR_BLACK);
    display_draw_text_large(display, 20, 50, "PLACE FINGER", COLOR_YELLOW, COLOR_BLACK);
    display_draw_text(display, 60, 100, "Scanning...", COLOR_WHITE, COLOR_BLACK);
}

static void draw_success_screen(display_handle_t display, uint16_t fp_id) {
    display_clear(display, COLOR_GREEN);
    display_draw_text_large(display, 50, 40, "SUCCESS!", COLOR_WHITE, COLOR_GREEN);
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "ID: %d", fp_id);
    display_draw_text_large(display, 80, 90, id_str, COLOR_WHITE, COLOR_GREEN);
}

static void draw_failure_screen(display_handle_t display) {
    display_clear(display, COLOR_RED);
    display_draw_text_large(display, 60, 50, "FAILED", COLOR_WHITE, COLOR_RED);
    display_draw_text(display, 40, 100, "Try again", COLOR_WHITE, COLOR_RED);
}

static void draw_admin_pin_screen(display_handle_t display, const char *pin_buffer) {
    display_clear(display, COLOR_BLUE);
    display_draw_text_large(display, 30, 30, "ADMIN MODE", COLOR_WHITE, COLOR_BLUE);
    display_draw_text(display, 40, 80, "Enter PIN & '#'", COLOR_WHITE, COLOR_BLUE);
    
    char display_pin[16];
    int len = strlen(pin_buffer);
    for (int i = 0; i < len; i++) display_pin[i] = '*';
    display_pin[len] = '\0';
    
    display_draw_text_large(display, 80, 110, display_pin, COLOR_YELLOW, COLOR_BLUE);
}

static void draw_register_screen(display_handle_t display, const char *id_buffer) {
    ESP_LOGI(TAG, "Drawing Register Screen..."); 
    display_clear(display, COLOR_BLUE);
    display_draw_text_large(display, 10, 30, "NEW USER", COLOR_WHITE, COLOR_BLUE);
    display_draw_text(display, 20, 70, "Enter ID (1-20):", COLOR_WHITE, COLOR_BLUE);
    display_draw_text_large(display, 100, 110, id_buffer, COLOR_YELLOW, COLOR_BLUE);
    display_draw_text(display, 40, 140, "Press '#' to Save", COLOR_WHITE, COLOR_BLUE);
    ESP_LOGI(TAG, "Drawing Complete");
}

static void draw_enroll_step1(display_handle_t display) {
    display_clear(display, COLOR_BLACK);
    display_draw_text_large(display, 20, 50, "STEP 1/2", COLOR_CYAN, COLOR_BLACK);
    display_draw_text(display, 40, 100, "Place Finger...", COLOR_WHITE, COLOR_BLACK);
}

static void draw_enroll_step2(display_handle_t display) {
    display_clear(display, COLOR_BLACK);
    display_draw_text_large(display, 20, 50, "STEP 2/2", COLOR_CYAN, COLOR_BLACK);
    display_draw_text(display, 40, 100, "Place Again...", COLOR_WHITE, COLOR_BLACK);
}

static void draw_out_of_service_screen(display_handle_t display) {
    display_clear(display, COLOR_DARKGRAY);
    display_draw_text_large(display, 20, 50, "OUT OF", COLOR_RED, COLOR_DARKGRAY);
    display_draw_text_large(display, 30, 90, "SERVICE", COLOR_RED, COLOR_DARKGRAY);
}

// --- Main Task ---

void ui_task(void *pvParameters) {
    ESP_LOGI(TAG, "UI task started");
    
    system_message_t msg;
    char input_buffer[16] = {0};
    
    // Draw initial screen
    draw_idle_screen(g_display_handle);
    
    while (1) {
        if (xQueueReceive(g_ui_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            // FRAME SKIPPING CHECK
            // If there are more messages waiting (user typing fast),
            // skip the expensive drawing functions for intermediate keys.
            bool skip_draw = (uxQueueMessagesWaiting(g_ui_queue) > 0);

            switch (msg.type) {
                // --- HANDLE KEYPAD INPUT ---
                case MSG_KEYPAD_KEY_PRESSED:
                    char key = msg.data.keypad.key;
                    
                    // 1. IDLE STATE: Press '#' to enter Admin Mode
                    if (g_current_state == STATE_IDLE) {
                        if (key == '#') {
                            ESP_LOGI(TAG, "Entering Admin PIN Mode");
                            g_current_state = STATE_ADMIN_PIN_ENTRY;
                            memset(input_buffer, 0, sizeof(input_buffer));
                            draw_admin_pin_screen(g_display_handle, input_buffer);
                        }
                    }
                    
                    // 2. PIN ENTRY STATE
                    else if (g_current_state == STATE_ADMIN_PIN_ENTRY) {
                        size_t len = strlen(input_buffer);
                        
                        if (key >= '0' && key <= '9') {
                            if (len < 6) {
                                input_buffer[len] = key;
                                input_buffer[len+1] = '\0';
                                // OPTIMIZATION: Only draw if this is the last key in the buffer
                                if (!skip_draw) {
                                    draw_admin_pin_screen(g_display_handle, input_buffer);
                                }
                            }
                        } 
                        else if (key == '*') { // Cancel / Back
                            g_current_state = STATE_IDLE;
                            draw_idle_screen(g_display_handle);
                        }
                        else if (key == '#') { // Enter to Verify
                            ESP_LOGI(TAG, "Verifying PIN: %s", input_buffer);
                            if (strcmp(input_buffer, ADMIN_PIN) == 0) {
                                ESP_LOGI(TAG, "PIN Correct");
                                g_current_state = STATE_ADMIN_FINGERPRINT_REGISTER;
                                memset(input_buffer, 0, sizeof(input_buffer));
                                draw_register_screen(g_display_handle, input_buffer);
                            } else {
                                ESP_LOGW(TAG, "PIN Incorrect");
                                draw_failure_screen(g_display_handle);
                                vTaskDelay(pdMS_TO_TICKS(1000));
                                g_current_state = STATE_IDLE;
                                draw_idle_screen(g_display_handle);
                            }
                        }
                    }
                    
                    // 3. ENTER ID STATE (Register)
                    else if (g_current_state == STATE_ADMIN_FINGERPRINT_REGISTER) {
                         size_t len = strlen(input_buffer);
                         
                         if (key >= '0' && key <= '9') {
                             if (len < 3) {
                                 input_buffer[len] = key;
                                 input_buffer[len+1] = '\0';
                                 // OPTIMIZATION: Only draw if queue is empty
                                 if (!skip_draw) {
                                    draw_register_screen(g_display_handle, input_buffer);
                                 }
                             }
                         }
                         else if (key == '#') { // Enter ID to Start Enrollment
                             int id = atoi(input_buffer);
                             if (id > 0 && id <= 200) {
                                 ESP_LOGI(TAG, "Starting Enroll for ID: %d", id);
                                 system_message_t enroll_msg = {
                                     .type = MSG_START_ENROLL,
                                     .data.enroll.enroll_id = (uint16_t)id
                                 };
                                 xQueueSend(g_fingerprint_queue, &enroll_msg, 0);
                             } else {
                                 draw_failure_screen(g_display_handle);
                                 vTaskDelay(pdMS_TO_TICKS(1000));
                                 memset(input_buffer, 0, sizeof(input_buffer));
                                 draw_register_screen(g_display_handle, input_buffer);
                             }
                         }
                         else if (key == '*') { // Cancel
                             g_current_state = STATE_IDLE;
                             draw_idle_screen(g_display_handle);
                         }
                    }
                    break;

                // --- HANDLE DISPLAY UPDATES ---
                case MSG_DISPLAY_UPDATE:
                     if (g_current_state == STATE_IDLE) draw_idle_screen(g_display_handle);
                     else if (g_current_state == STATE_FINGERPRINT_SCAN) draw_scanning_screen(g_display_handle);
                     else if (g_current_state == STATE_SUCCESS) draw_success_screen(g_display_handle, msg.data.fingerprint.fingerprint_id);
                     else if (g_current_state == STATE_FAILURE) draw_failure_screen(g_display_handle);
                     else if (g_current_state == STATE_OUT_OF_SERVICE) draw_out_of_service_screen(g_display_handle);
                     break;

                case MSG_ENROLL_STEP_1:
                    draw_enroll_step1(g_display_handle);
                    break;
                case MSG_ENROLL_STEP_2:
                    draw_enroll_step2(g_display_handle);
                    break;
                case MSG_ENROLL_SUCCESS:
                    draw_success_screen(g_display_handle, msg.data.enroll.enroll_id);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    g_current_state = STATE_IDLE;
                    draw_idle_screen(g_display_handle);
                    break;
                case MSG_ENROLL_FAIL:
                    draw_failure_screen(g_display_handle);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    g_current_state = STATE_IDLE;
                    draw_idle_screen(g_display_handle);
                    break;

                default:
                    break;
            }
        }
        
        EventBits_t bits = xEventGroupGetBits(g_system_events);
        if (bits & EVENT_OUT_OF_SERVICE) {
            if (g_current_state != STATE_OUT_OF_SERVICE) {
                g_current_state = STATE_OUT_OF_SERVICE;
                draw_out_of_service_screen(g_display_handle);
            }
        }
    }
}