/* Author: Aung Naing Oo 
 * ESP32-S3 Portrait Pong
 * Hardware: Waveshare ESP32-S3-Touch-LCD-4.3
 * Orientation: Portrait (480x800)
 */

#include <Arduino.h>
#include <esp_display_panel.hpp> 
#include "lvgl_v8_port.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// ================= CONFIGURATION =================
#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 800

#define PADDLE_WIDTH  100
#define PADDLE_HEIGHT 20
#define BALL_SIZE     20

#define BALL_SPEED    12 // Higher is faster
#define AI_SPEED      7  // Lower is easier, Higher is harder

// ================= GLOBALS =================
Board *board = nullptr;

// UI Objects
lv_obj_t * scr;
lv_obj_t * paddle_player;
lv_obj_t * paddle_ai;
lv_obj_t * ball_obj;
lv_obj_t * label_score;
lv_obj_t * touch_zone; // Invisible layer to catch touches

// Game State
int ball_x = SCREEN_WIDTH / 2;
int ball_y = SCREEN_HEIGHT / 2;
int ball_dx = BALL_SPEED;
int ball_dy = BALL_SPEED;

int player_x = (SCREEN_WIDTH - PADDLE_WIDTH) / 2;
int ai_x = (SCREEN_WIDTH - PADDLE_WIDTH) / 2;

int score_player = 0;
int score_ai = 0;

// ================= GAME LOGIC =================

void update_score() {
    lv_label_set_text_fmt(label_score, "%d  :  %d", score_ai, score_player);
}

void reset_ball() {
    ball_x = SCREEN_WIDTH / 2;
    ball_y = SCREEN_HEIGHT / 2;
    
    // Randomize start direction
    ball_dx = (random(0, 2) == 0) ? BALL_SPEED : -BALL_SPEED;
    ball_dy = (random(0, 2) == 0) ? BALL_SPEED : -BALL_SPEED;
    
    update_score();
}

// The Physics Loop (Runs every 33ms ~ 30FPS)
void game_timer_cb(lv_timer_t * timer) {
    
    // 1. Move Ball
    ball_x += ball_dx;
    ball_y += ball_dy;

    // 2. Wall Collisions (Left/Right)
    if (ball_x <= 0 || ball_x >= (SCREEN_WIDTH - BALL_SIZE)) {
        ball_dx = -ball_dx;
        // Clamp to edges to prevent sticking
        if (ball_x <= 0) ball_x = 0;
        if (ball_x >= (SCREEN_WIDTH - BALL_SIZE)) ball_x = SCREEN_WIDTH - BALL_SIZE;
    }

    // 3. Paddle Collisions
    
    // Player Paddle (Bottom)
    // Y position is fixed at 750
    if (ball_dy > 0 && ball_y >= (750 - BALL_SIZE) && ball_y < 760) {
        // Check X overlap
        if ((ball_x + BALL_SIZE) >= player_x && ball_x <= (player_x + PADDLE_WIDTH)) {
            ball_dy = -ball_dy;
            
            // Add some "English" (spin) based on where it hit the paddle
            int hit_center = ball_x - (player_x + PADDLE_WIDTH / 2);
            ball_dx = hit_center / 3; 
        }
    }

    // AI Paddle (Top)
    // Y position is fixed at 50
    if (ball_dy < 0 && ball_y <= (50 + PADDLE_HEIGHT) && ball_y > 40) {
        if ((ball_x + BALL_SIZE) >= ai_x && ball_x <= (ai_x + PADDLE_WIDTH)) {
            ball_dy = -ball_dy;
        }
    }

    // 4. Scoring (Top/Bottom Walls)
    if (ball_y < 0) {
        score_player++;
        reset_ball();
    }
    if (ball_y > SCREEN_HEIGHT) {
        score_ai++;
        reset_ball();
    }

    // 5. AI Movement (Simple Tracking)
    int target_x = ball_x - (PADDLE_WIDTH / 2);
    if (ai_x < target_x) ai_x += AI_SPEED;
    if (ai_x > target_x) ai_x -= AI_SPEED;
    
    // Clamp AI
    if (ai_x < 0) ai_x = 0;
    if (ai_x > (SCREEN_WIDTH - PADDLE_WIDTH)) ai_x = SCREEN_WIDTH - PADDLE_WIDTH;

    // 6. Update Visuals
    lv_obj_set_pos(ball_obj, ball_x, ball_y);
    lv_obj_set_pos(paddle_ai, ai_x, 50);
    // Player paddle is updated in input_cb, but we ensure it stays within bounds here?
    // Actually, let's update visual here to be safe if we change logic later
    // lv_obj_set_pos(paddle_player, player_x, 750); 
}

// Touch Input Handler
void input_cb(lv_event_t * e) {
    lv_indev_t * indev = lv_indev_get_act();
    if (!indev) return;

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    // Center paddle on finger X
    player_x = p.x - (PADDLE_WIDTH / 2);

    // Clamp to screen
    if (player_x < 0) player_x = 0;
    if (player_x > (SCREEN_WIDTH - PADDLE_WIDTH)) player_x = SCREEN_WIDTH - PADDLE_WIDTH;

    lv_obj_set_pos(paddle_player, player_x, 750);
}

// ================= SETUP =================
void setup() {
    Serial.begin(115200);

    // 1. Init Board (Same stable config)
    board = new Board();
    board->init();
    board->begin();

    // 2. Init LVGL
    lvgl_port_init(board->getLCD(), board->getTouch());
    
    // Ensure Portrait Mode
    lv_disp_set_rotation(NULL, LV_DISP_ROT_90);

    // 3. Create Game UI
    lvgl_port_lock(-1);
    
    scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); // Black BG
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Center Line
    lv_obj_t * line = lv_obj_create(scr);
    lv_obj_set_size(line, SCREEN_WIDTH - 40, 2);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(line, 0, 0);

    // Score Label (Center)
    label_score = lv_label_create(scr);
    lv_obj_set_style_text_font(label_score, &lv_font_montserrat_30, 0); // Big Font
    lv_obj_set_style_text_color(label_score, lv_color_hex(0x444444), 0); // Dim Grey
    lv_obj_align(label_score, LV_ALIGN_CENTER, 0, 0);
    update_score();

    // AI Paddle (Top / Red)
    paddle_ai = lv_obj_create(scr);
    lv_obj_set_size(paddle_ai, PADDLE_WIDTH, PADDLE_HEIGHT);
    lv_obj_set_pos(paddle_ai, ai_x, 50);
    lv_obj_set_style_bg_color(paddle_ai, lv_color_hex(0xFF0055), 0); // Neon Red
    lv_obj_set_style_border_width(paddle_ai, 0, 0);

    // Player Paddle (Bottom / Green)
    paddle_player = lv_obj_create(scr);
    lv_obj_set_size(paddle_player, PADDLE_WIDTH, PADDLE_HEIGHT);
    lv_obj_set_pos(paddle_player, player_x, 750);
    lv_obj_set_style_bg_color(paddle_player, lv_color_hex(0x00FF55), 0); // Neon Green
    lv_obj_set_style_border_width(paddle_player, 0, 0);

    // Ball (White Square)
    ball_obj = lv_obj_create(scr);
    lv_obj_set_size(ball_obj, BALL_SIZE, BALL_SIZE);
    lv_obj_set_pos(ball_obj, ball_x, ball_y);
    lv_obj_set_style_bg_color(ball_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(ball_obj, 10, 0); // Make it a circle
    lv_obj_set_style_border_width(ball_obj, 0, 0);

    // Touch Zone (Invisible, Full Screen)
    // This captures drags anywhere on screen to move the paddle
    touch_zone = lv_obj_create(scr);
    lv_obj_set_size(touch_zone, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_opa(touch_zone, LV_OPA_TRANSP, 0); // Invisible
    lv_obj_set_style_border_width(touch_zone, 0, 0);
    lv_obj_add_event_cb(touch_zone, input_cb, LV_EVENT_PRESSING, NULL);

    lvgl_port_unlock();

    // 4. Start Physics Timer (30ms = ~33 FPS)
    lv_timer_create(game_timer_cb, 30, NULL);
}

void loop() {
    delay(1000);
}