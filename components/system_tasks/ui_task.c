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
extern QueueHandle_t g_network_queue;
extern QueueHandle_t g_audio_queue;

volatile system_state_t g_current_state = STATE_IDLE; 

// --- Screen Drawing Functions ---

static void draw_idle_screen(display_handle_t display) {
    display_clear(display, COLOR_BLACK);
    display_draw_text_large(display, 40, 20, "ATTENDANCE", COLOR_WHITE, COLOR_BLACK);
    display_draw_text_large(display, 50, 50, "SYSTEM", COLOR_WHITE, COLOR_BLACK);
    display_draw_text(display, 10, 80, "A:Scan  B:Manual", COLOR_CYAN, COLOR_BLACK);
    display_draw_text(display, 10, 110, "C:Remove #:Admin", COLOR_CYAN, COLOR_BLACK);
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
    display_clear(display, COLOR_BLUE);
    display_draw_text_large(display, 10, 30, "NEW USER", COLOR_WHITE, COLOR_BLUE);
    display_draw_text(display, 20, 70, "Enter ID (1-200):", COLOR_WHITE, COLOR_BLUE);
    display_draw_text_large(display, 100, 110, id_buffer, COLOR_YELLOW, COLOR_BLUE);
    display_draw_text(display, 40, 140, "Press '#' to Save", COLOR_WHITE, COLOR_BLUE);
}

static void draw_remove_user_screen(display_handle_t display, const char *id_buffer) {
    display_clear(display, COLOR_RED);
    display_draw_text_large(display, 10, 30, "DELETE USER", COLOR_WHITE, COLOR_RED);
    display_draw_text(display, 20, 70, "Enter ID to Del:", COLOR_WHITE, COLOR_RED);
    display_draw_text_large(display, 100, 110, id_buffer, COLOR_YELLOW, COLOR_RED);
    display_draw_text(display, 40, 140, "#=Delete  *=Exit", COLOR_WHITE, COLOR_RED);
}

static void draw_manual_attendance_screen(display_handle_t display, const char *id_buffer) {
    display_clear(display, COLOR_BLUE);
    display_draw_text_large(display, 10, 30, "MANUAL ENTRY", COLOR_WHITE, COLOR_BLUE);
    display_draw_text(display, 20, 70, "Enter User ID:", COLOR_WHITE, COLOR_BLUE);
    if (strlen(id_buffer) > 0) {
        display_draw_text_large(display, 100, 110, id_buffer, COLOR_YELLOW, COLOR_BLUE);
    } else {
        display_draw_text(display, 100, 110, "_", COLOR_GRAY, COLOR_BLUE);
    }
    display_draw_text(display, 40, 140, "#=Log  *=Exit", COLOR_WHITE, COLOR_BLUE);
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
    
    draw_idle_screen(g_display_handle);
    
    while (1) {
        if (xQueueReceive(g_ui_queue, &msg, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            bool skip_draw = (uxQueueMessagesWaiting(g_ui_queue) > 0);

            switch (msg.type) {
                case MSG_KEYPAD_KEY_PRESSED:
                    char key = msg.data.keypad.key;
                    
                    // 1. IDLE STATE
                    if (g_current_state == STATE_IDLE) {
                        if (key == '#') {
                            g_current_state = STATE_ADMIN_PIN_ENTRY;
                            memset(input_buffer, 0, sizeof(input_buffer));
                            draw_admin_pin_screen(g_display_handle, input_buffer);
                        }
                        else if (key == 'C') { // Remove User
                            g_current_state = STATE_REMOVE_USER;
                            memset(input_buffer, 0, sizeof(input_buffer));
                            draw_remove_user_screen(g_display_handle, input_buffer);
                        }
                        else if (key == 'B') { // Manual Attendance
                            g_current_state = STATE_MANUAL_ATTENDANCE;
                            memset(input_buffer, 0, sizeof(input_buffer));
                            draw_manual_attendance_screen(g_display_handle, input_buffer);
                        }
                    }
                    
                    // 2. ADMIN PIN STATE
                    else if (g_current_state == STATE_ADMIN_PIN_ENTRY) {
                        size_t len = strlen(input_buffer);
                        if (key >= '0' && key <= '9' && len < 6) {
                            input_buffer[len] = key; input_buffer[len+1] = '\0';
                            if (!skip_draw) draw_admin_pin_screen(g_display_handle, input_buffer);
                        } 
                        else if (key == '*') {
                            g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                        }
                        else if (key == '#') {
                            if (strcmp(input_buffer, ADMIN_PIN) == 0) {
                                g_current_state = STATE_ADMIN_FINGERPRINT_REGISTER;
                                memset(input_buffer, 0, sizeof(input_buffer));
                                draw_register_screen(g_display_handle, input_buffer);
                            } else {
                                draw_failure_screen(g_display_handle);
                                vTaskDelay(pdMS_TO_TICKS(1000));
                                g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                            }
                        }
                    }
                    
                    // 3. REGISTER STATE
                    else if (g_current_state == STATE_ADMIN_FINGERPRINT_REGISTER) {
                         size_t len = strlen(input_buffer);
                         if (key >= '0' && key <= '9' && len < 3) {
                             input_buffer[len] = key; input_buffer[len+1] = '\0';
                             if (!skip_draw) draw_register_screen(g_display_handle, input_buffer);
                         }
                         else if (key == '#') {
                             int id = atoi(input_buffer);
                             if (id > 0 && id <= 200) {
                                 system_message_t enroll_msg = { .type = MSG_START_ENROLL, .data.enroll.enroll_id = (uint16_t)id };
                                 xQueueSend(g_fingerprint_queue, &enroll_msg, 0);
                             } else {
                                 draw_failure_screen(g_display_handle);
                                 vTaskDelay(pdMS_TO_TICKS(1000));
                                 memset(input_buffer, 0, sizeof(input_buffer));
                                 draw_register_screen(g_display_handle, input_buffer);
                             }
                         }
                         else if (key == '*') {
                             g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                         }
                    }

                    // 4. REMOVE USER STATE
                    else if (g_current_state == STATE_REMOVE_USER) {
                        size_t len = strlen(input_buffer);
                        if (key >= '0' && key <= '9' && len < 3) {
                            input_buffer[len] = key; input_buffer[len+1] = '\0';
                            if (!skip_draw) draw_remove_user_screen(g_display_handle, input_buffer);
                        }
                        else if (key == '*') {
                            g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                        }
                        else if (key == '#') {
                            int id = atoi(input_buffer);
                            if (id > 0) {
                                display_draw_text(g_display_handle, 20, 140, "Deleting...", COLOR_WHITE, COLOR_RED);
                                system_message_t del_msg = { .type = MSG_REQ_DELETE_USER, .data.fingerprint.fingerprint_id = (uint16_t)id };
                                xQueueSend(g_fingerprint_queue, &del_msg, 0);
                            }
                        }
                    }

                    // 5. MANUAL ATTENDANCE STATE
                    else if (g_current_state == STATE_MANUAL_ATTENDANCE) {
                        size_t len = strlen(input_buffer);
                        if (key >= '0' && key <= '9' && len < 5) {
                            input_buffer[len] = key; input_buffer[len+1] = '\0';
                            if (!skip_draw) draw_manual_attendance_screen(g_display_handle, input_buffer);
                        }
                        else if (key == '*') {
                            g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                        }
                        else if (key == '#') {
                            int id = atoi(input_buffer);
                            if (id > 0) {
                                g_current_state = STATE_SUCCESS;
                                draw_success_screen(g_display_handle, (uint16_t)id);
                                system_message_t success_msg = { 
                                    .type = MSG_FINGERPRINT_MATCHED, 
                                    .data.fingerprint.fingerprint_id = (uint16_t)id, 
                                    .data.fingerprint.success = true 
                                };
                                xQueueSend(g_network_queue, &success_msg, 0);
                                xQueueSend(g_audio_queue, &success_msg, 0);
                                vTaskDelay(pdMS_TO_TICKS(2000));
                                g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                            } else {
                                draw_failure_screen(g_display_handle);
                                vTaskDelay(pdMS_TO_TICKS(1000));
                                g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                            }
                        }
                    }
                    break;

                // --- DISPLAY MESSAGES ---
                case MSG_DISPLAY_UPDATE:
                     if (g_current_state == STATE_IDLE) draw_idle_screen(g_display_handle);
                     else if (g_current_state == STATE_FINGERPRINT_SCAN) draw_scanning_screen(g_display_handle);
                     else if (g_current_state == STATE_SUCCESS) draw_success_screen(g_display_handle, msg.data.fingerprint.fingerprint_id);
                     else if (g_current_state == STATE_FAILURE) draw_failure_screen(g_display_handle);
                     break;

                // --- FEEDBACK MESSAGES ---
                case MSG_ENROLL_STEP_1: draw_enroll_step1(g_display_handle); break;
                case MSG_ENROLL_STEP_2: draw_enroll_step2(g_display_handle); break;
                case MSG_ENROLL_SUCCESS:
                    draw_success_screen(g_display_handle, msg.data.enroll.enroll_id);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                    break;
                case MSG_ENROLL_FAIL:
                    draw_failure_screen(g_display_handle);
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                    break;
                case MSG_DELETE_RESULT:
                    if (msg.data.fingerprint.success) {
                        display_clear(g_display_handle, COLOR_GREEN);
                        display_draw_text_large(g_display_handle, 30, 60, "DELETED!", COLOR_WHITE, COLOR_GREEN);
                    } else {
                        display_clear(g_display_handle, COLOR_RED);
                        display_draw_text_large(g_display_handle, 30, 60, "ERR/EMPTY", COLOR_WHITE, COLOR_RED);
                    }
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    g_current_state = STATE_IDLE; draw_idle_screen(g_display_handle);
                    break;
                default: break;
            }
        }
        
        EventBits_t bits = xEventGroupGetBits(g_system_events);
        if (bits & EVENT_OUT_OF_SERVICE && g_current_state != STATE_OUT_OF_SERVICE) {
            g_current_state = STATE_OUT_OF_SERVICE;
            draw_out_of_service_screen(g_display_handle);
        }
    }
}