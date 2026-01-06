/*
 * Piezo Buzzer Driver for ESP32
 * Supports melodies with portamento and MIDI playback
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Musical note frequencies (Hz)
#define NOTE_REST   0
#define NOTE_C4     262
#define NOTE_CS4    277
#define NOTE_D4     294
#define NOTE_DS4    311
#define NOTE_E4     330
#define NOTE_F4     349
#define NOTE_FS4    370
#define NOTE_G4     392
#define NOTE_GS4    415
#define NOTE_A4     440
#define NOTE_AS4    466
#define NOTE_B4     494
#define NOTE_C5     523
#define NOTE_CS5    554
#define NOTE_D5     587
#define NOTE_DS5    622
#define NOTE_E5     659
#define NOTE_F5     698
#define NOTE_FS5    740
#define NOTE_G5     784
#define NOTE_GS5    831
#define NOTE_A5     880
#define NOTE_AS5    932
#define NOTE_B5     988
#define NOTE_C6     1047
#define NOTE_D6     1175
#define NOTE_E6     1319
#define NOTE_F6     1397
#define NOTE_G6     1568

// MIDI note numbers
#define MIDI_C4     60
#define MIDI_D4     62
#define MIDI_E4     64
#define MIDI_F4     65
#define MIDI_G4     67
#define MIDI_A4     69
#define MIDI_B4     71
#define MIDI_C5     72

// Sound effect types (mapped to emotions)
typedef enum {
    SFX_NONE = 0,
    SFX_HAPPY,      // Cheerful ascending
    SFX_LAUGHING,   // Bouncy giggles
    SFX_SAD,        // Slow descending
    SFX_ANGRY,      // Low growl
    SFX_SURPRISED,  // Quick up glide
    SFX_SLEEPY,     // Gentle lullaby
    SFX_SLEEPING,   // Soft snore
    SFX_CRAZY,      // Wild random
    SFX_LOVE,       // Heartbeat melody
    SFX_WINK,       // Playful boop
    SFX_SMUG,       // Sassy slide
    SFX_SCARED,     // Trembling
    SFX_BLINK,      // Quick blip
    SFX_STARTUP,    // Boot sound
    SFX_BIRTHDAY,   // Happy Birthday song
} sound_effect_t;

// Simple MIDI event structure for monophonic playback
typedef struct {
    uint8_t note;       // MIDI note number (0-127, 0 = rest)
    uint16_t duration;  // Duration in milliseconds
    uint8_t velocity;   // Velocity (volume) 0-127
} midi_event_t;

// MIDI song structure
typedef struct {
    const midi_event_t *events;
    uint16_t event_count;
    uint16_t tempo_bpm;
    bool loop;
    bool portamento;    // Enable smooth note gliding
} midi_song_t;

/**
 * Initialize the buzzer on specified GPIO pin
 */
esp_err_t buzzer_init(int gpio_pin);

/**
 * Play a single tone at given frequency
 * @param freq_hz Frequency in Hz (0 = silence)
 */
void buzzer_tone(uint16_t freq_hz);

/**
 * Stop any playing sound
 */
void buzzer_stop(void);

/**
 * Play a sound effect (non-blocking, runs in background)
 */
void buzzer_play_sfx(sound_effect_t sfx);

/**
 * Update buzzer state - call from main loop for portamento effects
 * @param delta_ms Milliseconds since last update
 */
void buzzer_update(uint32_t delta_ms);

/**
 * Check if a sound is currently playing
 */
bool buzzer_is_playing(void);

/**
 * Set master volume (0-100)
 */
void buzzer_set_volume(uint8_t volume);

// ============================================================================
// MIDI PLAYBACK
// ============================================================================

/**
 * Convert MIDI note number to frequency in Hz
 * @param midi_note MIDI note number (0-127, middle C = 60)
 * @return Frequency in Hz
 */
uint16_t midi_note_to_freq(uint8_t midi_note);

/**
 * Play a MIDI song (non-blocking)
 * @param song Pointer to MIDI song structure
 */
void buzzer_play_midi(const midi_song_t *song);

/**
 * Load and play MIDI from raw MIDI file data
 * Note: Only supports simple single-track MIDI (format 0)
 * @param data Pointer to MIDI file data
 * @param length Length of MIDI data
 * @return true if successfully parsed and started playing
 */
bool buzzer_play_midi_data(const uint8_t *data, size_t length);

/**
 * Get built-in Happy Birthday song
 */
const midi_song_t* buzzer_get_birthday_song(void);

#endif // BUZZER_H

