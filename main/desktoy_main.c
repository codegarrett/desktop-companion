/*
 * Desktoy - Expressive Anime Face on OLED Display
 * ESP32-C3 Super Mini + GME12864-17 (SSD1306 128x64 OLED)
 * 
 * Hybrid 2D/3D rendering with procedural anime-style face
 */

#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "ssd1306.h"
#include "render3d.h"
#include "buzzer.h"

static const char *TAG = "desktoy";

// ============================================================================
// CONFIGURATION
// ============================================================================

// Pin configuration
#define I2C_SDA_PIN     9
#define I2C_SCL_PIN     8
#define OLED_I2C_ADDR   0x3C
#define BUZZER_PIN      3

// Emotion types
typedef enum {
    EMO_NORMAL = 0,
    EMO_HAPPY,
    EMO_LAUGHING,
    EMO_ANGRY,
    EMO_SAD,
    EMO_SURPRISED,
    EMO_SLEEPY,
    EMO_SLEEPING,
    EMO_CRAZY,
    EMO_LOVE,
    EMO_WINK,
    EMO_SMUG,
    EMO_SCARED,
    EMO_BIRTHDAY,
    EMO_TROLLFACE,
    EMO_COUNT
} emotion_t;

// Face state
typedef struct {
    float left_eye_open;     // 0.0 = closed, 1.0 = open
    float right_eye_open;
    float target_left_eye;
    float target_right_eye;
    int look_x, look_y;
    int target_look_x, target_look_y;
    float left_brow_angle;   // -1.0 to 1.0 (down to up on inner side)
    float right_brow_angle;
    float left_brow_height;  // -5 to 5 pixels offset
    float right_brow_height;
    float mouth_open;        // 0.0 to 1.0
    float mouth_width;       // 0.5 to 1.5
    float mouth_curve;       // -1.0 (frown) to 1.0 (smile)
    emotion_t emotion;
    uint32_t next_blink;
    uint32_t next_look;
    uint32_t next_emotion;
    uint32_t anim_start;     // For special animations
    float bounce;            // For laughing/dancing
    float shake;             // For crazy/scared
    uint32_t next_crazy_look; // For crazy googly eyes timing
    float crazy_left_look_x, crazy_left_look_y;     // Current crazy eye positions
    float crazy_right_look_x, crazy_right_look_y;
    float crazy_left_target_x, crazy_left_target_y;  // Target positions for smooth motion
    float crazy_right_target_x, crazy_right_target_y;
} face_state_t;

static face_state_t face = {
    .left_eye_open = 1.0f, .right_eye_open = 1.0f,
    .target_left_eye = 1.0f, .target_right_eye = 1.0f,
    .look_x = 0, .look_y = 0,
    .left_brow_angle = 0, .right_brow_angle = 0,
    .left_brow_height = 0, .right_brow_height = 0,
    .mouth_open = 0, .mouth_width = 1.0f, .mouth_curve = 0,
    .emotion = EMO_NORMAL,
};

// Helper functions
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Track last played emotion to avoid replaying sounds
static emotion_t last_sound_emotion = EMO_COUNT;

// ============================================================================
// SPLASH SCREEN
// ============================================================================

static void draw_splash_screen(const char *text) {
    ssd1306_fill();  // White background
    
    int text_len = strlen(text);
    int char_w = 16;
    int char_h = 24;
    int total_w = text_len * char_w + (text_len - 1) * 4;
    int start_x = (SCREEN_WIDTH - total_w) / 2;
    int start_y = (SCREEN_HEIGHT - char_h) / 2;
    
    for (int i = 0; i < text_len; i++) {
        int cx = start_x + i * (char_w + 4);
        char c = text[i];
        
        if (c == 'K') {
            for (int y = 0; y < char_h; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            for (int d = 0; d < char_h/2; d++) {
                for (int t = 0; t < 4; t++) {
                    ssd1306_set_pixel(cx + 4 + d * 2/3 + t, start_y + char_h/2 - d, false);
                }
            }
            for (int d = 0; d < char_h/2; d++) {
                for (int t = 0; t < 4; t++) {
                    ssd1306_set_pixel(cx + 4 + d * 2/3 + t, start_y + char_h/2 + d, false);
                }
            }
        } else if (c == 'R') {
            for (int y = 0; y < char_h; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            for (int x = 0; x < char_w - 2; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            for (int x = 0; x < char_w - 4; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + char_h/2 - 2 + y, false);
                }
            }
            for (int y = 0; y < char_h/2; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + char_w - 4 + x, start_y + y, false);
                }
            }
            for (int d = 0; d < char_h/2; d++) {
                for (int t = 0; t < 4; t++) {
                    ssd1306_set_pixel(cx + 4 + d * 2/3 + t, start_y + char_h/2 + d, false);
                }
            }
        } else if (c == 'G') {
            for (int x = 2; x < char_w; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            for (int x = 2; x < char_w; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + char_h - 4 + y, false);
                }
            }
            for (int y = 0; y < char_h; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            for (int y = char_h/2; y < char_h; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + char_w - 4 + x, start_y + y, false);
                }
            }
            for (int x = char_w/2; x < char_w; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + char_h/2 - 2 + y, false);
                }
            }
        }
    }
    
    ssd1306_update();
}

// ============================================================================
// EMOTION SYSTEM
// ============================================================================

static void apply_emotion_internal(emotion_t emo, bool play_sound) {
    face.emotion = emo;
    
    // Reset to defaults
    face.target_left_eye = 1.0f;
    face.target_right_eye = 1.0f;
    face.left_brow_angle = 0;
    face.right_brow_angle = 0;
    face.left_brow_height = 0;
    face.right_brow_height = 0;
    face.shake = 0;
    face.mouth_curve = 0;
    face.mouth_open = 0;
    
    switch (emo) {
        case EMO_NORMAL:
            face.left_brow_angle = -0.1f;
            face.right_brow_angle = -0.1f;
            face.mouth_curve = 0;
            break;
            
        case EMO_HAPPY:
            face.left_brow_height = -3;
            face.right_brow_height = -3;
            face.left_brow_angle = -0.3f;
            face.right_brow_angle = -0.3f;
            face.mouth_curve = 0.7f;
            break;
            
        case EMO_LAUGHING:
            face.target_left_eye = 0.35f;
            face.target_right_eye = 0.35f;
            face.left_brow_height = -4;
            face.right_brow_height = -4;
            face.left_brow_angle = -0.4f;
            face.right_brow_angle = -0.4f;
            face.mouth_curve = 1.0f;
            face.mouth_open = 0.8f;
            break;
            
        case EMO_ANGRY:
            face.left_brow_angle = 1.0f;
            face.right_brow_angle = 1.0f;
            face.left_brow_height = 3;
            face.right_brow_height = 3;
            face.mouth_curve = -0.5f;
            break;
            
        case EMO_SAD:
            face.left_brow_angle = -0.9f;
            face.right_brow_angle = -0.9f;
            face.left_brow_height = -1;
            face.right_brow_height = -1;
            face.target_left_eye = 0.65f;
            face.target_right_eye = 0.65f;
            face.mouth_curve = -0.8f;
            break;
            
        case EMO_SURPRISED:
            face.left_brow_height = -7;
            face.right_brow_height = -7;
            face.left_brow_angle = -0.5f;
            face.right_brow_angle = -0.5f;
            face.mouth_curve = 0;
            face.mouth_open = 1.0f;
            break;
            
        case EMO_SLEEPY:
            face.target_left_eye = 0.25f;
            face.target_right_eye = 0.25f;
            face.left_brow_angle = -0.4f;
            face.right_brow_angle = -0.4f;
            face.left_brow_height = 2;
            face.right_brow_height = 2;
            face.mouth_curve = 0;
            break;
            
        case EMO_SLEEPING:
            face.target_left_eye = 0.0f;
            face.target_right_eye = 0.0f;
            face.left_brow_angle = -0.2f;
            face.right_brow_angle = -0.2f;
            face.mouth_curve = 0;
            break;
            
        case EMO_CRAZY:
            face.left_brow_height = -5;
            face.right_brow_height = 3;
            face.left_brow_angle = -0.8f;
            face.right_brow_angle = 0.9f;
            face.shake = 1.0f;
            face.mouth_curve = 1.0f;
            face.mouth_open = 0.6f;
            break;
            
        case EMO_LOVE:
            face.left_brow_height = -3;
            face.right_brow_height = -3;
            face.left_brow_angle = -0.4f;
            face.right_brow_angle = -0.4f;
            face.mouth_curve = 0.5f;
            break;
            
        case EMO_WINK:
            face.target_left_eye = 1.0f;
            face.target_right_eye = 0.0f;
            face.left_brow_height = -3;
            face.right_brow_height = 1;
            face.left_brow_angle = -0.3f;
            face.right_brow_angle = 0.2f;
            face.mouth_curve = 0.6f;
            break;
            
        case EMO_SMUG:
            face.target_left_eye = 0.6f;
            face.target_right_eye = 0.9f;
            face.left_brow_height = 2;
            face.right_brow_height = -4;
            face.left_brow_angle = 0.4f;
            face.right_brow_angle = -0.5f;
            face.mouth_curve = 0.4f;
            break;
            
        case EMO_SCARED:
            face.left_brow_angle = -1.0f;
            face.right_brow_angle = -1.0f;
            face.left_brow_height = -5;
            face.right_brow_height = -5;
            face.shake = 0;
            face.anim_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            face.mouth_curve = -0.3f;
            face.mouth_open = 0.3f;
            break;
            
        case EMO_BIRTHDAY:
            face.left_brow_height = -4;
            face.right_brow_height = -4;
            face.left_brow_angle = -0.4f;
            face.right_brow_angle = -0.4f;
            face.target_left_eye = 0.4f;
            face.target_right_eye = 0.4f;
            face.mouth_curve = 1.0f;
            face.mouth_open = 0.7f;
            break;
            
        case EMO_TROLLFACE:
            // The iconic trollface: squinty downturned eyes, raised brows, exaggerated troll mouth
            face.target_left_eye = 0.3f;    // Squinty eyes
            face.target_right_eye = 0.3f;
            face.left_brow_height = -6;     // Raised high
            face.right_brow_height = -5;
            face.left_brow_angle = -0.6f;   // Curved smugly
            face.right_brow_angle = -0.5f;
            face.mouth_curve = 1.0f;        // Maximum smile
            face.mouth_open = 1.0f;         // Wide open mouth
            face.mouth_width = 2.0f;        // Extra wide
            break;
            
        default:
            break;
    }
    
    // Play sound effect for this emotion
    if (play_sound && emo != last_sound_emotion) {
        last_sound_emotion = emo;
        
        sound_effect_t sfx = SFX_NONE;
        switch (emo) {
            case EMO_NORMAL:    sfx = SFX_NONE; break;
            case EMO_HAPPY:     sfx = SFX_HAPPY; break;
            case EMO_LAUGHING:  sfx = SFX_LAUGHING; break;
            case EMO_SAD:       sfx = SFX_SAD; break;
            case EMO_ANGRY:     sfx = SFX_ANGRY; break;
            case EMO_SURPRISED: sfx = SFX_SURPRISED; break;
            case EMO_SLEEPY:    sfx = SFX_SLEEPY; break;
            case EMO_SLEEPING:  sfx = SFX_SLEEPING; break;
            case EMO_CRAZY:     sfx = SFX_CRAZY; break;
            case EMO_LOVE:      sfx = SFX_LOVE; break;
            case EMO_WINK:      sfx = SFX_WINK; break;
            case EMO_SMUG:      sfx = SFX_SMUG; break;
            case EMO_SCARED:    sfx = SFX_SCARED; break;
            case EMO_BIRTHDAY:  sfx = SFX_BIRTHDAY; break;
            case EMO_TROLLFACE: sfx = SFX_SMUG; break;  // Reuse smug sound for troll
            default: break;
        }
        if (sfx != SFX_NONE) {
            buzzer_play_sfx(sfx);
        }
    }
}

static void apply_emotion(emotion_t emo) {
    apply_emotion_internal(emo, true);
}

static void apply_emotion_silent(emotion_t emo) {
    apply_emotion_internal(emo, false);
}

// ============================================================================
// ANIMATION UPDATE
// ============================================================================

static void update_face(uint32_t now) {
    // Smooth eye transitions
    face.left_eye_open = lerp(face.left_eye_open, face.target_left_eye, 0.3f);
    face.right_eye_open = lerp(face.right_eye_open, face.target_right_eye, 0.3f);
    
    // Look direction
    if (face.look_x != face.target_look_x) 
        face.look_x += (face.target_look_x > face.look_x) ? 1 : -1;
    if (face.look_y != face.target_look_y)
        face.look_y += (face.target_look_y > face.look_y) ? 1 : -1;
    
    // Bounce animation for laughing
    if (face.emotion == EMO_LAUGHING) {
        face.bounce = sinf(now * 0.02f) * 0.5f + 0.5f;
    } else {
        face.bounce = lerp(face.bounce, 0, 0.2f);
    }
    
    // Blinking (skip during sleep, wink, or crazy)
    if (face.emotion != EMO_SLEEPING && face.emotion != EMO_WINK && face.emotion != EMO_CRAZY) {
        if (now >= face.next_blink) {
            if (face.target_left_eye > 0.5f && face.target_right_eye > 0.5f) {
                face.target_left_eye = 0;
                face.target_right_eye = 0;
                face.next_blink = now + 100;
            } else if (face.emotion != EMO_SLEEPY && face.emotion != EMO_SAD) {
                apply_emotion_silent(face.emotion);
                face.next_blink = now + 2500 + (esp_random() % 4000);
            } else {
                face.next_blink = now + 150;
            }
        }
    }
    
    // Looking around (skip during sleep)
    if (face.emotion != EMO_SLEEPING && now >= face.next_look) {
        face.target_look_x = ((int)(esp_random() % 15)) - 7;
        face.target_look_y = ((int)(esp_random() % 9)) - 4;
        face.next_look = now + 800 + (esp_random() % 2000);
    }

    // Crazy googly eyes direction changes
    if (face.emotion == EMO_CRAZY && now >= face.next_crazy_look) {
        face.crazy_left_look_x = ((int)(esp_random() % 12)) - 6;
        face.crazy_left_look_y = ((int)(esp_random() % 8)) - 4;
        face.crazy_right_look_x = ((int)(esp_random() % 12)) - 6;
        face.crazy_right_look_y = ((int)(esp_random() % 8)) - 4;
        face.next_crazy_look = now + 800 + (esp_random() % 1200);  // Change every 800-2000ms
    }
    
    // Emotion changes - predictable rotation for development
    if (now >= face.next_emotion) {
        static emotion_t emotion_sequence[] = {
            EMO_NORMAL, EMO_HAPPY, EMO_LAUGHING, EMO_SURPRISED, EMO_WINK,
            EMO_SMUG, EMO_LOVE, EMO_SLEEPY, EMO_SLEEPING, EMO_SAD,
            EMO_ANGRY, EMO_SCARED, EMO_CRAZY, EMO_BIRTHDAY, EMO_TROLLFACE
        };
        static int current_emotion_index = 0;

        emotion_t new_emo = emotion_sequence[current_emotion_index];
        current_emotion_index = (current_emotion_index + 1) % (sizeof(emotion_sequence) / sizeof(emotion_sequence[0]));

        apply_emotion(new_emo);

        int duration = 3000 + (esp_random() % 5000);
        if (new_emo == EMO_SLEEPING) duration = 5000 + (esp_random() % 3000);
        if (new_emo == EMO_CRAZY) duration = 4000 + (esp_random() % 4000);  // Longer crazy duration
        if (new_emo == EMO_BIRTHDAY) duration = 8000;  // Longer duration for birthday song

        face.next_emotion = now + duration;
        face.anim_start = now;

        ESP_LOGI(TAG, "Emotion: %d (%s)", new_emo,
                 new_emo == EMO_NORMAL ? "NORMAL" :
                 new_emo == EMO_HAPPY ? "HAPPY" :
                 new_emo == EMO_LAUGHING ? "LAUGHING" :
                 new_emo == EMO_SURPRISED ? "SURPRISED" :
                 new_emo == EMO_WINK ? "WINK" :
                 new_emo == EMO_SMUG ? "SMUG" :
                 new_emo == EMO_LOVE ? "LOVE" :
                 new_emo == EMO_SLEEPY ? "SLEEPY" :
                 new_emo == EMO_SLEEPING ? "SLEEPING" :
                 new_emo == EMO_SAD ? "SAD" :
                 new_emo == EMO_ANGRY ? "ANGRY" :
                 new_emo == EMO_SCARED ? "SCARED" :
                 new_emo == EMO_CRAZY ? "CRAZY" :
                 new_emo == EMO_BIRTHDAY ? "BIRTHDAY" :
                 new_emo == EMO_TROLLFACE ? "TROLLFACE" : "UNKNOWN");
    }
}

// ============================================================================
// 3D RENDERING CONTEXT
// ============================================================================

static render_ctx_t render_ctx;
static float head_rotation = 0;
static float head_tilt = 0;

// Anime face parameters
typedef struct {
    int x, y;
    int look_x, look_y;
    float blink;
} anime_eye_t;

static anime_eye_t left_eye, right_eye;

// Note: 3D mouth mesh code removed - using 2D drawing for reliability
// The 3D rendering infrastructure remains available for future experiments

// ============================================================================
// 3D SCENE INITIALIZATION
// ============================================================================

static void init_3d_scene(void) {
    if (!render3d_init(&render_ctx, SCREEN_WIDTH, SCREEN_HEIGHT)) {
        ESP_LOGE(TAG, "Failed to init 3D renderer!");
        return;
    }
    
    // Camera setup - positioned to see mouth clearly
    // Camera slightly above looking down to position mouth in lower part of screen
    camera_t cam = {
        .position = vec3_create(0, 0.5f, 2.0f),
        .target = vec3_create(0, 0, 0),
        .up = vec3_create(0, 1, 0),
        .fov = 40.0f,
        .near_plane = 0.1f,
        .far_plane = 100.0f
    };
    render3d_set_camera(&render_ctx, &cam);
    
    // Frontal lighting from above
    light_t light = {
        .direction = vec3_create(0.0f, -0.5f, -1.0f),
        .intensity = 0.8f,
        .ambient = 0.3f
    };
    render3d_set_light(&render_ctx, &light);
    
    // Initialize eye positions
    left_eye = (anime_eye_t){.x = 32, .y = 26, .look_x = 0, .look_y = 0, .blink = 0};
    right_eye = (anime_eye_t){.x = 96, .y = 26, .look_x = 0, .look_y = 0, .blink = 0};
}

// ============================================================================
// 2D DRAWING HELPERS (for eyes, eyebrows, effects)
// ============================================================================

// Draw a heart outline for love eyes (beating animation)
static void draw_heart_2d(int cx, int cy, int base_size) {
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    float beat_phase = (float)(now % 600) / 600.0f;
    float beat;
    if (beat_phase < 0.15f) {
        beat = beat_phase / 0.15f;
    } else if (beat_phase < 0.3f) {
        beat = 1.0f - (beat_phase - 0.15f) / 0.15f;
    } else {
        beat = 0;
    }
    float size = base_size + beat * 3.0f;

    // Draw heart outline using parametric equation (thicker/darker)
    for (float t = 0; t < 6.28f; t += 0.06f) {
        float x = 16.0f * sinf(t) * sinf(t) * sinf(t);
        float y = 13.0f * cosf(t) - 5.0f * cosf(2*t) - 2.0f * cosf(3*t) - cosf(4*t);
        int px = cx + (int)(x * size / 16.0f);
        int py = cy - (int)(y * size / 17.0f);

        // Draw thicker outline by setting multiple pixels around each point
        ssd1306_set_pixel(px, py, false);
        ssd1306_set_pixel(px + 1, py, false);
        ssd1306_set_pixel(px, py + 1, false);
        ssd1306_set_pixel(px - 1, py, false);
        ssd1306_set_pixel(px, py - 1, false);
    }
}


// Draw anime-style eye
static void draw_anime_eye_2d(int cx, int cy, int look_x, int look_y, float openness, 
                               bool is_left, emotion_t emo) {
    int eye_w = 40;
    int eye_h = 34;
    int half_w = eye_w / 2;
    int half_h = eye_h / 2;
    
    if (face.shake > 0 && face.emotion != EMO_CRAZY) {
        cx += (int)(face.shake * ((esp_random() % 5) - 2));
    }
    
    if (emo == EMO_LOVE && openness > 0.3f) {
        draw_heart_2d(cx, cy, 18);
        return;
    }
    
    // Crazy eyes use normal eyes but with bloodshot effect
    
    
    if (openness < 0.2f) {
        for (int x = -half_w; x <= half_w; x++) {
            float t = (float)x / (float)half_w;
            int curve = (emo == EMO_HAPPY || emo == EMO_LAUGHING) ? 4 : 2;
            int y = (int)(t * t * curve);
            ssd1306_set_pixel(cx + x, cy + y, false);
            ssd1306_set_pixel(cx + x, cy + y + 1, false);
        }
        return;
    }
    
    // Trollface gets special squinty downturned eyes (opposite of the smile)
    if (emo == EMO_TROLLFACE && openness < 0.8f) {
        // Draw the downturned squinty eye - curves downward opposite to the crooked smile
        for (int x = -half_w; x <= half_w; x++) {
            float t = (float)x / (float)half_w;
            // Downturned curve - opposite of the upward smile
            float downturn = is_left ? -0.4f : 0.4f;  // Asymmetric downturn
            int y = (int)((t * t * 8) + (t * downturn * 4));  // More curved and downturned
            // Thick downturned line
            ssd1306_set_pixel(cx + x, cy + y - 1, false);
            ssd1306_set_pixel(cx + x, cy + y, false);
            ssd1306_set_pixel(cx + x, cy + y + 1, false);
            ssd1306_set_pixel(cx + x, cy + y + 2, false);
        }
        // Add a small knowing highlight
        ssd1306_set_pixel(cx + (is_left ? -4 : 4), cy + 2, false);
        return;
    }
    
    int visible_h = (int)(eye_h * openness);
    if (visible_h < 8) visible_h = 8;
    int adj_half_h = visible_h / 2;
    
    if (emo == EMO_SURPRISED) {
        adj_half_h = half_h + 3;
    }
    
    for (int x = -half_w; x <= half_w; x++) {
        float t = (float)x / (float)half_w;
        int y = -adj_half_h + (int)(fabsf(t) * 3);
        ssd1306_set_pixel(cx + x, cy + y, false);
        ssd1306_set_pixel(cx + x, cy + y + 1, false);
        ssd1306_set_pixel(cx + x, cy + y + 2, false);
    }
    
    for (int x = -half_w + 2; x <= half_w - 2; x++) {
        float t = (float)x / (float)half_w;
        int y = adj_half_h - 2 - (int)(fabsf(t) * 2);
        ssd1306_set_pixel(cx + x, cy + y, false);
    }
    
    for (int y = -adj_half_h + 3; y < adj_half_h - 3; y++) {
        float t = (float)(y + adj_half_h) / (float)(adj_half_h * 2);
        int left_x = -half_w + (int)(t * 5);
        int right_x = half_w - (int)(t * 5);
        ssd1306_set_pixel(cx + left_x, cy + y, false);
        ssd1306_set_pixel(cx + right_x, cy + y, false);
    }
    
    int iris_cx = cx + look_x;
    int iris_cy = cy + look_y + 1;
    int iris_w = 11;
    int iris_h = (int)(9 * openness);
    if (iris_h < 5) iris_h = 5;
    
    for (int dy = -iris_h; dy <= iris_h; dy++) {
        for (int dx = -iris_w; dx <= iris_w; dx++) {
            float ex = (float)dx / iris_w;
            float ey = (float)dy / iris_h;
            if (ex*ex + ey*ey <= 1.0f) {
                float gradient = (float)(dy + iris_h) / (float)(2 * iris_h);
                int threshold = (int)(gradient * 4);
                bool pixel;
                if (threshold == 0) pixel = false;
                else if (threshold == 1) pixel = ((dx + dy) % 2 != 0);
                else if (threshold == 2) pixel = ((dx + dy) % 2 == 0);
                else if (threshold == 3) pixel = ((dx % 2 == 0) && (dy % 2 == 0));
                else pixel = true;
                ssd1306_set_pixel(iris_cx + dx, iris_cy + dy, pixel);
            }
        }
    }
    
    // Draw pupil (normal for all emotions)
    int pupil_w = (emo == EMO_SURPRISED) ? 2 : 4;
    int pupil_h = (emo == EMO_SURPRISED) ? 3 : 6;
    for (int dy = -pupil_h; dy <= pupil_h; dy++) {
        for (int dx = -pupil_w; dx <= pupil_w; dx++) {
            float ex = (float)dx / pupil_w;
            float ey = (float)dy / pupil_h;
            if (ex*ex + ey*ey <= 1.0f) {
                ssd1306_set_pixel(iris_cx + dx, iris_cy + dy, false);
            }
        }
    }

    int hl_x = iris_cx - 5;
    int hl_y = iris_cy - 4;
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            if (dx*dx + dy*dy <= 5) {
                ssd1306_set_pixel(hl_x + dx, hl_y + dy, true);
            }
        }
    }
    ssd1306_set_pixel(iris_cx + 4, iris_cy + 3, true);
    ssd1306_set_pixel(iris_cx + 5, iris_cy + 3, true);
    
    if (emo == EMO_WINK && !is_left) {
        ssd1306_fill_rect(cx - half_w - 2, cy - half_h - 2, eye_w + 4, eye_h + 4, true);
        for (int x = -half_w; x <= half_w; x++) {
            float t = (float)x / (float)half_w;
            int y = (int)(t * t * 4);
            ssd1306_set_pixel(cx + x, cy + y, false);
            ssd1306_set_pixel(cx + x, cy + y + 1, false);
        }
    }
}

// Draw eyebrow
static void draw_eyebrow_2d(int cx, int cy, bool is_left, float angle, float height_offset) {
    int brow_w = 28;  // Wider to match the big anime eyes
    int dir = is_left ? 1 : -1;
    
    cy += (int)height_offset;
    
    for (int i = 0; i < brow_w; i++) {
        float t = (float)i / (float)brow_w;
        int x = cx + (i - brow_w/2) * dir;
        
        float base_curve = sinf(t * 3.14159f) * 5.0f;  // Slightly more curve
        float tilt = angle * (t - 0.5f) * 10.0f * dir;  // More expressive tilt
        int y = cy - (int)(base_curve + tilt);
        
        // Dithered eyebrows - softer/fainter appearance
        // Use checkerboard pattern for a gray look
        for (int row = 0; row < 5; row++) {
            // Determine if this row should be drawn based on taper
            bool draw_row = true;
            if (row == 3 && (t < 0.15f || t > 0.85f)) draw_row = false;
            if (row == 4 && (t < 0.3f || t > 0.7f)) draw_row = false;
            
            if (draw_row) {
                // Dithering: checkerboard pattern makes it appear gray
                // Outer rows more dithered (fainter), inner rows less dithered (darker)
                bool dither;
                if (row == 0 || row == 4) {
                    // Outer edges: sparse dithering (very faint)
                    dither = ((x + y + row) % 3 == 0);
                } else if (row == 1 || row == 3) {
                    // Middle-outer: checkerboard (medium gray)
                    dither = ((x + y + row) % 2 == 0);
                } else {
                    // Center row: denser (darker but still soft)
                    dither = ((x + y) % 2 == 0) || ((x % 2 == 0) && (y % 2 == 0));
                }
                
                if (dither) {
                    ssd1306_set_pixel(x, y + row, false);
                }
            }
        }
    }
}

// Draw ZZZ for sleeping
static void draw_zzz_2d(int x, int y) {
    for (int i = 0; i < 3; i++) {
        int zx = x + i * 8;
        int zy = y - i * 4;
        int size = 4 + i;
        for (int j = 0; j < size; j++) {
            ssd1306_set_pixel(zx + j, zy, false);
            ssd1306_set_pixel(zx + size - j - 1, zy + j, false);
            ssd1306_set_pixel(zx + j, zy + size - 1, false);
        }
    }
}

// Helper: Draw teeth inside an open mouth area
static void draw_teeth(int cx, int top_y, int bot_y, int width, int num_teeth) {
    if (num_teeth <= 0) return;
    
    int mouth_height = bot_y - top_y;
    if (mouth_height < 3) return;
    
    int tooth_gap = (width * 2) / num_teeth;
    int tooth_height = mouth_height / 2;
    if (tooth_height < 2) tooth_height = 2;
    
    // Top teeth (hang down from top)
    for (int t = 0; t < num_teeth; t++) {
        int tx = cx - width + t * tooth_gap + tooth_gap / 2;
        for (int th = 0; th < tooth_height; th++) {
            // Each tooth is a small rectangle
            ssd1306_set_pixel(tx, top_y + th, true);
            ssd1306_set_pixel(tx + 1, top_y + th, true);
        }
        // Tooth separator line
        ssd1306_set_pixel(tx + 2, top_y, false);
        ssd1306_set_pixel(tx + 2, top_y + 1, false);
    }
}

// Draw mouth based on emotion - now with teeth and more expression!
static void draw_mouth_2d(int cx, int cy, emotion_t emo) {
    int bounce_y = (int)(face.bounce * 2);
    cy += bounce_y;
    
    switch (emo) {
        case EMO_NORMAL: {
            // Gentle closed mouth with slight curve
            for (int x = -8; x <= 8; x++) {
                float t = (float)x / 8.0f;
                int y = (int)(t * t * 1.5f);  // Very slight smile
                ssd1306_set_pixel(cx + x, cy + y, false);
            }
            break;
        }
            
        case EMO_HAPPY: {
            // Nice curved smile with thicker line
            int width = 12;
            int curve = 5;
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                int y = (int)(curve * (1.0f - t * t));
                ssd1306_set_pixel(cx + x, cy + y, false);
                ssd1306_set_pixel(cx + x, cy + y + 1, false);
            }
            break;
        }
            
        case EMO_LAUGHING: {
            // Wide open laughing mouth with teeth
            int width = 14;
            int top_curve = 2;
            int bot_curve = 8;
            
            // Top lip (slight curve)
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                int y = (int)(-top_curve * (1.0f - t * t));
                ssd1306_set_pixel(cx + x, cy + y - 2, false);
                ssd1306_set_pixel(cx + x, cy + y - 1, false);
            }
            
            // Bottom lip (big curve)
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                int y = (int)(bot_curve * (1.0f - t * t));
                ssd1306_set_pixel(cx + x, cy + y + 2, false);
                ssd1306_set_pixel(cx + x, cy + y + 3, false);
            }
            
            // Teeth!
            draw_teeth(cx, cy - 1, cy + 5, width - 3, 8);
            
            // Dark mouth interior
            for (int y = cy + 1; y < cy + 6; y++) {
                int fill_w = width - 2 - (y - cy) / 2;
                for (int x = -fill_w; x <= fill_w; x++) {
                    ssd1306_set_pixel(cx + x, y, false);
                }
            }
            break;
        }
            
        case EMO_SAD: {
            // Wobbly frown
            for (int x = -8; x <= 8; x++) {
                float t = (float)x / 8.0f;
                int y = (int)(-4 * (1.0f - t * t)) + 4;
                ssd1306_set_pixel(cx + x, cy + y, false);
                ssd1306_set_pixel(cx + x, cy + y + 1, false);
            }
            break;
        }
            
        case EMO_ANGRY: {
            // Tight grimace with visible teeth (gritting)
            int width = 10;
            
            // Tight lips
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                int y = (int)(-2 * (1.0f - t * t));
                ssd1306_set_pixel(cx + x, cy + y, false);
                ssd1306_set_pixel(cx + x, cy + y + 3, false);
            }
            
            // Gritted teeth showing through
            for (int t = 0; t < 6; t++) {
                int tx = cx - width + 2 + t * 3;
                ssd1306_set_pixel(tx, cy + 1, true);
                ssd1306_set_pixel(tx + 1, cy + 1, true);
                ssd1306_set_pixel(tx, cy + 2, true);
                ssd1306_set_pixel(tx + 1, cy + 2, true);
            }
            break;
        }
            
        case EMO_SURPRISED: {
            // Round O mouth
            int rx = 6;
            int ry = 8;
            for (int angle = 0; angle < 360; angle += 8) {
                float rad = angle * 3.14159f / 180.0f;
                int x = (int)(cosf(rad) * rx);
                int y = (int)(sinf(rad) * ry);
                ssd1306_set_pixel(cx + x, cy + y, false);
                ssd1306_set_pixel(cx + x + 1, cy + y, false);
            }
            // Dark inside
            for (int dy = -ry + 2; dy < ry - 2; dy++) {
                int w = (int)(sqrtf(1.0f - (float)(dy * dy) / (ry * ry)) * (rx - 2));
                for (int dx = -w; dx <= w; dx++) {
                    ssd1306_set_pixel(cx + dx, cy + dy, false);
                }
            }
            break;
        }
            
        case EMO_SLEEPY: {
            // Slightly open, relaxed
            for (int x = -5; x <= 5; x++) {
                ssd1306_set_pixel(cx + x, cy, false);
                ssd1306_set_pixel(cx + x, cy + 1, false);
            }
            break;
        }
        
        case EMO_SLEEPING: {
            // Small peaceful mouth
            for (int x = -4; x <= 4; x++) {
                float t = (float)x / 4.0f;
                int y = (int)(t * t * 2);
                ssd1306_set_pixel(cx + x, cy + y, false);
            }
            break;
        }
            
        case EMO_LOVE: {
            // Sweet small smile
            int width = 10;
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                int y = (int)(4 * (1.0f - t * t));
                ssd1306_set_pixel(cx + x, cy + y, false);
            }
            break;
        }
            
        case EMO_WINK: {
            // Playful smirk - asymmetric
            for (int x = -10; x <= 10; x++) {
                float t = (float)x / 10.0f;
                int y = (int)(4 * (1.0f - t * t) + t * 2);  // Asymmetric curve
                ssd1306_set_pixel(cx + x, cy + y, false);
                ssd1306_set_pixel(cx + x, cy + y + 1, false);
            }
            break;
        }
        
        case EMO_SMUG: {
            // One-sided smirk
            for (int x = -8; x <= 12; x++) {
                float t = (float)(x + 2) / 10.0f;
                int y = (int)(5 * (1.0f - t * t) + t * 3);
                ssd1306_set_pixel(cx + x, cy + y, false);
                ssd1306_set_pixel(cx + x, cy + y + 1, false);
            }
            break;
        }
            
        case EMO_SCARED: {
            // Wobbly terrified mouth
            for (int x = -10; x <= 10; x++) {
                int wave = (int)(sinf(x * 0.7f) * 3);
                ssd1306_set_pixel(cx + x, cy + wave, false);
                ssd1306_set_pixel(cx + x, cy + wave + 1, false);
            }
            // Small opening in center
            for (int x = -3; x <= 3; x++) {
                ssd1306_set_pixel(cx + x, cy - 1, false);
                ssd1306_set_pixel(cx + x, cy + 3, false);
            }
            break;
        }
            
        case EMO_CRAZY: {
            // Unhinged wide grin with teeth
            int width = 16;
            
            // Top lip
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                int y = (int)(-2 * (1.0f - t * t));
                ssd1306_set_pixel(cx + x, cy + y - 2, false);
                ssd1306_set_pixel(cx + x, cy + y - 1, false);
            }
            
            // Bottom lip - extra wide crazy smile
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                int y = (int)(7 * (1.0f - t * t));
                ssd1306_set_pixel(cx + x, cy + y + 3, false);
                ssd1306_set_pixel(cx + x, cy + y + 4, false);
            }
            
            // Teeth
            draw_teeth(cx, cy, cy + 7, width - 2, 10);
            break;
        }
            
        case EMO_BIRTHDAY: {
            // Excited open smile with teeth
            int width = 14;
            
            // Top lip
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                int y = (int)(-1 * (1.0f - t * t));
                ssd1306_set_pixel(cx + x, cy + y - 1, false);
                ssd1306_set_pixel(cx + x, cy + y, false);
            }
            
            // Bottom lip
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                int y = (int)(6 * (1.0f - t * t));
                ssd1306_set_pixel(cx + x, cy + y + 3, false);
                ssd1306_set_pixel(cx + x, cy + y + 4, false);
            }
            
            // Teeth
            draw_teeth(cx, cy + 1, cy + 6, width - 3, 8);
            break;
        }
        
        case EMO_TROLLFACE: {
            // THE TROLLFACE GRIN - extra wide, maximum smile, realistic teeth
            // This is the iconic "problem?" expression
            int width = 24;  // Even wider for exaggerated smile

            // The grin curves up dramatically at the corners - more exaggerated
            // Top lip - more pronounced upward curve
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                // More asymmetric and exaggerated smug curve
                int y = (int)(-4 * (1.0f - t * t) + t * 3);
                ssd1306_set_pixel(cx + x, cy + y - 4, false);
                ssd1306_set_pixel(cx + x, cy + y - 3, false);
                ssd1306_set_pixel(cx + x, cy + y - 2, false);
                ssd1306_set_pixel(cx + x, cy + y - 1, false);
            }

            // Bottom lip - even more dramatic grin curve, higher at the sides
            for (int x = -width; x <= width; x++) {
                float t = (float)x / (float)width;
                float abs_t = fabsf(t);
                // More exaggerated trollface grin shape
                int y = (int)(8 * (1.0f - t * t) - abs_t * abs_t * 5);
                ssd1306_set_pixel(cx + x, cy + y + 5, false);
                ssd1306_set_pixel(cx + x, cy + y + 6, false);
                ssd1306_set_pixel(cx + x, cy + y + 7, false);
                ssd1306_set_pixel(cx + x, cy + y + 8, false);
            }

            // Realistic teeth - individual tooth shapes
            int teeth_top = cy - 2;
            int teeth_bottom = cy + 8;
            int num_teeth = 10;  // Slightly fewer for more defined teeth
            int tooth_width = (width * 2 - 6) / num_teeth;

            for (int t = 0; t < num_teeth; t++) {
                int tx = cx - width + 3 + t * tooth_width + tooth_width / 2;
                // Draw each tooth with more realistic shape (white with dark outline)
                for (int ty = teeth_top; ty < teeth_bottom; ty++) {
                    // Tooth shape varies by position for realism
                    int tooth_height = teeth_bottom - ty;
                    int left_edge = tx - (tooth_height / 3);
                    int right_edge = tx + (tooth_height / 3) + 1;

                    // Fill tooth with white
                    if (tx >= left_edge && tx <= right_edge && ty >= teeth_top && ty < teeth_bottom) {
                        ssd1306_set_pixel(tx, ty, true);
                        ssd1306_set_pixel(tx + 1, ty, true);
                    }
                }
                // Dark separator between teeth
                for (int ty = teeth_top; ty < teeth_bottom; ty++) {
                    ssd1306_set_pixel(tx + tooth_width / 2 + 1, ty, false);
                }
            }

            // Dark gaps at corners of mouth (the iconic trollface look)
            for (int i = 0; i < 4; i++) {
                ssd1306_set_pixel(cx - width + i, cy - 3 + i, false);
                ssd1306_set_pixel(cx + width - i, cy - 4 + i, false);
            }
            break;
        }
            
        default: {
            // Fallback simple mouth
            for (int x = -6; x <= 6; x++) {
                ssd1306_set_pixel(cx + x, cy, false);
            }
            break;
        }
    }
}

// ============================================================================
// FALLING STARS OVERLAY
// ============================================================================

#define MAX_STARS 8
#define MAX_FLOATING_HEARTS 10

typedef struct {
    float x, y;
    float speed;
    float rotation;
    float rot_speed;
    int size;
    bool active;
} falling_star_t;

typedef struct {
    float x, y;
    float speed;
    float wobble;
    int size;
    bool active;
} floating_heart_t;

static falling_star_t stars[MAX_STARS];
static floating_heart_t hearts[MAX_FLOATING_HEARTS];
static bool stars_initialized = false;
static bool hearts_initialized = false;
static bool stars_enabled = false;
static bool hearts_enabled = false;

static void init_falling_stars(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = esp_random() % SCREEN_WIDTH;
        stars[i].y = -(int)(esp_random() % 30);
        stars[i].speed = 0.8f + (esp_random() % 100) / 80.0f;
        stars[i].rotation = (esp_random() % 628) / 100.0f;
        stars[i].rot_speed = 0.15f + (esp_random() % 20) / 80.0f;
        stars[i].size = 2 + (esp_random() % 3);
        stars[i].active = true;
    }
    stars_initialized = true;
}

static void update_falling_stars(void) {
    if (!stars_initialized) init_falling_stars();
    
    for (int i = 0; i < MAX_STARS; i++) {
        if (!stars[i].active) continue;
        
        stars[i].y += stars[i].speed;
        stars[i].rotation += stars[i].rot_speed;
        
        if (stars[i].y > SCREEN_HEIGHT + 10) {
            stars[i].x = esp_random() % SCREEN_WIDTH;
            stars[i].y = -(int)(esp_random() % 15);
            stars[i].speed = 0.8f + (esp_random() % 100) / 80.0f;
            stars[i].size = 2 + (esp_random() % 3);
            stars[i].rot_speed = 0.15f + (esp_random() % 20) / 80.0f;
        }
    }
}

static void draw_spinning_star(int cx, int cy, int size, float rotation) {
    for (int i = 0; i < 4; i++) {
        float angle = rotation + (i * M_PI / 2);
        int x1 = cx + (int)(cosf(angle) * size);
        int y1 = cy + (int)(sinf(angle) * size);
        
        int steps = size;
        for (int s = 0; s <= steps; s++) {
            int px = cx + (x1 - cx) * s / steps;
            int py = cy + (y1 - cy) * s / steps;
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                ssd1306_set_pixel(px, py, false);
            }
        }
    }
    if (cx >= 0 && cx < SCREEN_WIDTH && cy >= 0 && cy < SCREEN_HEIGHT) {
        ssd1306_set_pixel(cx, cy, false);
    }
}

static void set_falling_stars_enabled(bool enabled) {
    stars_enabled = enabled;
    if (enabled && !stars_initialized) {
        init_falling_stars();
    }
}

static void draw_falling_stars_overlay(void) {
    if (!stars_enabled) return;

    update_falling_stars();

    for (int i = 0; i < MAX_STARS; i++) {
        if (!stars[i].active) continue;
        if (stars[i].y < 0 || stars[i].y >= SCREEN_HEIGHT) continue;

        draw_spinning_star((int)stars[i].x, (int)stars[i].y,
                          stars[i].size, stars[i].rotation);
    }
}

// ============================================================================
// FLOATING HEARTS OVERLAY (for love emotion)
// ============================================================================

static void init_floating_hearts(void) {
    for (int i = 0; i < MAX_FLOATING_HEARTS; i++) {
        hearts[i].x = esp_random() % SCREEN_WIDTH;
        hearts[i].y = SCREEN_HEIGHT + (int)(esp_random() % 20);
        hearts[i].speed = 1.0f + (esp_random() % 100) / 100.0f;  // Faster: 1.0 to 2.0
        hearts[i].wobble = (esp_random() % 628) / 100.0f;  // Random phase
        hearts[i].size = 14 + (esp_random() % 7);  // Bigger: Size 14-20
        hearts[i].active = true;
    }
    hearts_initialized = true;
}

static void update_floating_hearts(void) {
    if (!hearts_initialized) init_floating_hearts();

    for (int i = 0; i < MAX_FLOATING_HEARTS; i++) {
        if (!hearts[i].active) continue;

        hearts[i].y -= hearts[i].speed;
        hearts[i].x += sinf(hearts[i].wobble) * 0.3f;
        hearts[i].wobble += 0.1f;

        // Reset heart when it goes off screen
        if (hearts[i].y < -10) {
            hearts[i].x = esp_random() % SCREEN_WIDTH;
            hearts[i].y = SCREEN_HEIGHT + (int)(esp_random() % 20);
            hearts[i].speed = 1.0f + (esp_random() % 100) / 100.0f;
            hearts[i].wobble = (esp_random() % 628) / 100.0f;
            hearts[i].size = 14 + (esp_random() % 7);
        }
    }
}

static void draw_floating_heart(int cx, int cy, int size) {
    // Draw a floating heart outline using parametric equation
    float scale = size / 6.0f;  // Scale based on desired size (size 14-20)
    for (float t = 0; t < 6.28f; t += 0.15f) {  // Fewer points for smaller hearts
        float x = 16.0f * sinf(t) * sinf(t) * sinf(t);
        float y = 13.0f * cosf(t) - 5.0f * cosf(2*t) - 2.0f * cosf(3*t) - cosf(4*t);
        int px = cx + (int)(x * scale / 16.0f);
        int py = cy - (int)(y * scale / 17.0f);

        // Draw single pixel for tiny hearts
        if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
            ssd1306_set_pixel(px, py, false);
        }
    }
}

static void set_floating_hearts_enabled(bool enabled) {
    hearts_enabled = enabled;
    if (enabled && !hearts_initialized) {
        init_floating_hearts();
    }
}

static void draw_floating_hearts_overlay(void) {
    if (!hearts_enabled) return;

    update_floating_hearts();

    for (int i = 0; i < MAX_FLOATING_HEARTS; i++) {
        if (!hearts[i].active) continue;
        if (hearts[i].y < -5 || hearts[i].y >= SCREEN_HEIGHT + 5) continue;

        draw_floating_heart((int)hearts[i].x, (int)hearts[i].y, hearts[i].size);
    }
}

// ============================================================================
// BIRTHDAY CAKE AND TEXT
// ============================================================================

static void draw_birthday_cake(void) {
    // Draw a simple cake outline in the center
    int cake_center_x = SCREEN_WIDTH / 2;
    int cake_center_y = SCREEN_HEIGHT / 2 - 10;

    // Cake base (rectangle)
    int cake_width = 40;
    int cake_height = 20;
    int cake_left = cake_center_x - cake_width / 2;
    int cake_top = cake_center_y - cake_height / 2;
    int cake_bottom = cake_top + cake_height;

    // Draw cake outline
    for (int x = cake_left; x <= cake_left + cake_width; x++) {
        ssd1306_set_pixel(x, cake_top, false);        // Top
        ssd1306_set_pixel(x, cake_bottom, false);     // Bottom
    }
    for (int y = cake_top; y <= cake_bottom; y++) {
        ssd1306_set_pixel(cake_left, y, false);       // Left
        ssd1306_set_pixel(cake_left + cake_width, y, false); // Right
    }

    // Cake top layer (smaller rectangle on top)
    int top_width = 35;
    int top_height = 8;
    int top_left = cake_center_x - top_width / 2;
    int top_top = cake_top - top_height;

    for (int x = top_left; x <= top_left + top_width; x++) {
        ssd1306_set_pixel(x, top_top, false);         // Top
    }
    for (int y = top_top; y <= cake_top; y++) {
        ssd1306_set_pixel(top_left, y, false);        // Left
        ssd1306_set_pixel(top_left + top_width, y, false); // Right
    }

    // Simple candle in the center
    int candle_x = cake_center_x;
    int candle_top = top_top - 8;
    for (int y = candle_top; y <= top_top; y++) {
        ssd1306_set_pixel(candle_x, y, false);        // Candle stick
    }
    // Candle flame (small triangle)
    ssd1306_set_pixel(candle_x, candle_top, false);
    ssd1306_set_pixel(candle_x - 1, candle_top + 1, false);
    ssd1306_set_pixel(candle_x + 1, candle_top + 1, false);
}

static void draw_birthday_text(void) {
    // Draw "HAPPY BIRTHDAY" at the bottom in large text (~12px tall)
    const char* text = "HAPPY BIRTHDAY";
    int text_len = strlen(text);
    int char_width = 8;  // Approximate width per character
    int total_width = text_len * char_width;
    int start_x = (SCREEN_WIDTH - total_width) / 2;
    int start_y = SCREEN_HEIGHT - 15;  // Near bottom with some margin

    // Simple large text rendering (each character is 8x12 pixels)
    for (int i = 0; i < text_len; i++) {
        int char_x = start_x + i * char_width;
        char c = text[i];

        // Draw each character as a simple 8x12 bitmap
        // Using basic ASCII character patterns
        for (int dy = 0; dy < 12; dy++) {
            for (int dx = 0; dx < 8; dx++) {
                int pixel_x = char_x + dx;
                int pixel_y = start_y + dy;

                if (pixel_x >= 0 && pixel_x < SCREEN_WIDTH &&
                    pixel_y >= 0 && pixel_y < SCREEN_HEIGHT) {

                    // Simple character patterns (very basic)
                    bool draw_pixel = false;

                    switch (c) {
                        case 'H':
                            if ((dx == 0 || dx == 7) || (dy == 6 && dx >= 1 && dx <= 6)) draw_pixel = true;
                            break;
                        case 'A':
                            if ((dx == 0 || dx == 7) && dy > 2) draw_pixel = true;
                            else if (dy == 0 || dy == 6) draw_pixel = true;
                            break;
                        case 'P':
                            if (dx == 0 || (dx == 7 && dy <= 6) || ((dy == 0 || dy == 6) && dx <= 6)) draw_pixel = true;
                            break;
                        case 'Y':
                            if ((dx == dy/2 || dx == 7 - dy/2) && dy <= 6) draw_pixel = true;
                            else if (dy > 6 && dx == 3) draw_pixel = true;
                            break;
                        case 'B':
                            if (dx == 0 || (dx == 6 && dy != 0 && dy != 6 && dy != 12) ||
                                ((dy == 0 || dy == 6 || dy == 11) && dx <= 6)) draw_pixel = true;
                            break;
                        case 'I':
                            if (dx == 3 || dy == 0 || dy == 11) draw_pixel = true;
                            break;
                        case 'R':
                            if (dx == 0 || (dx == 6 && dy <= 6) || dy == 0 || dy == 6) draw_pixel = true;
                            else if (dy > 6 && dx == dy - 6) draw_pixel = true;
                            break;
                        case 'T':
                            if (dy == 0 || dx == 3) draw_pixel = true;
                            break;
                        case 'D':
                            if (dx == 0 || (dx == 6 && dy > 0 && dy < 11) ||
                                ((dy == 0 || dy == 11) && dx <= 6)) draw_pixel = true;
                            break;
                        case ' ':
                            draw_pixel = false;
                            break;
                    }

                    if (draw_pixel) {
                        ssd1306_set_pixel(pixel_x, pixel_y, false);
                    }
                }
            }
        }
    }
}

// ============================================================================
// FACE UPDATE AND RENDERING
// ============================================================================

static void update_3d_face(uint32_t now) {
    update_face(now);
    
    // Head movement for pseudo-3D parallax effect
    head_rotation = sinf(now * 0.0008f) * 15.0f + face.look_x * 2.0f;
    head_tilt = sinf(now * 0.0012f) * 5.0f + face.bounce * 5.0f;
    
    // Update eye positions
    int base_left_x = 32, base_right_x = 96;
    int base_y = 26;
    
    int rot_offset = (int)(head_rotation * 0.8f);
    int bounce_y = (int)(face.bounce * 3);
    
    left_eye.x = base_left_x + rot_offset;
    right_eye.x = base_right_x + rot_offset;
    left_eye.y = base_y + (int)(head_tilt * 0.3f) + bounce_y;
    right_eye.y = base_y + (int)(head_tilt * 0.3f) + bounce_y;
    
    left_eye.blink = 1.0f - face.left_eye_open;
    right_eye.blink = 1.0f - face.right_eye_open;
    
    left_eye.look_x = face.look_x + (int)(head_rotation * 0.1f);
    left_eye.look_y = face.look_y;
    right_eye.look_x = face.look_x + (int)(head_rotation * 0.1f);
    right_eye.look_y = face.look_y;
}

static void draw_3d_face(void) {
    ssd1306_fill();  // White background
    
    int bounce_y = (int)(face.bounce * 3);
    
    // Scared animation
    int scared_wiggle_x = 0;
    int scared_look_x = 0;
    if (face.emotion == EMO_SCARED) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        float anim_time = (float)(now - face.anim_start) / 1000.0f;
        scared_wiggle_x = (int)(sinf(anim_time * 12.0f) * 2);
        float dart = sinf(anim_time * 18.0f) + 0.3f * sinf(anim_time * 31.0f);
        scared_look_x = (int)(dart * 4);
    }
    
    int left_eye_x = left_eye.x + scared_wiggle_x;
    int right_eye_x = right_eye.x + scared_wiggle_x;
    
    // ========================================
    // MOUTH (2D drawing - reliable and clear)
    // ========================================
    int mouth_x = (left_eye_x + right_eye_x) / 2;
    int mouth_y = 48 + (int)(head_tilt * 0.3f) + bounce_y;
    draw_mouth_2d(mouth_x, mouth_y, face.emotion);
    
    // Eyebrows
    int brow_y = left_eye.y - 18 + bounce_y;
    draw_eyebrow_2d(left_eye_x, brow_y, true, face.left_brow_angle, face.left_brow_height);
    draw_eyebrow_2d(right_eye_x, brow_y, false, face.right_brow_angle, face.right_brow_height);
    
    // Eyes
    float left_openness = face.left_eye_open;
    float right_openness = face.right_eye_open;
    int look_x = left_eye.look_x + scared_look_x;
    int look_y = left_eye.look_y;

    if (face.emotion == EMO_CRAZY) {
        // Crazy rolling eyes - smooth animated motion between random positions
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Update target positions every 400-600ms for smooth rolling effect
        if (now >= face.next_crazy_look) {
            face.crazy_left_target_x = ((int)(esp_random() % 14)) - 7;
            face.crazy_left_target_y = ((int)(esp_random() % 10)) - 5;
            face.crazy_right_target_x = ((int)(esp_random() % 14)) - 7;
            face.crazy_right_target_y = ((int)(esp_random() % 10)) - 5;
            face.next_crazy_look = now + 400 + (esp_random() % 200);
        }

        // Smoothly interpolate current positions toward targets
        face.crazy_left_look_x = lerp(face.crazy_left_look_x, face.crazy_left_target_x, 0.08f);
        face.crazy_left_look_y = lerp(face.crazy_left_look_y, face.crazy_left_target_y, 0.08f);
        face.crazy_right_look_x = lerp(face.crazy_right_look_x, face.crazy_right_target_x, 0.08f);
        face.crazy_right_look_y = lerp(face.crazy_right_look_y, face.crazy_right_target_y, 0.08f);

        int crazy_left_lx = look_x + (int)face.crazy_left_look_x;
        int crazy_left_ly = look_y + (int)face.crazy_left_look_y;
        int crazy_right_lx = look_x + (int)face.crazy_right_look_x;
        int crazy_right_ly = look_y + (int)face.crazy_right_look_y;

        draw_anime_eye_2d(left_eye_x, left_eye.y, crazy_left_lx, crazy_left_ly,
                          left_openness, true, face.emotion);
        draw_anime_eye_2d(right_eye_x, right_eye.y, crazy_right_lx, crazy_right_ly,
                          right_openness, false, face.emotion);
    } else {
        draw_anime_eye_2d(left_eye_x, left_eye.y, look_x, look_y,
                          left_openness, true, face.emotion);
        draw_anime_eye_2d(right_eye_x, right_eye.y, look_x, look_y,
                          right_openness, false, face.emotion);
    }
    
    // Special effects
    if (face.emotion == EMO_SLEEPING) {
        draw_zzz_2d(100, 12);
    }
    
    // Sweat drop for scared
    if (face.emotion == EMO_SCARED) {
        int drop_x = right_eye_x + 22;
        int drop_y = right_eye.y - 10;
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        int drip_offset = (int)((now - face.anim_start) / 100) % 3;
        for (int i = 0; i < 6 + drip_offset; i++) {
            ssd1306_set_pixel(drop_x, drop_y + i, false);
            if (i > 1 && i < 5) {
                ssd1306_set_pixel(drop_x - 1, drop_y + i, false);
                ssd1306_set_pixel(drop_x + 1, drop_y + i, false);
            }
        }
    }
    
    // Blush
    if (face.emotion == EMO_LOVE || face.emotion == EMO_HAPPY) {
        int blush_y = left_eye.y + 12;
        for (int i = 0; i < 8; i++) {
            if (i % 2 == 0) {
                ssd1306_set_pixel(left_eye_x - 8 + i, blush_y, false);
                ssd1306_set_pixel(right_eye_x - 8 + i, blush_y, false);
            }
        }
    }
    
    // Falling stars for birthday
    if (face.emotion == EMO_BIRTHDAY) {
        set_falling_stars_enabled(true);
        int blush_y = left_eye.y + 12;
        for (int i = 0; i < 8; i++) {
            if (i % 2 == 0) {
                ssd1306_set_pixel(left_eye_x - 8 + i, blush_y, false);
                ssd1306_set_pixel(right_eye_x - 8 + i, blush_y, false);
            }
        }
    } else {
        set_falling_stars_enabled(false);
    }

    // Floating hearts for love
    if (face.emotion == EMO_LOVE) {
        set_floating_hearts_enabled(true);
    } else {
        set_floating_hearts_enabled(false);
    }
    
    draw_falling_stars_overlay();
    draw_floating_hearts_overlay();

    // Birthday cake and text
    if (face.emotion == EMO_BIRTHDAY) {
        draw_birthday_cake();
        draw_birthday_text();
    }

    ssd1306_update();
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

void app_main(void)
{
    ESP_LOGI(TAG, "=== Desktoy (Hybrid 2D/3D Rendering) ===");
    
    esp_err_t ret = ssd1306_init(I2C_SDA_PIN, I2C_SCL_PIN, OLED_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed!");
        return;
    }
    
    // Splash screen
    ESP_LOGI(TAG, "Showing splash screen...");
    draw_splash_screen("KRG");
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Initialize buzzer
    ret = buzzer_init(BUZZER_PIN);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Buzzer init failed - continuing without sound");
    } else {
        ESP_LOGI(TAG, "Buzzer initialized on GPIO%d", BUZZER_PIN);
        buzzer_set_volume(60);
    }
    
    // Initialize 3D scene
    init_3d_scene();
    ESP_LOGI(TAG, "3D renderer initialized");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    
    // Initialize face timers
    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    face.next_blink = start + 3000;
    face.next_look = start + 2000;
    face.next_emotion = start + 3000;  // Start emotion changes after 3 seconds
    face.next_crazy_look = start + 1000;  // Start crazy eye changes after 1 second
    face.crazy_left_look_x = 0;
    face.crazy_left_look_y = 0;
    face.crazy_right_look_x = 0;
    face.crazy_right_look_y = 0;
    face.crazy_left_target_x = 0;
    face.crazy_left_target_y = 0;
    face.crazy_right_target_x = 0;
    face.crazy_right_target_y = 0;

    // Start with trollface emotion for testing
    apply_emotion(EMO_TROLLFACE);
    ESP_LOGI(TAG, "Starting with trollface emotion");
    
    ESP_LOGI(TAG, "Starting animation...");
    
    uint32_t frame_count = 0;
    uint32_t last_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t delta_ms = now - last_time;
        last_time = now;
        
        update_3d_face(now);
        draw_3d_face();
        
        buzzer_update(delta_ms);
        
        frame_count++;
        if (frame_count % 100 == 0) {
            ESP_LOGI(TAG, "Frame %lu", (unsigned long)frame_count);
        }
        
        vTaskDelay(pdMS_TO_TICKS(8));
    }
}
