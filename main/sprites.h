/*
 * Sprite System for Desktoy
 * Allows loading custom face sprites for different expressions
 */

#ifndef SPRITES_H
#define SPRITES_H

#include <stdint.h>
#include <stdbool.h>

// Sprite dimensions (can vary per sprite)
typedef struct {
    uint8_t width;      // Width in pixels
    uint8_t height;     // Height in pixels  
    const uint8_t *data; // Bitmap data (1 bit per pixel, row-major, MSB first)
} sprite_t;

// A face is composed of component sprites
typedef struct {
    sprite_t left_eye;
    sprite_t right_eye;
    sprite_t mouth;
    sprite_t left_brow;
    sprite_t right_brow;
    // Positions (top-left corner)
    int8_t left_eye_x, left_eye_y;
    int8_t right_eye_x, right_eye_y;
    int8_t mouth_x, mouth_y;
    int8_t left_brow_x, left_brow_y;
    int8_t right_brow_x, right_brow_y;
} face_sprite_set_t;

// Expression names for sprite indexing
typedef enum {
    SPRITE_EXPR_NORMAL = 0,
    SPRITE_EXPR_HAPPY,
    SPRITE_EXPR_LAUGHING,
    SPRITE_EXPR_ANGRY,
    SPRITE_EXPR_SAD,
    SPRITE_EXPR_SURPRISED,
    SPRITE_EXPR_SLEEPY,
    SPRITE_EXPR_SLEEPING,
    SPRITE_EXPR_LOVE,
    SPRITE_EXPR_WINK,
    SPRITE_EXPR_COUNT
} sprite_expression_t;

// Full character sprite pack (all expressions)
typedef struct {
    const char *name;
    face_sprite_set_t expressions[SPRITE_EXPR_COUNT];
} character_pack_t;

/**
 * Draw a sprite at position (x, y)
 * @param sprite Sprite to draw
 * @param x X position (top-left)
 * @param y Y position (top-left)
 * @param invert If true, draws black pixels where sprite has white
 */
void sprite_draw(const sprite_t *sprite, int x, int y, bool invert);

/**
 * Draw a sprite with transparency (only draws 'on' pixels)
 * @param sprite Sprite to draw
 * @param x X position
 * @param y Y position
 * @param color true = white pixels, false = black pixels
 */
void sprite_draw_transparent(const sprite_t *sprite, int x, int y, bool color);

/**
 * Draw a complete face expression using sprites
 * @param face_set The face sprite set to use
 * @param look_x Eye look offset X (-5 to 5)
 * @param look_y Eye look offset Y (-3 to 3)
 */
void sprite_draw_face(const face_sprite_set_t *face_set, int look_x, int look_y);

/**
 * Draw a face with animation parameters
 * @param face_set The face sprite set
 * @param blink 0.0 = open, 1.0 = closed (for blink animation)
 * @param look_x Eye look offset
 * @param look_y Eye look offset
 * @param bounce_y Vertical bounce offset
 */
void sprite_draw_face_animated(const face_sprite_set_t *face_set, 
                                float blink, int look_x, int look_y, int bounce_y);

// ============================================================================
// BUILT-IN SPRITE DATA - "Mochi" character (big cute anime eyes!)
// ============================================================================

// --- Big Cute Eyes (24x20 pixels each) ---
extern const uint8_t SPRITE_EYE_NORMAL[];   // Rounded with large pupil & highlights
extern const uint8_t SPRITE_EYE_HAPPY[];    // ^_^ squint style
extern const uint8_t SPRITE_EYE_CLOSED[];   // Curved line for blink/sleep
extern const uint8_t SPRITE_EYE_WIDE[];     // Surprised big eye
extern const uint8_t SPRITE_EYE_HEART[];    // Heart-shaped for love
extern const uint8_t SPRITE_EYE_SLEEPY[];   // Half-closed droopy

// --- Cute Mouths (24x10 pixels) ---
extern const uint8_t SPRITE_MOUTH_NORMAL[];    // Small line
extern const uint8_t SPRITE_MOUTH_SMILE[];     // Cat smile :3
extern const uint8_t SPRITE_MOUTH_LAUGH[];     // Big open D
extern const uint8_t SPRITE_MOUTH_SAD[];       // Frown
extern const uint8_t SPRITE_MOUTH_SURPRISED[]; // O shape
extern const uint8_t SPRITE_MOUTH_ANGRY[];     // Gritted teeth

// --- Thin Eyebrows (16x3 pixels) ---
extern const uint8_t SPRITE_BROW_NORMAL[];
extern const uint8_t SPRITE_BROW_ANGRY[];
extern const uint8_t SPRITE_BROW_SAD[];
extern const uint8_t SPRITE_BROW_RAISED[];

// Built-in character pack
extern const character_pack_t CHARACTER_MOCHI;

// Get built-in character
const character_pack_t* sprite_get_builtin_character(void);

#endif // SPRITES_H

