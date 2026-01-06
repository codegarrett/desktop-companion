/*
 * Desktoy - Expressive Anime Face on OLED Display
 * ESP32-C3 Super Mini + GME12864-17 (SSD1306 128x64 OLED)
 * 
 * Rendering modes:
 *   0 = Procedural (dynamic drawing)
 *   1 = Sprite-based (customizable bitmap sprites)
 *   2 = 3D rendered (real-time 3D with dithered shading)
 */

#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "ssd1306.h"
#include "sprites.h"
#include "render3d.h"
#include "obj_loader.h"
#include "buzzer.h"

static const char *TAG = "desktoy";

// ============================================================================
// CONFIGURATION
// ============================================================================

// Rendering mode: 0 = Procedural, 1 = Sprite-based, 2 = 3D
#define RENDER_MODE  2

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
    EMO_BIRTHDAY,   // Happy Birthday special emotion!
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

// Helper functions (used by both modes)
static float lerp(float a, float b, float t) { return a + (b - a) * t; }

// ============================================================================
// PROCEDURAL RENDERING FUNCTIONS (only compiled in procedural mode)
// ============================================================================
#if RENDER_MODE == 0

static int clamp(int v, int min, int max) { return v < min ? min : (v > max ? max : v); }

// Drawing primitives
static void draw_ellipse(int cx, int cy, int rx, int ry, bool on) {
    if (rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; y++) {
        for (int x = -rx; x <= rx; x++) {
            float ex = (float)x / rx, ey = (float)y / ry;
            if (ex * ex + ey * ey <= 1.0f)
                ssd1306_set_pixel(cx + x, cy + y, on);
        }
    }
}

// Draw rounded rectangle (proper anime eye shape)
static void draw_rounded_rect(int x, int y, int w, int h, int r, bool on) {
    if (w <= 0 || h <= 0) return;
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;
    
    // Main body
    for (int j = y + r; j < y + h - r; j++) {
        for (int i = x; i < x + w; i++) {
            ssd1306_set_pixel(i, j, on);
        }
    }
    // Top and bottom strips
    for (int j = y; j < y + r; j++) {
        for (int i = x + r; i < x + w - r; i++) {
            ssd1306_set_pixel(i, j, on);
        }
    }
    for (int j = y + h - r; j < y + h; j++) {
        for (int i = x + r; i < x + w - r; i++) {
            ssd1306_set_pixel(i, j, on);
        }
    }
    // Rounded corners
    for (int cy_off = 0; cy_off < r; cy_off++) {
        for (int cx_off = 0; cx_off < r; cx_off++) {
            int dx = r - 1 - cx_off, dy = r - 1 - cy_off;
            if (dx*dx + dy*dy <= r*r) {
                ssd1306_set_pixel(x + cx_off, y + cy_off, on);                    // top-left
                ssd1306_set_pixel(x + w - 1 - cx_off, y + cy_off, on);            // top-right
                ssd1306_set_pixel(x + cx_off, y + h - 1 - cy_off, on);            // bottom-left
                ssd1306_set_pixel(x + w - 1 - cx_off, y + h - 1 - cy_off, on);    // bottom-right
            }
        }
    }
}

static void draw_thick_line(int x1, int y1, int x2, int y2, int thickness, bool on) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        for (int t = -thickness/2; t <= thickness/2; t++) {
            if (dx > dy) ssd1306_set_pixel(x1, y1 + t, on);
            else ssd1306_set_pixel(x1 + t, y1, on);
        }
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

static void draw_arc(int cx, int cy, int r, int start_deg, int end_deg, int thickness, bool on) {
    for (int deg = start_deg; deg <= end_deg; deg += 3) {
        float rad = deg * 3.14159f / 180.0f;
        int x = cx + (int)(r * cosf(rad));
        int y = cy + (int)(r * sinf(rad));
        ssd1306_fill_circle(x, y, thickness/2, on);
    }
}

// Draw heart shape
static void draw_heart(int cx, int cy, int size, bool on) {
    for (int y = -size; y <= size; y++) {
        for (int x = -size; x <= size; x++) {
            float fx = (float)x / size, fy = (float)y / size;
            // Heart equation
            float val = (fx*fx + fy*fy - 1);
            val = val * val * val - fx * fx * fy * fy * fy;
            if (val <= 0) ssd1306_set_pixel(cx + x, cy + y, on);
        }
    }
}

// Draw spiral (for crazy eyes)
static void draw_spiral(int cx, int cy, int size, bool on) {
    for (float t = 0; t < 12; t += 0.3f) {
        float r = t * size / 12;
        int x = cx + (int)(r * cosf(t));
        int y = cy + (int)(r * sinf(t));
        ssd1306_set_pixel(x, y, on);
        ssd1306_set_pixel(x+1, y, on);
    }
}

// Draw ZZZ for sleeping
static void draw_zzz(int x, int y) {
    // Z shape repeated
    for (int i = 0; i < 3; i++) {
        int ox = x + i * 8;
        int oy = y - i * 4;
        int sz = 4 - i;
        draw_thick_line(ox, oy, ox + sz*2, oy, 1, true);
        draw_thick_line(ox + sz*2, oy, ox, oy + sz, 1, true);
        draw_thick_line(ox, oy + sz, ox + sz*2, oy + sz, 1, true);
    }
}

// Draw expressive curved eyebrow (thin and dynamic)
static void draw_eyebrow(int cx, int cy, int width, float angle, bool is_left) {
    int half_w = width / 2;
    float ang_mult = angle * 0.5f; // More dramatic angle range
    
    // Eyebrow curves - inner side can go up or down based on angle
    int x_inner = is_left ? cx + half_w - 2 : cx - half_w + 2;
    int x_outer = is_left ? cx - half_w : cx + half_w;
    
    int y_inner = cy + (int)(ang_mult * 8 * (is_left ? 1 : -1));
    int y_outer = cy - (int)(ang_mult * 4 * (is_left ? 1 : -1));
    
    // Draw curved eyebrow using quadratic bezier-like curve
    int mid_x = (x_inner + x_outer) / 2;
    int curve_peak = -3; // Eyebrows curve upward naturally
    if (angle > 0.3f) curve_peak = 1;  // Angry = flatter
    if (angle < -0.3f) curve_peak = -5; // Worried = more curved
    
    for (int i = 0; i <= width; i++) {
        float t = (float)i / width;
        // Quadratic bezier: P = (1-t)²P0 + 2(1-t)tP1 + t²P2
        float t1 = 1.0f - t;
        int px = (int)(t1*t1*x_outer + 2*t1*t*(mid_x) + t*t*x_inner);
        int py_base = (int)(t1*t1*y_outer + 2*t1*t*(cy + curve_peak) + t*t*y_inner);
        
        // Draw thin line (2 pixels for visibility)
        ssd1306_set_pixel(px, py_base, true);
        ssd1306_set_pixel(px, py_base + 1, true);
    }
}

// Draw anime eye (proper rectangular anime style)
static void draw_anime_eye(int cx, int cy, int width, int height, float openness,
                           int look_x, int look_y, bool is_left, emotion_t emo) {
    int visible_h = (int)(height * openness);
    if (visible_h < 3) visible_h = 3;
    
    int top_y = cy - visible_h / 2;
    int left_x = cx - width / 2;
    int shake_x = (int)(face.shake * ((esp_random() % 5) - 2));
    cx += shake_x;
    left_x += shake_x;
    
    // Special eye types for certain emotions
    if (emo == EMO_LOVE && openness > 0.3f) {
        // Heart eyes
        draw_heart(cx, cy, width/3, true);
        // Still draw the thick upper lid
        draw_thick_line(left_x - 1, cy - height/3 - 2, left_x + width, cy - height/3 - 2, 3, true);
        return;
    }
    
    if (emo == EMO_CRAZY && openness > 0.3f) {
        // Spiral eyes with rounded rect background
        draw_rounded_rect(left_x, top_y, width, visible_h, 5, true);
        draw_spiral(cx, cy, width/3, false);
        draw_thick_line(left_x - 1, top_y - 2, left_x + width, top_y - 2, 3, true);
        return;
    }
    
    // Normal anime eye - ROUNDED RECTANGLE shape
    if (openness > 0.1f) {
        // Eye white - anime style rounded rectangle
        int corner_r = visible_h / 4;
        if (corner_r < 3) corner_r = 3;
        if (corner_r > 6) corner_r = 6;
        draw_rounded_rect(left_x, top_y, width, visible_h, corner_r, true);
        
        // Large iris/pupil (anime style = bigger)
        int iris_w = width / 3 + 2;
        int iris_h = (visible_h * 2) / 3;
        if (iris_h < 4) iris_h = 4;
        
        // Offset iris based on look direction
        int iris_x = cx + (look_x * (width/7)) / 10;
        int iris_y = cy + (look_y * (visible_h/6)) / 10;
        iris_x = clamp(iris_x, left_x + iris_w/2 + 2, left_x + width - iris_w/2 - 2);
        
        if (emo != EMO_SCARED) {
            // Draw dark iris (oval)
            draw_ellipse(iris_x, iris_y, iris_w, iris_h, false);
        } else {
            // Tiny pupils when scared
            ssd1306_fill_circle(iris_x, iris_y, 2, false);
        }
        
        // Anime highlights - signature sparkle look!
        // Main large highlight (upper left of iris)
        int h1_x = iris_x - iris_w/3 + (is_left ? -1 : 2);
        int h1_y = iris_y - iris_h/3;
        ssd1306_fill_circle(h1_x, h1_y, 3, true);
        
        // Secondary smaller highlight (lower right)
        int h2_x = iris_x + iris_w/3 + (is_left ? 0 : -1);
        int h2_y = iris_y + iris_h/4;
        ssd1306_set_pixel(h2_x, h2_y, true);
        ssd1306_set_pixel(h2_x + 1, h2_y, true);
        ssd1306_set_pixel(h2_x, h2_y + 1, true);
        ssd1306_set_pixel(h2_x + 1, h2_y + 1, true);
        
        // Happy squint effect - curved bottom
        if ((emo == EMO_HAPPY || emo == EMO_LAUGHING) && openness > 0.5f) {
            for (int i = 3; i < width - 3; i++) {
                int curve = (i * (width - i)) / (width * 2);
                int py = cy + visible_h/2 - 2 - curve;
                ssd1306_set_pixel(left_x + i, py, false);
                ssd1306_set_pixel(left_x + i, py + 1, false);
            }
        }
    }
    
    // Thick upper eyelid line (anime signature!)
    draw_thick_line(left_x - 2, top_y - 2, left_x + width + 1, top_y - 2, 3, true);
    
    // Eyelid coverage for blinking
    if (openness < 1.0f) {
        int cover = (int)((1.0f - openness) * height / 2) + 4;
        // Top eyelid coming down
        ssd1306_fill_rect(left_x - 3, cy - height/2 - 6, width + 6, cover + 4, false);
        // Bottom eyelid coming up
        ssd1306_fill_rect(left_x - 3, cy + height/2 - cover + 2, width + 6, cover + 4, false);
    }
}

// Draw mouth based on emotion
static void draw_mouth(int cx, int cy, emotion_t emo, float mouth_open, float curve, float width_mult) {
    (void)mouth_open; (void)curve; (void)width_mult; // May use later for dynamic mouths
    int bounce_y = (int)(face.bounce * 2);
    cy += bounce_y;
    
    switch (emo) {
        case EMO_NORMAL:
            draw_thick_line(cx - 5, cy, cx + 5, cy, 2, true);
            break;
            
        case EMO_HAPPY:
            // Curved smile
            draw_arc(cx, cy - 4, 8, 30, 150, 2, true);
            break;
            
        case EMO_LAUGHING:
            // Big open laugh - D shape
            draw_arc(cx, cy - 2, 10, 20, 160, 2, true);
            draw_thick_line(cx - 9, cy - 2, cx + 9, cy - 2, 2, true);
            // Teeth
            for (int i = -6; i <= 6; i += 4) {
                ssd1306_set_pixel(cx + i, cy, false);
                ssd1306_set_pixel(cx + i, cy + 1, false);
            }
            break;
            
        case EMO_ANGRY:
            // Gritted teeth / grimace
            draw_thick_line(cx - 10, cy, cx + 10, cy, 2, true);
            draw_thick_line(cx - 10, cy + 4, cx + 10, cy + 4, 2, true);
            // Vertical lines (teeth)
            for (int i = -8; i <= 8; i += 4) {
                draw_thick_line(cx + i, cy, cx + i, cy + 4, 1, true);
            }
            break;
            
        case EMO_SAD:
            // Frown
            draw_arc(cx, cy + 8, 8, 210, 330, 2, true);
            break;
            
        case EMO_SURPRISED:
            // Open O
            draw_ellipse(cx, cy + 2, 6, 8, true);
            draw_ellipse(cx, cy + 2, 4, 6, false);
            break;
            
        case EMO_SLEEPY:
            // Wavy tired mouth
            for (int i = -6; i <= 6; i++) {
                int wave = (int)(sinf(i * 0.8f) * 1.5f);
                ssd1306_set_pixel(cx + i, cy + wave, true);
                ssd1306_set_pixel(cx + i, cy + wave + 1, true);
            }
            break;
            
        case EMO_SLEEPING:
            // Small peaceful line
            draw_thick_line(cx - 4, cy, cx + 4, cy, 2, true);
            break;
            
        case EMO_CRAZY:
            // Wobbly wide grin
            for (int i = -12; i <= 12; i++) {
                int wave = (int)(sinf(i * 0.5f + face.shake * 3) * 3);
                ssd1306_set_pixel(cx + i, cy + wave, true);
                ssd1306_set_pixel(cx + i, cy + wave + 1, true);
            }
            break;
            
        case EMO_LOVE:
            // Cat smile :3
            ssd1306_fill_circle(cx - 4, cy, 2, true);
            ssd1306_fill_circle(cx + 4, cy, 2, true);
            draw_thick_line(cx - 4, cy, cx + 4, cy, 2, true);
            break;
            
        case EMO_WINK:
            // Playful smile
            draw_arc(cx, cy - 2, 7, 30, 150, 2, true);
            // Tongue
            ssd1306_fill_circle(cx + 5, cy + 3, 3, true);
            break;
            
        case EMO_SMUG:
            // Smirk (asymmetric)
            draw_arc(cx + 3, cy - 2, 6, 30, 120, 2, true);
            draw_thick_line(cx - 6, cy, cx, cy, 2, true);
            break;
            
        case EMO_SCARED:
            // Wavy scared mouth
            for (int i = -8; i <= 8; i++) {
                int wave = (int)(sinf(i * 1.2f) * 2);
                ssd1306_set_pixel(cx + i, cy + wave, true);
            }
            // Sweat drop
            draw_ellipse(cx + 20, cy - 15, 2, 4, true);
            break;
            
        default:
            draw_thick_line(cx - 5, cy, cx + 5, cy, 2, true);
    }
}

#endif // RENDER_MODE == 0 (end of procedural drawing primitives)

// Track last played emotion to avoid replaying sounds
static emotion_t last_sound_emotion = EMO_COUNT;

// Draw splash screen with centered text
static void draw_splash_screen(const char *text) {
    ssd1306_fill();  // White background
    
    // Calculate text dimensions (assume 6x8 font, we'll draw big blocky letters)
    int text_len = strlen(text);
    int char_w = 16;  // Big block letter width
    int char_h = 24;  // Big block letter height
    int total_w = text_len * char_w + (text_len - 1) * 4;  // Include spacing
    int start_x = (SCREEN_WIDTH - total_w) / 2;
    int start_y = (SCREEN_HEIGHT - char_h) / 2;
    
    // Draw each character as big blocky letters
    for (int i = 0; i < text_len; i++) {
        int cx = start_x + i * (char_w + 4);
        char c = text[i];
        
        // Draw big block letters (simple 3-segment style)
        if (c == 'K') {
            // Vertical bar
            for (int y = 0; y < char_h; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            // Diagonal top (going right-up)
            for (int d = 0; d < char_h/2; d++) {
                for (int t = 0; t < 4; t++) {
                    ssd1306_set_pixel(cx + 4 + d * 2/3 + t, start_y + char_h/2 - d, false);
                }
            }
            // Diagonal bottom (going right-down)
            for (int d = 0; d < char_h/2; d++) {
                for (int t = 0; t < 4; t++) {
                    ssd1306_set_pixel(cx + 4 + d * 2/3 + t, start_y + char_h/2 + d, false);
                }
            }
        } else if (c == 'R') {
            // Vertical bar
            for (int y = 0; y < char_h; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            // Top horizontal
            for (int x = 0; x < char_w - 2; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            // Middle horizontal
            for (int x = 0; x < char_w - 4; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + char_h/2 - 2 + y, false);
                }
            }
            // Right vertical top half
            for (int y = 0; y < char_h/2; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + char_w - 4 + x, start_y + y, false);
                }
            }
            // Diagonal leg
            for (int d = 0; d < char_h/2; d++) {
                for (int t = 0; t < 4; t++) {
                    ssd1306_set_pixel(cx + 4 + d * 2/3 + t, start_y + char_h/2 + d, false);
                }
            }
        } else if (c == 'G') {
            // Top horizontal
            for (int x = 2; x < char_w; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            // Bottom horizontal
            for (int x = 2; x < char_w; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + char_h - 4 + y, false);
                }
            }
            // Left vertical
            for (int y = 0; y < char_h; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + x, start_y + y, false);
                }
            }
            // Right vertical bottom half
            for (int y = char_h/2; y < char_h; y++) {
                for (int x = 0; x < 4; x++) {
                    ssd1306_set_pixel(cx + char_w - 4 + x, start_y + y, false);
                }
            }
            // Middle horizontal (the G crossbar)
            for (int x = char_w/2; x < char_w; x++) {
                for (int y = 0; y < 4; y++) {
                    ssd1306_set_pixel(cx + x, start_y + char_h/2 - 2 + y, false);
                }
            }
        }
    }
    
    ssd1306_update();
}

// Set expression parameters based on emotion (used by both modes)
// play_sound: set to false during blink recovery to avoid restarting sounds
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
    
    switch (emo) {
        case EMO_NORMAL:
            face.left_brow_angle = -0.1f;  // Slight natural curve
            face.right_brow_angle = -0.1f;
            break;
            
        case EMO_HAPPY:
            face.left_brow_height = -3;
            face.right_brow_height = -3;
            face.left_brow_angle = -0.3f;  // Raised, curved
            face.right_brow_angle = -0.3f;
            break;
            
        case EMO_LAUGHING:
            face.target_left_eye = 0.35f;
            face.target_right_eye = 0.35f;
            face.left_brow_height = -4;
            face.right_brow_height = -4;
            face.left_brow_angle = -0.4f;
            face.right_brow_angle = -0.4f;
            break;
            
        case EMO_ANGRY:
            face.left_brow_angle = 1.0f;   // Very angled inward/down
            face.right_brow_angle = 1.0f;
            face.left_brow_height = 3;
            face.right_brow_height = 3;
            break;
            
        case EMO_SAD:
            face.left_brow_angle = -0.9f;  // Inner side raised (worried look)
            face.right_brow_angle = -0.9f;
            face.left_brow_height = -1;
            face.right_brow_height = -1;
            face.target_left_eye = 0.65f;
            face.target_right_eye = 0.65f;
            break;
            
        case EMO_SURPRISED:
            face.left_brow_height = -7;    // Very high
            face.right_brow_height = -7;
            face.left_brow_angle = -0.5f;  // Arched
            face.right_brow_angle = -0.5f;
            break;
            
        case EMO_SLEEPY:
            face.target_left_eye = 0.25f;
            face.target_right_eye = 0.25f;
            face.left_brow_angle = -0.4f;
            face.right_brow_angle = -0.4f;
            face.left_brow_height = 2;
            face.right_brow_height = 2;
            break;
            
        case EMO_SLEEPING:
            face.target_left_eye = 0.0f;
            face.target_right_eye = 0.0f;
            face.left_brow_angle = -0.2f;
            face.right_brow_angle = -0.2f;
            break;
            
        case EMO_CRAZY:
            face.left_brow_height = -5;
            face.right_brow_height = 3;
            face.left_brow_angle = -0.8f;  // Asymmetric chaos
            face.right_brow_angle = 0.9f;
            face.shake = 1.0f;
            break;
            
        case EMO_LOVE:
            face.left_brow_height = -3;
            face.right_brow_height = -3;
            face.left_brow_angle = -0.4f;
            face.right_brow_angle = -0.4f;
            break;
            
        case EMO_WINK:
            face.target_left_eye = 1.0f;
            face.target_right_eye = 0.0f;
            face.left_brow_height = -3;
            face.right_brow_height = 1;
            face.left_brow_angle = -0.3f;
            face.right_brow_angle = 0.2f;
            break;
            
        case EMO_SMUG:
            face.target_left_eye = 0.6f;
            face.target_right_eye = 0.9f;
            face.left_brow_height = 2;
            face.right_brow_height = -4;
            face.left_brow_angle = 0.4f;   // One skeptical
            face.right_brow_angle = -0.5f; // One raised
            break;
            
        case EMO_SCARED:
            face.left_brow_angle = -1.0f;  // Maximum worry
            face.right_brow_angle = -1.0f;
            face.left_brow_height = -5;
            face.right_brow_height = -5;
            face.shake = 0;  // No random shake - use smooth animation instead
            face.anim_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            break;
            
        case EMO_BIRTHDAY:
            // Happy celebration face!
            face.left_brow_height = -4;
            face.right_brow_height = -4;
            face.left_brow_angle = -0.4f;
            face.right_brow_angle = -0.4f;
            face.target_left_eye = 0.4f;  // Happy squint
            face.target_right_eye = 0.4f;
            break;
            
        default:
            break;
    }
    
    // Play sound effect for this emotion (only if requested and emotion changed)
    if (play_sound && emo != last_sound_emotion) {
        last_sound_emotion = emo;
        
        sound_effect_t sfx = SFX_NONE;
        switch (emo) {
            case EMO_NORMAL:    sfx = SFX_NONE; break;  // No sound for normal
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
            default: break;
        }
        if (sfx != SFX_NONE) {
            buzzer_play_sfx(sfx);
        }
    }
}

// Wrapper: apply emotion with sound
static void apply_emotion(emotion_t emo) {
    apply_emotion_internal(emo, true);
}

// Wrapper: apply emotion without sound (for blink recovery)
static void apply_emotion_silent(emotion_t emo) {
    apply_emotion_internal(emo, false);
}

// Draw complete face (procedural mode only)
#if RENDER_MODE == 0
static void draw_face(void) {
    ssd1306_clear();
    
    int bounce_y = (int)(face.bounce * 3);
    
    // Eye positions
    int left_ex = 34, right_ex = 94;
    int eye_y = 26 + bounce_y;
    int eye_w = 28, eye_h = 24;
    
    // Draw eyebrows
    int brow_y = eye_y - eye_h/2 - 8;
    draw_eyebrow(left_ex, brow_y + (int)face.left_brow_height, 20, 
                 face.left_brow_angle, true);
    draw_eyebrow(right_ex, brow_y + (int)face.right_brow_height, 20,
                 face.right_brow_angle, false);
    
    // Draw eyes
    draw_anime_eye(left_ex, eye_y, eye_w, eye_h, face.left_eye_open,
                   face.look_x, face.look_y, true, face.emotion);
    draw_anime_eye(right_ex, eye_y, eye_w, eye_h, face.right_eye_open,
                   face.look_x, face.look_y, false, face.emotion);
    
    // Draw mouth
    draw_mouth(64, 54 + bounce_y, face.emotion, face.mouth_open, 
               face.mouth_curve, face.mouth_width);
    
    // Special effects
    if (face.emotion == EMO_SLEEPING) {
        draw_zzz(85, 15);
    }
    
    ssd1306_update();
}
#endif // RENDER_MODE == 0

// Update animation state
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
    
    // Blinking (skip during sleep or wink)
    if (face.emotion != EMO_SLEEPING && face.emotion != EMO_WINK) {
        if (now >= face.next_blink) {
            if (face.target_left_eye > 0.5f && face.target_right_eye > 0.5f) {
                // Start blink
                face.target_left_eye = 0;
                face.target_right_eye = 0;
                face.next_blink = now + 100;
            } else if (face.emotion != EMO_SLEEPY && face.emotion != EMO_SAD) {
                // End blink - restore eye targets (silent - don't replay sound)
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
    
    // Emotion changes
    if (now >= face.next_emotion) {
        // Weighted random emotion selection
        int r = esp_random() % 100;
        emotion_t new_emo;
        
        if (r < 25) new_emo = EMO_NORMAL;
        else if (r < 45) new_emo = EMO_HAPPY;
        else if (r < 55) new_emo = EMO_LAUGHING;
        else if (r < 62) new_emo = EMO_SURPRISED;
        else if (r < 68) new_emo = EMO_WINK;
        else if (r < 74) new_emo = EMO_SMUG;
        else if (r < 79) new_emo = EMO_LOVE;
        else if (r < 84) new_emo = EMO_SLEEPY;
        else if (r < 88) new_emo = EMO_SLEEPING;
        else if (r < 92) new_emo = EMO_SAD;
        else if (r < 95) new_emo = EMO_ANGRY;
        else if (r < 98) new_emo = EMO_SCARED;
        else new_emo = EMO_CRAZY;
        
        apply_emotion(new_emo);
        
        // Duration varies by emotion
        int duration = 3000 + (esp_random() % 5000);
        if (new_emo == EMO_SLEEPING) duration = 5000 + (esp_random() % 3000);
        if (new_emo == EMO_CRAZY) duration = 1500 + (esp_random() % 1500);
        
        face.next_emotion = now + duration;
        face.anim_start = now;
        
        ESP_LOGI(TAG, "Emotion: %d", new_emo);
    }
}

// ============================================================================
// SPRITE MODE - State and Animation
// ============================================================================
#if RENDER_MODE == 1

// Map procedural emotions to sprite expressions
static sprite_expression_t emotion_to_sprite_expr(emotion_t emo) {
    switch (emo) {
        case EMO_NORMAL:    return SPRITE_EXPR_NORMAL;
        case EMO_HAPPY:     return SPRITE_EXPR_HAPPY;
        case EMO_LAUGHING:  return SPRITE_EXPR_LAUGHING;
        case EMO_ANGRY:     return SPRITE_EXPR_ANGRY;
        case EMO_SAD:       return SPRITE_EXPR_SAD;
        case EMO_SURPRISED: return SPRITE_EXPR_SURPRISED;
        case EMO_SLEEPY:    return SPRITE_EXPR_SLEEPY;
        case EMO_SLEEPING:  return SPRITE_EXPR_SLEEPING;
        case EMO_LOVE:      return SPRITE_EXPR_LOVE;
        case EMO_WINK:      return SPRITE_EXPR_WINK;
        default:            return SPRITE_EXPR_NORMAL;
    }
}

static void draw_face_sprite(const character_pack_t *character) {
    ssd1306_clear();
    
    sprite_expression_t expr = emotion_to_sprite_expr(face.emotion);
    const face_sprite_set_t *face_set = &character->expressions[expr];
    
    int bounce_y = (int)(face.bounce * 3);
    float blink = 1.0f - face.left_eye_open; // Invert for blink amount
    
    sprite_draw_face_animated(face_set, blink, face.look_x / 2, face.look_y / 2, bounce_y);
    
    // Draw ZZZ for sleeping
    if (face.emotion == EMO_SLEEPING) {
        // Simple ZZZ
        for (int i = 0; i < 3; i++) {
            int x = 90 + i * 7;
            int y = 8 - i * 3;
            ssd1306_set_pixel(x, y, true);
            ssd1306_set_pixel(x+1, y, true);
            ssd1306_set_pixel(x+2, y, true);
            ssd1306_set_pixel(x+2, y+1, true);
            ssd1306_set_pixel(x+1, y+2, true);
            ssd1306_set_pixel(x, y+3, true);
            ssd1306_set_pixel(x, y+4, true);
            ssd1306_set_pixel(x+1, y+4, true);
            ssd1306_set_pixel(x+2, y+4, true);
        }
    }
    
    ssd1306_update();
}

#endif // RENDER_MODE == 1

// ============================================================================
// 3D RENDERING MODE
// ============================================================================
#if RENDER_MODE == 2

// Example OBJ data - a simple cube (can be replaced with downloaded models)
static const char *DEMO_OBJ_CUBE = 
    "# Simple cube\n"
    "v -0.5 -0.5  0.5\n"
    "v  0.5 -0.5  0.5\n"
    "v  0.5  0.5  0.5\n"
    "v -0.5  0.5  0.5\n"
    "v -0.5 -0.5 -0.5\n"
    "v  0.5 -0.5 -0.5\n"
    "v  0.5  0.5 -0.5\n"
    "v -0.5  0.5 -0.5\n"
    "f 1 2 3 4\n"    // Front
    "f 8 7 6 5\n"    // Back
    "f 4 3 7 8\n"    // Top
    "f 1 5 6 2\n"    // Bottom
    "f 1 4 8 5\n"    // Left
    "f 2 6 7 3\n";   // Right

static render_ctx_t render_ctx;
static mesh_t *head_mesh = NULL;
static float head_rotation = 0;
static float head_tilt = 0;

// Anime face parameters
typedef struct {
    int x, y;           // Screen position
    int look_x, look_y; // Pupil offset (-3 to 3)
    float blink;        // 0 = open, 1 = closed
} anime_eye_t;

static anime_eye_t left_eye, right_eye;

// Animation state for birthday cakes
static float cake_rotation = 0.0f;

static void init_3d_scene(void) {
    if (!render3d_init(&render_ctx, SCREEN_WIDTH, SCREEN_HEIGHT)) {
        ESP_LOGE(TAG, "Failed to init 3D renderer!");
        return;
    }
    
    // Camera setup
    camera_t cam = {
        .position = vec3_create(0, 0, 4),
        .target = vec3_create(0, 0, 0),
        .up = vec3_create(0, 1, 0),
        .fov = 50.0f,
        .near_plane = 0.1f,
        .far_plane = 100.0f
    };
    render3d_set_camera(&render_ctx, &cam);
    
    // Soft frontal lighting
    light_t light = {
        .direction = vec3_create(0.0f, -0.3f, -1.0f),
        .intensity = 0.6f,
        .ambient = 0.4f
    };
    render3d_set_light(&render_ctx, &light);
    
    // Create head mesh (slightly squashed sphere)
    head_mesh = mesh_create_sphere(1.0f, 8);
    if (head_mesh) {
        mesh_set_scale(head_mesh, 1.0f, 1.1f, 0.9f); // Taller, thinner
        ESP_LOGI(TAG, "Created 3D head mesh");
    }
    
    // Initialize eye positions (will be updated based on head rotation)
    // Eyes spread apart more for bigger eye size
    left_eye = (anime_eye_t){.x = 32, .y = 26, .look_x = 0, .look_y = 0, .blink = 0};
    right_eye = (anime_eye_t){.x = 96, .y = 26, .look_x = 0, .look_y = 0, .blink = 0};
}

// Draw a heart shape for love eyes (shaded with beating animation)
static void draw_heart_2d(int cx, int cy, int base_size) {
    // Animated beating effect - smooth pulse
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    float beat_phase = (float)(now % 600) / 600.0f;  // 600ms = ~100 BPM heartbeat
    float beat;
    if (beat_phase < 0.15f) {
        // Quick expand
        beat = beat_phase / 0.15f;
    } else if (beat_phase < 0.3f) {
        // Quick contract
        beat = 1.0f - (beat_phase - 0.15f) / 0.15f;
    } else {
        // Rest
        beat = 0;
    }
    float size = base_size + beat * 3.0f;  // Pulse by 3 pixels
    
    // Highlight position shifts with beat (moves slightly during pulse)
    float highlight_x = -0.3f - beat * 0.1f;
    float highlight_y = -0.4f - beat * 0.1f;
    
    // Fill heart with shading using implicit equation
    int isize = (int)size + 2;
    for (int py = -isize; py <= isize; py++) {
        for (int px = -isize; px <= isize; px++) {
            // Normalize coordinates
            float nx = (float)px / size;
            float ny = -(float)py / size;  // Flip Y for correct orientation
            
            // Heart implicit equation: (x² + y² - 1)³ - x²y³ < 0 means inside
            float x2 = nx * nx;
            float y2 = ny * ny;
            float y3 = ny * ny * ny;
            float inner = x2 + y2 - 1.0f;
            float heart_val = inner * inner * inner - x2 * y3;
            
            if (heart_val < 0) {
                // Inside heart - calculate shading
                // Distance from highlight center (upper-left area)
                float dx = nx - highlight_x;
                float dy = ny - highlight_y;
                float highlight_dist = sqrtf(dx*dx + dy*dy);
                
                // Shading: closer to highlight = lighter (white), further = darker (black)
                // Also factor in depth (center of heart is "deeper")
                float shade = highlight_dist * 1.2f;
                
                // Add edge darkening for more 3D effect
                float edge_dist = -heart_val;  // How far inside the heart
                if (edge_dist < 0.1f) {
                    shade += 0.3f;  // Darken near edges
                }
                
                // Dithered shading based on shade value
                bool pixel_on;
                if (shade < 0.3f) {
                    // Highlight - mostly white
                    pixel_on = true;
                } else if (shade < 0.5f) {
                    // Light dither
                    pixel_on = ((px + py) % 2 == 0);
                } else if (shade < 0.7f) {
                    // Medium dither
                    pixel_on = ((px + py) % 3 == 0);
                } else if (shade < 0.9f) {
                    // Dark dither
                    pixel_on = ((px % 2 == 0) && (py % 2 == 0));
                } else {
                    // Darkest - mostly black
                    pixel_on = false;
                }
                
                ssd1306_set_pixel(cx + px, cy + py, pixel_on);
            }
        }
    }
    
    // Draw solid outline for definition
    for (float t = 0; t < 6.28f; t += 0.06f) {
        float x = 16.0f * sinf(t) * sinf(t) * sinf(t);
        float y = 13.0f * cosf(t) - 5.0f * cosf(2*t) - 2.0f * cosf(3*t) - cosf(4*t);
        
        int px = cx + (int)(x * size / 16.0f);
        int py = cy - (int)(y * size / 17.0f);
        
        ssd1306_set_pixel(px, py, false);
    }
}

// Draw spiral for crazy eyes
static void draw_spiral_2d(int cx, int cy, int size) {
    for (float a = 0; a < 12; a += 0.3f) {
        float r = a * size / 12.0f;
        int x = cx + (int)(cosf(a) * r);
        int y = cy + (int)(sinf(a) * r);
        ssd1306_set_pixel(x, y, false);
        ssd1306_set_pixel(x+1, y, false);
    }
}

// ============================================================================
// FALLING STARS OVERLAY (reusable animation for any emotion)
// ============================================================================

#define MAX_STARS 8

typedef struct {
    float x, y;           // Position
    float speed;          // Fall speed
    float rotation;       // Current rotation angle
    float rot_speed;      // Rotation speed
    int size;             // Star size (2-5)
    bool active;
} falling_star_t;

static falling_star_t stars[MAX_STARS];
static bool stars_initialized = false;
static bool stars_enabled = false;

static void init_falling_stars(void) {
    for (int i = 0; i < MAX_STARS; i++) {
        stars[i].x = esp_random() % SCREEN_WIDTH;
        stars[i].y = -(int)(esp_random() % 30);  // Start above screen
        stars[i].speed = 0.8f + (esp_random() % 100) / 80.0f;
        stars[i].rotation = (esp_random() % 628) / 100.0f;  // 0 to 2π
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
        
        // Reset star when it falls off screen
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
    // Draw a 4-pointed star that rotates
    for (int i = 0; i < 4; i++) {
        float angle = rotation + (i * M_PI / 2);
        int x1 = cx + (int)(cosf(angle) * size);
        int y1 = cy + (int)(sinf(angle) * size);
        
        // Draw line from center to point
        int steps = size;
        for (int s = 0; s <= steps; s++) {
            int px = cx + (x1 - cx) * s / steps;
            int py = cy + (y1 - cy) * s / steps;
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                ssd1306_set_pixel(px, py, false);
            }
        }
    }
    // Center dot
    if (cx >= 0 && cx < SCREEN_WIDTH && cy >= 0 && cy < SCREEN_HEIGHT) {
        ssd1306_set_pixel(cx, cy, false);
    }
}

// Call this to enable/disable falling stars overlay
static void set_falling_stars_enabled(bool enabled) {
    stars_enabled = enabled;
    if (enabled && !stars_initialized) {
        init_falling_stars();
    }
}

// Draw the falling stars overlay (call after main face drawing)
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
// BIRTHDAY CAKE (matching reference: solid with frosting drips and layers)
// ============================================================================

static void draw_cake_3d_style(int cx, int cy, int size) {
    int w = size;           // Half width
    int h = size + 2;       // Height of cake body
    int top_y = cy - h/3;   // Top of cake body
    int bot_y = cy + h*2/3; // Bottom of cake
    
    // Rounded top (semi-ellipse filled) - the frosting dome
    int curve_h = size / 3;
    for (int x = -w; x <= w; x++) {
        float t = (float)x / w;
        int curve = (int)(curve_h * sqrtf(fmaxf(0, 1 - t*t)));
        for (int y = top_y - curve; y <= top_y; y++) {
            ssd1306_set_pixel(cx + x, y, false);
        }
    }
    
    // Solid cake body
    for (int y = top_y; y <= bot_y; y++) {
        for (int x = -w; x <= w; x++) {
            ssd1306_set_pixel(cx + x, y, false);
        }
    }
    
    // Frosting drips (white drips hanging down from top)
    int drips[][2] = {{-w+1, 5}, {-w/2, 7}, {-2, 4}, {3, 6}, {w-1, 5}};
    for (int i = 0; i < 5; i++) {
        int dx = drips[i][0];
        int dlen = drips[i][1] * size / 14;
        for (int dy = 0; dy < dlen; dy++) {
            // Tapered drip
            int drip_w = (dy < dlen * 2/3) ? 1 : 0;
            for (int ddx = -drip_w; ddx <= drip_w; ddx++) {
                ssd1306_set_pixel(cx + dx + ddx, top_y + dy, true);
            }
        }
        // Round drip end
        ssd1306_set_pixel(cx + dx, top_y + dlen, true);
    }
    
    // Horizontal layer lines (white stripes)
    int line_y1 = cy + 1;
    int line_y2 = cy + 5;
    for (int x = -w + 1; x <= w - 1; x++) {
        ssd1306_set_pixel(cx + x, line_y1, true);
        ssd1306_set_pixel(cx + x, line_y2, true);
    }
    
    // Candle (on top of frosting dome)
    int candle_w = 1;
    int candle_h = size / 2 + 1;
    int candle_base = top_y - curve_h;
    for (int y = 0; y < candle_h; y++) {
        for (int x = -candle_w; x <= candle_w; x++) {
            ssd1306_set_pixel(cx + x, candle_base - y, false);
        }
    }
    
    // Flame (teardrop pointing up)
    int flame_base = candle_base - candle_h;
    int flame_h = size / 3 + 2;
    for (int y = 0; y < flame_h; y++) {
        float t = (float)y / flame_h;
        // Teardrop: wide at bottom (y=0), narrow at top
        int fw = (int)((1 - t) * (candle_w + 2));
        for (int x = -fw; x <= fw; x++) {
            ssd1306_set_pixel(cx + x, flame_base - y, false);
        }
    }
    // Pointed tip
    ssd1306_set_pixel(cx, flame_base - flame_h, false);
}

// Draw cute anime-style eye with emotion support
static void draw_anime_eye_2d(int cx, int cy, int look_x, int look_y, float openness, 
                               bool is_left, emotion_t emo) {
    int eye_w = 40;   // Larger outer eye to contain iris/pupil movement
    int eye_h = 34;
    int half_w = eye_w / 2;
    int half_h = eye_h / 2;
    
    // Apply shake for certain emotions
    if (face.shake > 0) {
        cx += (int)(face.shake * ((esp_random() % 5) - 2));
    }
    
    // Special eyes for certain emotions
    if (emo == EMO_LOVE && openness > 0.3f) {
        draw_heart_2d(cx, cy, 18);  // Big hearts, similar to eye size
        return;
    }
    
    if (emo == EMO_CRAZY && openness > 0.3f) {
        draw_spiral_2d(cx, cy, 10);
        return;
    }
    
    if (emo == EMO_BIRTHDAY && openness > 0.3f) {
        draw_cake_3d_style(cx, cy, 10);
        return;
    }
    
    // Closed/blinking eye
    if (openness < 0.2f) {
        // Closed eye - curved smile line
        for (int x = -half_w; x <= half_w; x++) {
            float t = (float)x / (float)half_w;
            int curve = (emo == EMO_HAPPY || emo == EMO_LAUGHING) ? 4 : 2;
            int y = (int)(t * t * curve);
            ssd1306_set_pixel(cx + x, cy + y, false);
            ssd1306_set_pixel(cx + x, cy + y + 1, false);
        }
        return;
    }
    
    // Sleepy/half-closed eyes
    int visible_h = (int)(eye_h * openness);
    if (visible_h < 8) visible_h = 8;
    int adj_half_h = visible_h / 2;
    
    // Surprised - wider eyes
    if (emo == EMO_SURPRISED) {
        adj_half_h = half_h + 3;
    }
    
    // Draw eye outline
    // Top edge - thick curved line (upper eyelid)
    for (int x = -half_w; x <= half_w; x++) {
        float t = (float)x / (float)half_w;
        int y = -adj_half_h + (int)(fabsf(t) * 3);
        ssd1306_set_pixel(cx + x, cy + y, false);
        ssd1306_set_pixel(cx + x, cy + y + 1, false);
        ssd1306_set_pixel(cx + x, cy + y + 2, false);
    }
    
    // Bottom edge
    for (int x = -half_w + 2; x <= half_w - 2; x++) {
        float t = (float)x / (float)half_w;
        int y = adj_half_h - 2 - (int)(fabsf(t) * 2);
        ssd1306_set_pixel(cx + x, cy + y, false);
    }
    
    // Side edges
    for (int y = -adj_half_h + 3; y < adj_half_h - 3; y++) {
        float t = (float)(y + adj_half_h) / (float)(adj_half_h * 2);
        int left_x = -half_w + (int)(t * 5);
        int right_x = half_w - (int)(t * 5);
        ssd1306_set_pixel(cx + left_x, cy + y, false);
        ssd1306_set_pixel(cx + right_x, cy + y, false);
    }
    
    // Iris with gradient
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
    
    // Pupil
    int pupil_w = (emo == EMO_SURPRISED) ? 2 : 4;  // Small pupils when surprised
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
    
    // Highlights
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
    
    // Wink - close one eye
    if (emo == EMO_WINK && !is_left) {
        // Redraw right eye as wink (curved line)
        ssd1306_fill_rect(cx - half_w - 2, cy - half_h - 2, eye_w + 4, eye_h + 4, true);
        for (int x = -half_w; x <= half_w; x++) {
            float t = (float)x / (float)half_w;
            int y = (int)(t * t * 4);
            ssd1306_set_pixel(cx + x, cy + y, false);
            ssd1306_set_pixel(cx + x, cy + y + 1, false);
        }
    }
}

// Draw thick curved eyebrow with emotion-based angle and height
static void draw_eyebrow_2d(int cx, int cy, bool is_left, float angle, float height_offset) {
    int brow_w = 16;
    int dir = is_left ? 1 : -1;
    
    // Apply height offset
    cy += (int)height_offset;
    
    // Thick curved arc eyebrow with angle
    for (int i = 0; i < brow_w; i++) {
        float t = (float)i / (float)brow_w;
        int x = cx + (i - brow_w/2) * dir;
        
        // Arc shape - higher in middle, with angle tilt
        float base_curve = sinf(t * 3.14159f) * 4.0f;
        float tilt = angle * (t - 0.5f) * 8.0f * dir;  // Inner vs outer tilt
        int y = cy - (int)(base_curve + tilt);
        
        // Make it thick (3-4 pixels tall)
        ssd1306_set_pixel(x, y, false);
        ssd1306_set_pixel(x, y + 1, false);
        ssd1306_set_pixel(x, y + 2, false);
        if (t > 0.2f && t < 0.8f) {
            ssd1306_set_pixel(x, y + 3, false);
        }
    }
}

// Draw mouth based on emotion
static void draw_mouth_2d(int cx, int cy, emotion_t emo) {
    int bounce_y = (int)(face.bounce * 2);
    cy += bounce_y;
    
    switch (emo) {
        case EMO_NORMAL:
            // Simple line
            for (int x = -5; x <= 5; x++) {
                ssd1306_set_pixel(cx + x, cy, false);
            }
            break;
            
        case EMO_HAPPY:
            // Curved smile (U shape - edges curve UP)
            for (int x = -8; x <= 8; x++) {
                int y = -(x * x) / 16 + 4;  // Negative parabola = smile
                ssd1306_set_pixel(cx + x, cy + y, false);
                ssd1306_set_pixel(cx + x, cy + y + 1, false);
            }
            break;
            
        case EMO_LAUGHING:
            // Big open laugh - D shape (wide smile curving up)
            for (int x = -10; x <= 10; x++) {
                int y = -(x * x) / 20 + 5;  // Bottom curves up
                ssd1306_set_pixel(cx + x, cy + y, false);
                ssd1306_set_pixel(cx + x, cy + y + 1, false);
            }
            // Top line (straight)
            for (int x = -9; x <= 9; x++) {
                ssd1306_set_pixel(cx + x, cy - 1, false);
            }
            break;
            
        case EMO_SAD:
            // Frown - upside down curve
            for (int x = -6; x <= 6; x++) {
                int y = -(x * x) / 18;
                ssd1306_set_pixel(cx + x, cy + y + 3, false);
            }
            break;
            
        case EMO_ANGRY:
            // Gritted teeth / tight frown
            for (int x = -7; x <= 7; x++) {
                int y = -(x * x) / 24;
                ssd1306_set_pixel(cx + x, cy + y + 2, false);
                ssd1306_set_pixel(cx + x, cy + y + 3, false);
            }
            break;
            
        case EMO_SURPRISED:
            // Open O mouth
            for (int angle = 0; angle < 360; angle += 10) {
                float rad = angle * 3.14159f / 180.0f;
                int x = (int)(cosf(rad) * 5);
                int y = (int)(sinf(rad) * 6);
                ssd1306_set_pixel(cx + x, cy + y, false);
            }
            break;
            
        case EMO_SLEEPY:
        case EMO_SLEEPING:
            // Small relaxed mouth
            for (int x = -3; x <= 3; x++) {
                ssd1306_set_pixel(cx + x, cy, false);
            }
            break;
            
        case EMO_LOVE:
            // Small happy smile (U shape)
            for (int x = -6; x <= 6; x++) {
                int y = -(x * x) / 18 + 2;  // Curves up
                ssd1306_set_pixel(cx + x, cy + y, false);
            }
            break;
            
        case EMO_WINK:
        case EMO_SMUG:
            // Smirk - asymmetric smile (one side higher)
            for (int x = -6; x <= 6; x++) {
                int y = -(x * x) / 18 + x / 4 + 2;  // Curves up with tilt
                ssd1306_set_pixel(cx + x, cy + y, false);
            }
            break;
            
        case EMO_SCARED:
            // Wavy scared mouth
            for (int x = -8; x <= 8; x++) {
                int y = (int)(sinf(x * 0.8f) * 2);
                ssd1306_set_pixel(cx + x, cy + y, false);
            }
            break;
            
        case EMO_CRAZY:
            // Big crazy grin (wide U shape)
            for (int x = -10; x <= 10; x++) {
                int y = -(x * x) / 20 + 5;  // Bottom curves up
                ssd1306_set_pixel(cx + x, cy + y, false);
            }
            // Top line
            for (int x = -8; x <= 8; x++) {
                ssd1306_set_pixel(cx + x, cy, false);
            }
            break;
            
        case EMO_BIRTHDAY:
            // Big excited open smile! (wide happy U)
            for (int x = -10; x <= 10; x++) {
                int y = -(x * x) / 18 + 6;  // Bottom curves up nicely
                ssd1306_set_pixel(cx + x, cy + y, false);
                ssd1306_set_pixel(cx + x, cy + y + 1, false);
            }
            // Top of mouth (straight line)
            for (int x = -9; x <= 9; x++) {
                ssd1306_set_pixel(cx + x, cy, false);
            }
            break;
            
        default:
            for (int x = -4; x <= 4; x++) {
                ssd1306_set_pixel(cx + x, cy, false);
            }
            break;
    }
}

static void update_3d_face(uint32_t now) {
    // Use the shared update_face function for blinking, emotions, etc.
    update_face(now);
    
    // Gentle head movement (add head bob from bounce)
    head_rotation = sinf(now * 0.0008f) * 15.0f + face.look_x * 2.0f;
    head_tilt = sinf(now * 0.0012f) * 5.0f + face.bounce * 5.0f;
    
    if (head_mesh) {
        mesh_set_rotation(head_mesh, head_tilt, head_rotation, face.shake * 10);
    }
    
    // Update eye positions based on head rotation
    int base_left_x = 32, base_right_x = 96;
    int base_y = 26;
    
    // Shift eyes with head rotation and bounce
    int rot_offset = (int)(head_rotation * 0.8f);
    int bounce_y = (int)(face.bounce * 3);
    
    left_eye.x = base_left_x + rot_offset;
    right_eye.x = base_right_x + rot_offset;
    left_eye.y = base_y + (int)(head_tilt * 0.3f) + bounce_y;
    right_eye.y = base_y + (int)(head_tilt * 0.3f) + bounce_y;
    
    // Use face state for blinking
    left_eye.blink = 1.0f - face.left_eye_open;
    right_eye.blink = 1.0f - face.right_eye_open;
    
    // Pupils follow look direction
    left_eye.look_x = face.look_x + (int)(head_rotation * 0.1f);
    left_eye.look_y = face.look_y;
    right_eye.look_x = face.look_x + (int)(head_rotation * 0.1f);
    right_eye.look_y = face.look_y;
}

// Draw ZZZ for sleeping emotion
static void draw_zzz_2d(int x, int y) {
    for (int i = 0; i < 3; i++) {
        int zx = x + i * 8;
        int zy = y - i * 4;
        int size = 4 + i;
        // Draw Z
        for (int j = 0; j < size; j++) {
            ssd1306_set_pixel(zx + j, zy, false);           // Top
            ssd1306_set_pixel(zx + size - j - 1, zy + j, false);  // Diagonal
            ssd1306_set_pixel(zx + j, zy + size - 1, false); // Bottom
        }
    }
}

static void draw_3d_face(void) {
    ssd1306_fill();  // White background
    
    // Calculate face center based on head rotation (between the two eyes)
    int face_cx = (left_eye.x + right_eye.x) / 2;
    int bounce_y = (int)(face.bounce * 3);
    
    // Scared animation: smooth wiggle and darting eyes
    int scared_wiggle_x = 0;
    int scared_look_x = 0;
    if (face.emotion == EMO_SCARED) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        float anim_time = (float)(now - face.anim_start) / 1000.0f;  // Seconds since scared started
        
        // Face wiggle: gentle side-to-side oscillation (2Hz)
        scared_wiggle_x = (int)(sinf(anim_time * 12.0f) * 2);
        
        // Eye darting: faster, more erratic looking left-right (3Hz with harmonics for nervousness)
        float dart = sinf(anim_time * 18.0f) + 0.3f * sinf(anim_time * 31.0f);
        scared_look_x = (int)(dart * 4);  // +/- 4 pixels
    }
    
    // Apply scared wiggle to eye positions
    int left_eye_x = left_eye.x + scared_wiggle_x;
    int right_eye_x = right_eye.x + scared_wiggle_x;
    
    // Eyebrows (positioned above eyes, with emotion-based angle/height)
    int brow_y = left_eye.y - 18 + bounce_y;
    draw_eyebrow_2d(left_eye_x, brow_y, true, face.left_brow_angle, face.left_brow_height);
    draw_eyebrow_2d(right_eye_x, brow_y, false, face.right_brow_angle, face.right_brow_height);
    
    // Eyes (with emotion support!)
    float left_openness = face.left_eye_open;
    float right_openness = face.right_eye_open;
    
    // Calculate look direction (normal + scared darting)
    int look_x = left_eye.look_x + scared_look_x;
    int look_y = left_eye.look_y;
    
    // For birthday: draw cute 3D-styled cakes as eyes
    if (face.emotion == EMO_BIRTHDAY && left_openness > 0.3f) {
        // Animate cake bobbing
        cake_rotation += 0.15f;
        int bob = (int)(sinf(cake_rotation) * 2);
        
        // Draw cute isometric cakes at eye positions
        draw_cake_3d_style(left_eye_x, left_eye.y + bob, 12);
        draw_cake_3d_style(right_eye_x, right_eye.y - bob, 12);  // Opposite bob
    } else {
        draw_anime_eye_2d(left_eye_x, left_eye.y, look_x, look_y, 
                          left_openness, true, face.emotion);
        draw_anime_eye_2d(right_eye_x, right_eye.y, look_x, look_y, 
                          right_openness, false, face.emotion);
    }
    
    // Mouth (with emotion!) - follows face wiggle
    int mouth_x = face_cx + scared_wiggle_x;
    int mouth_y = 54 + (int)(head_tilt * 0.3f) + bounce_y;
    draw_mouth_2d(mouth_x, mouth_y, face.emotion);
    
    // Special effects for certain emotions
    if (face.emotion == EMO_SLEEPING) {
        draw_zzz_2d(100, 12);
    }
    
    // Sweat drop for scared (follows the wiggle)
    if (face.emotion == EMO_SCARED) {
        int drop_x = right_eye_x + 22;
        int drop_y = right_eye.y - 10;
        // Animated drip effect
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
    
    // Blush for love/happy (follows face position)
    if (face.emotion == EMO_LOVE || face.emotion == EMO_HAPPY) {
        // Dithered blush marks under eyes
        int blush_y = left_eye.y + 12;
        for (int i = 0; i < 8; i++) {
            if (i % 2 == 0) {
                ssd1306_set_pixel(left_eye_x - 8 + i, blush_y, false);
                ssd1306_set_pixel(right_eye_x - 8 + i, blush_y, false);
            }
        }
    }
    
    // Falling stars for birthday (and any emotion with stars enabled)
    if (face.emotion == EMO_BIRTHDAY) {
        set_falling_stars_enabled(true);
        // Blush for birthday too
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
    
    // Draw falling stars overlay (if enabled)
    draw_falling_stars_overlay();
    
    ssd1306_update();
}

#endif // RENDER_MODE == 2

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

void app_main(void)
{
#if RENDER_MODE == 0
    ESP_LOGI(TAG, "=== Desktoy (Procedural Mode) ===");
#elif RENDER_MODE == 1
    ESP_LOGI(TAG, "=== Desktoy (Sprite Mode: %s) ===", 
             sprite_get_builtin_character()->name);
#elif RENDER_MODE == 2
    ESP_LOGI(TAG, "=== Desktoy (3D Rendering Mode) ===");
#endif
    
    esp_err_t ret = ssd1306_init(I2C_SDA_PIN, I2C_SCL_PIN, OLED_I2C_ADDR);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display init failed!");
        return;
    }
    
    // ========================================
    // SPLASH SCREEN
    // ========================================
    ESP_LOGI(TAG, "Showing splash screen...");
    draw_splash_screen("KRG");
    vTaskDelay(pdMS_TO_TICKS(1000));  // Show for 1 second
    
    // Initialize buzzer
    ret = buzzer_init(BUZZER_PIN);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Buzzer init failed - continuing without sound");
    } else {
        ESP_LOGI(TAG, "Buzzer initialized on GPIO%d", BUZZER_PIN);
        buzzer_set_volume(60);  // Set volume (0-100)
    }
    
#if RENDER_MODE == 2
    // Initialize 3D scene
    init_3d_scene();
    ESP_LOGI(TAG, "3D renderer initialized");
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    
    // Birthday song duration: approximately 10 seconds
    // (4 lines × ~2.5 seconds each)
    #define BIRTHDAY_SONG_DURATION_MS 11000
    
    // Initialize face timers
    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    face.next_blink = start + 3000;  // Delay first blink
    face.next_look = start + 2000;   // Delay first look
    face.next_emotion = start + BIRTHDAY_SONG_DURATION_MS;  // Wait for song to finish
    
    // Start with Birthday emotion!
    face.emotion = EMO_BIRTHDAY;
    face.left_brow_height = -4;
    face.right_brow_height = -4;
    face.left_brow_angle = -0.4f;
    face.right_brow_angle = -0.4f;
    face.target_left_eye = 0.4f;  // Happy squint
    face.target_right_eye = 0.4f;
    face.left_eye_open = 0.4f;
    face.right_eye_open = 0.4f;
    last_sound_emotion = EMO_BIRTHDAY;  // Mark as already playing
    buzzer_play_sfx(SFX_BIRTHDAY);  // Play birthday song
    ESP_LOGI(TAG, "Playing Happy Birthday! Duration: %d ms", BIRTHDAY_SONG_DURATION_MS);
#else
    uint32_t start = xTaskGetTickCount() * portTICK_PERIOD_MS;
    face.next_blink = start + 2000;
    face.next_look = start + 1000;
    face.next_emotion = start + 3000;
    
    #if RENDER_MODE == 1
    const character_pack_t *character = sprite_get_builtin_character();
    ESP_LOGI(TAG, "Loaded character: %s", character->name);
    #endif
#endif
    
    ESP_LOGI(TAG, "Starting animation...");
    
    uint32_t frame_count = 0;
    uint32_t last_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t delta_ms = now - last_time;
        last_time = now;
        
#if RENDER_MODE == 0
        // Procedural rendering
        update_face(now);
        draw_face();
#elif RENDER_MODE == 1
        // Sprite-based rendering
        update_face(now);
        draw_face_sprite(character);
#elif RENDER_MODE == 2
        // 3D rendering
        update_3d_face(now);
        draw_3d_face();
#endif
        
        // Update buzzer for portamento effects
        buzzer_update(delta_ms);
        
        frame_count++;
        if (frame_count % 100 == 0) {
            ESP_LOGI(TAG, "Frame %lu", (unsigned long)frame_count);
        }
        
        vTaskDelay(pdMS_TO_TICKS(8)); // ~120 FPS
    }
}
