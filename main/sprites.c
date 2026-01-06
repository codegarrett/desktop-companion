/*
 * Sprite System Implementation
 * Contains sprite rendering and built-in "Mochi" character with big cute anime eyes
 */

#include "sprites.h"
#include "ssd1306.h"

// ============================================================================
// SPRITE RENDERING
// ============================================================================

void sprite_draw(const sprite_t *sprite, int x, int y, bool invert) {
    if (!sprite || !sprite->data) return;
    
    int bytes_per_row = (sprite->width + 7) / 8;
    
    for (int row = 0; row < sprite->height; row++) {
        for (int col = 0; col < sprite->width; col++) {
            int byte_idx = row * bytes_per_row + (col / 8);
            int bit_idx = 7 - (col % 8);  // MSB first
            bool pixel = (sprite->data[byte_idx] >> bit_idx) & 1;
            
            if (invert) pixel = !pixel;
            ssd1306_set_pixel(x + col, y + row, pixel);
        }
    }
}

void sprite_draw_transparent(const sprite_t *sprite, int x, int y, bool color) {
    if (!sprite || !sprite->data) return;
    
    int bytes_per_row = (sprite->width + 7) / 8;
    
    for (int row = 0; row < sprite->height; row++) {
        for (int col = 0; col < sprite->width; col++) {
            int byte_idx = row * bytes_per_row + (col / 8);
            int bit_idx = 7 - (col % 8);
            bool pixel = (sprite->data[byte_idx] >> bit_idx) & 1;
            
            if (pixel) {
                ssd1306_set_pixel(x + col, y + row, color);
            }
        }
    }
}

void sprite_draw_face(const face_sprite_set_t *face_set, int look_x, int look_y) {
    sprite_draw_face_animated(face_set, 0.0f, look_x, look_y, 0);
}

void sprite_draw_face_animated(const face_sprite_set_t *face_set,
                                float blink, int look_x, int look_y, int bounce_y) {
    if (!face_set) return;
    
    // Clamp look values
    if (look_x < -5) look_x = -5;
    if (look_x > 5) look_x = 5;
    if (look_y < -3) look_y = -3;
    if (look_y > 3) look_y = 3;
    
    // Draw eyebrows
    if (face_set->left_brow.data) {
        sprite_draw_transparent(&face_set->left_brow, 
            face_set->left_brow_x, face_set->left_brow_y + bounce_y, true);
    }
    if (face_set->right_brow.data) {
        sprite_draw_transparent(&face_set->right_brow,
            face_set->right_brow_x, face_set->right_brow_y + bounce_y, true);
    }
    
    // Draw eyes (with look offset)
    if (face_set->left_eye.data) {
        sprite_draw(&face_set->left_eye,
            face_set->left_eye_x + look_x, face_set->left_eye_y + look_y + bounce_y, false);
    }
    if (face_set->right_eye.data) {
        sprite_draw(&face_set->right_eye,
            face_set->right_eye_x + look_x, face_set->right_eye_y + look_y + bounce_y, false);
    }
    
    // Blink animation - cover eyes progressively
    if (blink > 0.3f) {
        int eye_h = face_set->left_eye.height;
        int blink_h = (int)(blink * eye_h);
        if (face_set->left_eye.data) {
            ssd1306_fill_rect(face_set->left_eye_x + look_x - 2, 
                             face_set->left_eye_y + bounce_y + look_y,
                             face_set->left_eye.width + 4, blink_h / 2 + 2, false);
            ssd1306_fill_rect(face_set->left_eye_x + look_x - 2,
                             face_set->left_eye_y + bounce_y + look_y + eye_h - blink_h / 2,
                             face_set->left_eye.width + 4, blink_h / 2 + 2, false);
        }
        if (face_set->right_eye.data) {
            ssd1306_fill_rect(face_set->right_eye_x + look_x - 2,
                             face_set->right_eye_y + bounce_y + look_y,
                             face_set->right_eye.width + 4, blink_h / 2 + 2, false);
            ssd1306_fill_rect(face_set->right_eye_x + look_x - 2,
                             face_set->right_eye_y + bounce_y + look_y + eye_h - blink_h / 2,
                             face_set->right_eye.width + 4, blink_h / 2 + 2, false);
        }
    }
    
    // Draw mouth
    if (face_set->mouth.data) {
        sprite_draw_transparent(&face_set->mouth,
            face_set->mouth_x, face_set->mouth_y + bounce_y, true);
    }
}

// ============================================================================
// BUILT-IN SPRITE DATA - "Mochi" Character
// Big cute rounded anime eyes with sparkly highlights!
// ============================================================================

// --- BIG CUTE EYES (24x20 pixels = 3 bytes per row = 60 bytes each) ---
// Design: Rounded rectangle white, large dark iris, BIG highlight + small highlight

const uint8_t SPRITE_EYE_NORMAL[] = {
    // Row 0-1: Top rounded edge
    0x00, 0xFF, 0x00,  //         ########
    0x07, 0xFF, 0xE0,  //      ##############
    // Row 2-3: Upper eye white
    0x1F, 0xFF, 0xF8,  //    ##################
    0x3F, 0xFF, 0xFC,  //   ####################
    // Row 4: Start of iris area
    0x7F, 0xFF, 0xFE,  //  ######################
    // Row 5-6: Iris top with big highlight
    0x7F, 0x00, 0xFE,  //  #######........#######
    0xFF, 0x00, 0x7F,  // ########........#######
    // Row 7-9: Main iris with highlights
    0xFE, 0x00, 0x7F,  // #######..........######  
    0xFE, 0x70, 0x7F,  // #######..###.....######  <- Big highlight
    0xFE, 0x70, 0x7F,  // #######..###.....######
    // Row 10-12: Lower iris
    0xFE, 0x00, 0x7F,  // #######..........######
    0xFE, 0x01, 0xFF,  // #######..........###### <- Small highlight
    0xFF, 0x01, 0xFF,  // ########.........#######
    // Row 13-14: Iris bottom
    0x7F, 0x00, 0xFE,  //  #######........######
    0x7F, 0x81, 0xFE,  //  ########......#######
    // Row 15-17: Lower eye white
    0x3F, 0xFF, 0xFC,  //   ####################
    0x1F, 0xFF, 0xF8,  //    ##################
    0x07, 0xFF, 0xE0,  //      ##############
    // Row 18-19: Bottom rounded edge
    0x01, 0xFF, 0x80,  //        ##########
    0x00, 0x7E, 0x00,  //          ######
};

// Happy eye - ^_^ style curved squint
const uint8_t SPRITE_EYE_HAPPY[] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x1F, 0xFF, 0xF8,  //    ##################
    0x3F, 0xFF, 0xFC,  //   ####################
    0x7F, 0xFF, 0xFE,  //  ######################
    0xFF, 0xFF, 0xFF,  // ########################
    0xFF, 0xFF, 0xFF,  // ########################
    0xFF, 0xFF, 0xFF,  // ########################
    0x7F, 0xFF, 0xFE,  //  ######################
    0x3F, 0xFF, 0xFC,  //   ####################
    0x1F, 0x81, 0xF8,  //    ######....######
    0x0F, 0x00, 0xF0,  //     ####......####
    0x06, 0x00, 0x60,  //      ##........##
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

// Closed eye - curved line for blinking
const uint8_t SPRITE_EYE_CLOSED[] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x07, 0xFF, 0xE0,  //      ##############
    0x1F, 0xFF, 0xF8,  //    ##################
    0x3F, 0xFF, 0xFC,  //   ####################
    0x3F, 0xFF, 0xFC,  //   ####################
    0x1F, 0xFF, 0xF8,  //    ##################
    0x07, 0xFF, 0xE0,  //      ##############
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

// Wide surprised eye - bigger pupil
const uint8_t SPRITE_EYE_WIDE[] = {
    0x00, 0xFF, 0x00,
    0x07, 0xFF, 0xE0,
    0x1F, 0xFF, 0xF8,
    0x3F, 0xFF, 0xFC,
    0x7F, 0xFF, 0xFE,
    0x7F, 0x81, 0xFE,  //  ########....########
    0xFF, 0x00, 0xFF,  // ########......########
    0xFE, 0x00, 0x7F,  // #######........#######
    0xFC, 0x60, 0x3F,  // ######...##....######  <- highlight
    0xFC, 0x60, 0x3F,  // ######...##....######
    0xFC, 0x00, 0x3F,  // ######..........######
    0xFC, 0x00, 0x3F,  // ######..........######
    0xFE, 0x00, 0x7F,  // #######........#######
    0xFF, 0x00, 0xFF,  // ########......########
    0x7F, 0x81, 0xFE,  //  ########....########
    0x3F, 0xFF, 0xFC,
    0x1F, 0xFF, 0xF8,
    0x07, 0xFF, 0xE0,
    0x01, 0xFF, 0x80,
    0x00, 0x7E, 0x00,
};

// Heart eyes for love emotion
const uint8_t SPRITE_EYE_HEART[] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x0E, 0x07, 0x00,  //     ###.....###
    0x1F, 0x0F, 0x80,  //    #####...#####
    0x3F, 0x9F, 0xC0,  //   #######.#######
    0x7F, 0xFF, 0xE0,  //  ###############
    0x7F, 0xFF, 0xE0,  //  ###############
    0xFF, 0xFF, 0xF0,  // ##################
    0xFF, 0xFF, 0xF0,  // ##################
    0xFF, 0xFF, 0xF0,  // ##################
    0x7F, 0xFF, 0xE0,  //  ################
    0x7F, 0xFF, 0xE0,  //  ################
    0x3F, 0xFF, 0xC0,  //   ##############
    0x1F, 0xFF, 0x80,  //    ############
    0x0F, 0xFF, 0x00,  //     ##########
    0x07, 0xFE, 0x00,  //      ########
    0x03, 0xFC, 0x00,  //       ######
    0x01, 0xF8, 0x00,  //        ####
    0x00, 0xF0, 0x00,  //         ##
    0x00, 0x00, 0x00,
};

// Sleepy half-closed eye
const uint8_t SPRITE_EYE_SLEEPY[] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x07, 0xFF, 0xE0,
    0x1F, 0xFF, 0xF8,
    0x3F, 0xFF, 0xFC,
    0x7F, 0x00, 0xFE,  //  #######....#######
    0x7E, 0x00, 0x7E,  //  ######......######
    0x7E, 0x70, 0x7E,  //  ######.###..######  <- highlight
    0x3F, 0x00, 0xFC,  //   #####......#####
    0x1F, 0xFF, 0xF8,
    0x07, 0xFF, 0xE0,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

// --- CUTE MOUTHS (24x10 pixels = 3 bytes per row = 30 bytes each) ---

// Normal - small cute line
const uint8_t SPRITE_MOUTH_NORMAL[] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0xFF, 0x00,  //         ########
    0x00, 0xFF, 0x00,  //         ########
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

// Cat smile :3 (cute w shape)
const uint8_t SPRITE_MOUTH_SMILE[] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x60, 0x00, 0x06,  //  ##..............##
    0x30, 0x42, 0x0C,  //   ##....#....#..##
    0x18, 0xE7, 0x18,  //    ##..###..###.##
    0x0F, 0x24, 0xF0,  //     ####..#..####
    0x03, 0x18, 0xC0,  //       ##...##..
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

// Big laugh - open D mouth
const uint8_t SPRITE_MOUTH_LAUGH[] = {
    0x0F, 0xFF, 0xF0,  //     ################
    0x1F, 0xFF, 0xF8,  //    ##################
    0x3F, 0xFF, 0xFC,  //   ####################
    0x30, 0x00, 0x0C,  //   ##..............##
    0x30, 0x00, 0x0C,  //   ##..............##
    0x18, 0x00, 0x18,  //    ##............##
    0x0C, 0x00, 0x30,  //     ##..........##
    0x07, 0x00, 0xE0,  //      ###......###
    0x01, 0xFF, 0x80,  //        ##########
    0x00, 0x7E, 0x00,  //          ######
};

// Sad frown
const uint8_t SPRITE_MOUTH_SAD[] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x00, 0x7E, 0x00,  //          ######
    0x01, 0xFF, 0x80,  //        ##########
    0x07, 0x81, 0xE0,  //      ####....####
    0x1E, 0x00, 0x78,  //    ####........####
    0x38, 0x00, 0x1C,  //   ###............###
    0x20, 0x00, 0x04,  //   #................#
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

// Surprised O
const uint8_t SPRITE_MOUTH_SURPRISED[] = {
    0x00, 0x7E, 0x00,  //          ######
    0x01, 0xFF, 0x80,  //        ##########
    0x03, 0xFF, 0xC0,  //       ############
    0x07, 0x81, 0xE0,  //      ####....####
    0x07, 0x00, 0xE0,  //      ###......###
    0x07, 0x00, 0xE0,  //      ###......###
    0x07, 0x81, 0xE0,  //      ####....####
    0x03, 0xFF, 0xC0,  //       ############
    0x01, 0xFF, 0x80,  //        ##########
    0x00, 0x7E, 0x00,  //          ######
};

// Angry gritted teeth
const uint8_t SPRITE_MOUTH_ANGRY[] = {
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
    0x1F, 0xFF, 0xF8,  //    ##################
    0x3F, 0xFF, 0xFC,  //   ####################
    0x36, 0xDB, 0x6C,  //   ## ## ## ## ## ##  <- teeth
    0x36, 0xDB, 0x6C,  //   ## ## ## ## ## ##
    0x3F, 0xFF, 0xFC,  //   ####################
    0x1F, 0xFF, 0xF8,  //    ##################
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

// --- THIN CUTE EYEBROWS (16x3 pixels = 2 bytes per row = 6 bytes each) ---

const uint8_t SPRITE_BROW_NORMAL[] = {
    0x3F, 0xFC,  //   ############
    0x7F, 0xFE,  //  ##############
    0x00, 0x00,
};

const uint8_t SPRITE_BROW_ANGRY[] = {
    0x00, 0x3E,  //           #####
    0x03, 0xFC,  //       ########
    0x3F, 0xE0,  //   #########
};

const uint8_t SPRITE_BROW_SAD[] = {
    0x3F, 0xE0,  //   #########
    0x03, 0xFC,  //       ########
    0x00, 0x3E,  //           #####
};

const uint8_t SPRITE_BROW_RAISED[] = {
    0x1F, 0xF8,  //    ##########
    0x3F, 0xFC,  //   ############
    0x00, 0x00,
};

// ============================================================================
// CHARACTER LAYOUTS
// ============================================================================

// Positions for big cute eyes on 128x64 screen
#define LEFT_EYE_X   12
#define RIGHT_EYE_X  92
#define EYE_Y        12
#define MOUTH_X      52
#define MOUTH_Y      50
#define LEFT_BROW_X  16
#define RIGHT_BROW_X 96
#define BROW_Y       5

// Mochi character - all expressions with big cute eyes
const character_pack_t CHARACTER_MOCHI = {
    .name = "Mochi",
    .expressions = {
        // NORMAL
        {
            .left_eye = {24, 20, SPRITE_EYE_NORMAL},
            .right_eye = {24, 20, SPRITE_EYE_NORMAL},
            .mouth = {24, 10, SPRITE_MOUTH_NORMAL},
            .left_brow = {16, 3, SPRITE_BROW_NORMAL},
            .right_brow = {16, 3, SPRITE_BROW_NORMAL},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X, .left_brow_y = BROW_Y,
            .right_brow_x = RIGHT_BROW_X, .right_brow_y = BROW_Y,
        },
        // HAPPY
        {
            .left_eye = {24, 20, SPRITE_EYE_HAPPY},
            .right_eye = {24, 20, SPRITE_EYE_HAPPY},
            .mouth = {24, 10, SPRITE_MOUTH_SMILE},
            .left_brow = {16, 3, SPRITE_BROW_RAISED},
            .right_brow = {16, 3, SPRITE_BROW_RAISED},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X, .left_brow_y = BROW_Y - 2,
            .right_brow_x = RIGHT_BROW_X, .right_brow_y = BROW_Y - 2,
        },
        // LAUGHING
        {
            .left_eye = {24, 20, SPRITE_EYE_HAPPY},
            .right_eye = {24, 20, SPRITE_EYE_HAPPY},
            .mouth = {24, 10, SPRITE_MOUTH_LAUGH},
            .left_brow = {16, 3, SPRITE_BROW_RAISED},
            .right_brow = {16, 3, SPRITE_BROW_RAISED},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X, .left_brow_y = BROW_Y - 3,
            .right_brow_x = RIGHT_BROW_X, .right_brow_y = BROW_Y - 3,
        },
        // ANGRY
        {
            .left_eye = {24, 20, SPRITE_EYE_NORMAL},
            .right_eye = {24, 20, SPRITE_EYE_NORMAL},
            .mouth = {24, 10, SPRITE_MOUTH_ANGRY},
            .left_brow = {16, 3, SPRITE_BROW_ANGRY},
            .right_brow = {16, 3, SPRITE_BROW_ANGRY},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X + 4, .left_brow_y = BROW_Y + 3,
            .right_brow_x = RIGHT_BROW_X - 4, .right_brow_y = BROW_Y + 3,
        },
        // SAD
        {
            .left_eye = {24, 20, SPRITE_EYE_SLEEPY},
            .right_eye = {24, 20, SPRITE_EYE_SLEEPY},
            .mouth = {24, 10, SPRITE_MOUTH_SAD},
            .left_brow = {16, 3, SPRITE_BROW_SAD},
            .right_brow = {16, 3, SPRITE_BROW_SAD},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X, .left_brow_y = BROW_Y,
            .right_brow_x = RIGHT_BROW_X, .right_brow_y = BROW_Y,
        },
        // SURPRISED
        {
            .left_eye = {24, 20, SPRITE_EYE_WIDE},
            .right_eye = {24, 20, SPRITE_EYE_WIDE},
            .mouth = {24, 10, SPRITE_MOUTH_SURPRISED},
            .left_brow = {16, 3, SPRITE_BROW_RAISED},
            .right_brow = {16, 3, SPRITE_BROW_RAISED},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X, .left_brow_y = BROW_Y - 4,
            .right_brow_x = RIGHT_BROW_X, .right_brow_y = BROW_Y - 4,
        },
        // SLEEPY
        {
            .left_eye = {24, 20, SPRITE_EYE_SLEEPY},
            .right_eye = {24, 20, SPRITE_EYE_SLEEPY},
            .mouth = {24, 10, SPRITE_MOUTH_NORMAL},
            .left_brow = {16, 3, SPRITE_BROW_SAD},
            .right_brow = {16, 3, SPRITE_BROW_SAD},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X, .left_brow_y = BROW_Y + 2,
            .right_brow_x = RIGHT_BROW_X, .right_brow_y = BROW_Y + 2,
        },
        // SLEEPING
        {
            .left_eye = {24, 20, SPRITE_EYE_CLOSED},
            .right_eye = {24, 20, SPRITE_EYE_CLOSED},
            .mouth = {24, 10, SPRITE_MOUTH_NORMAL},
            .left_brow = {16, 3, SPRITE_BROW_NORMAL},
            .right_brow = {16, 3, SPRITE_BROW_NORMAL},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X, .left_brow_y = BROW_Y,
            .right_brow_x = RIGHT_BROW_X, .right_brow_y = BROW_Y,
        },
        // LOVE
        {
            .left_eye = {24, 20, SPRITE_EYE_HEART},
            .right_eye = {24, 20, SPRITE_EYE_HEART},
            .mouth = {24, 10, SPRITE_MOUTH_SMILE},
            .left_brow = {16, 3, SPRITE_BROW_RAISED},
            .right_brow = {16, 3, SPRITE_BROW_RAISED},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X, .left_brow_y = BROW_Y - 2,
            .right_brow_x = RIGHT_BROW_X, .right_brow_y = BROW_Y - 2,
        },
        // WINK
        {
            .left_eye = {24, 20, SPRITE_EYE_NORMAL},
            .right_eye = {24, 20, SPRITE_EYE_HAPPY},
            .mouth = {24, 10, SPRITE_MOUTH_SMILE},
            .left_brow = {16, 3, SPRITE_BROW_RAISED},
            .right_brow = {16, 3, SPRITE_BROW_NORMAL},
            .left_eye_x = LEFT_EYE_X, .left_eye_y = EYE_Y,
            .right_eye_x = RIGHT_EYE_X, .right_eye_y = EYE_Y,
            .mouth_x = MOUTH_X, .mouth_y = MOUTH_Y,
            .left_brow_x = LEFT_BROW_X, .left_brow_y = BROW_Y - 2,
            .right_brow_x = RIGHT_BROW_X, .right_brow_y = BROW_Y,
        },
    }
};

const character_pack_t* sprite_get_builtin_character(void) {
    return &CHARACTER_MOCHI;
}
