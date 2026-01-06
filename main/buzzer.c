/*
 * Piezo Buzzer Driver Implementation
 * Uses LEDC PWM for tone generation with smooth portamento and MIDI support
 */

#include "buzzer.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "buzzer";

// LEDC configuration
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_10_BIT
#define LEDC_DUTY_50        (512)  // 50% duty cycle

// Portamento settings
#define PORTAMENTO_STEPS    20     // Steps for glide
#define MAX_MELODY_NOTES    16     // Max notes in a melody
#define MAX_MIDI_EVENTS     64     // Max events for MIDI songs

// Note structure for melodies
typedef struct {
    uint16_t freq;      // Target frequency
    uint16_t duration;  // Duration in ms
    bool portamento;    // Glide to this note
} note_t;

// Melody structure
typedef struct {
    note_t notes[MAX_MELODY_NOTES];
    uint8_t length;
    bool loop;
} melody_t;

// ============================================================================
// MIDI NOTE FREQUENCY TABLE
// ============================================================================

// Pre-calculated frequencies for MIDI notes 21-108 (A0 to C8)
static const uint16_t MIDI_FREQ_TABLE[] = {
    28,    29,    31,    33,    35,    37,    39,    41,    44,    46,    49,    52,    // A0-G#1
    55,    58,    62,    65,    69,    73,    78,    82,    87,    92,    98,   104,    // A1-G#2
   110,   117,   123,   131,   139,   147,   156,   165,   175,   185,   196,   208,    // A2-G#3
   220,   233,   247,   262,   277,   294,   311,   330,   349,   370,   392,   415,    // A3-G#4
   440,   466,   494,   523,   554,   587,   622,   659,   698,   740,   784,   831,    // A4-G#5
   880,   932,   988,  1047,  1109,  1175,  1245,  1319,  1397,  1480,  1568,  1661,    // A5-G#6
  1760,  1865,  1976,  2093,  2217,  2349,  2489,  2637,  2794,  2960,  3136,  3322,    // A6-G#7
  3520,  3729,  3951,  4186                                                             // A7-C8
};

// Convert MIDI note to frequency
uint16_t midi_note_to_freq(uint8_t midi_note) {
    if (midi_note == 0) return 0;  // Rest
    if (midi_note < 21) return 28;  // Below A0, clamp
    if (midi_note > 108) return 4186;  // Above C8, clamp
    return MIDI_FREQ_TABLE[midi_note - 21];
}

// ============================================================================
// HAPPY BIRTHDAY SONG (MIDI FORMAT)
// ============================================================================

static const midi_event_t BIRTHDAY_EVENTS[] = {
    // "Happy Birthday to You" in G major, 3/4 time
    // Traditional melody with proper rhythm
    // Tempo: dotted quarter = ~100 BPM, so quarter = 300ms
    
    // "Happy Birthday to you" (first line)
    {67, 200, 90},   // G4 - "Hap-" (pickup eighth)
    {67, 200, 90},   // G4 - "-py" (pickup eighth)
    {69, 350, 100},  // A4 - "Birth-" (quarter)
    {67, 350, 100},  // G4 - "-day" (quarter)
    {72, 350, 100},  // C5 - "to" (quarter)
    {71, 700, 100},  // B4 - "you" (half note)
    {0,  150, 0},    // breath
    
    // "Happy Birthday to you" (second line)
    {67, 200, 90},   // G4 - "Hap-"
    {67, 200, 90},   // G4 - "-py"
    {69, 350, 100},  // A4 - "Birth-"
    {67, 350, 100},  // G4 - "-day"
    {74, 350, 100},  // D5 - "to"
    {72, 700, 100},  // C5 - "you"
    {0,  150, 0},    // breath
    
    // "Happy Birthday dear [name]"
    {67, 200, 90},   // G4 - "Hap-"
    {67, 200, 90},   // G4 - "-py"
    {79, 350, 100},  // G5 - "Birth-" (high note!)
    {76, 350, 100},  // E5 - "-day"
    {72, 350, 100},  // C5 - "dear"
    {71, 350, 100},  // B4 - "[na-"
    {69, 700, 100},  // A4 - "-me]"
    {0,  150, 0},    // breath
    
    // "Happy Birthday to you!" (final line)
    {77, 200, 90},   // F5 - "Hap-"
    {77, 200, 90},   // F5 - "-py"
    {76, 350, 100},  // E5 - "Birth-"
    {72, 350, 100},  // C5 - "-day"
    {74, 350, 100},  // D5 - "to"
    {72, 900, 100},  // C5 - "you!" (dotted half - hold it!)
};

static const midi_song_t BIRTHDAY_SONG = {
    .events = BIRTHDAY_EVENTS,
    .event_count = sizeof(BIRTHDAY_EVENTS) / sizeof(midi_event_t),
    .tempo_bpm = 120,
    .loop = false,
    .portamento = false  // Clean note transitions for birthday song
};

const midi_song_t* buzzer_get_birthday_song(void) {
    return &BIRTHDAY_SONG;
}

// Playback mode
typedef enum {
    PLAY_MODE_NONE,
    PLAY_MODE_MELODY,
    PLAY_MODE_MIDI
} play_mode_t;

// Buzzer state
static struct {
    int gpio_pin;
    bool initialized;
    uint8_t volume;
    
    // Current playback
    uint16_t current_freq;
    uint16_t target_freq;
    float freq_step;
    
    // Melody playback
    melody_t current_melody;
    uint8_t melody_index;
    uint32_t note_time_remaining;
    bool playing;
    play_mode_t play_mode;
    
    // MIDI playback
    const midi_song_t *current_song;
    uint16_t midi_event_index;
    
    // Portamento state
    uint8_t porta_steps_remaining;
} buzzer = {
    .volume = 80,
    .initialized = false,
    .play_mode = PLAY_MODE_NONE,
};

// ============================================================================
// PREDEFINED MELODIES FOR EACH EMOTION
// ============================================================================

// Happy - cheerful ascending arpeggio with bounce
static const melody_t MELODY_HAPPY = {
    .notes = {
        {NOTE_C5, 80, true},
        {NOTE_E5, 80, true},
        {NOTE_G5, 80, true},
        {NOTE_C6, 120, true},
        {NOTE_G5, 60, true},
        {NOTE_C6, 150, false},
        {NOTE_REST, 50, false},
    },
    .length = 7,
    .loop = false
};

// Laughing - bouncy staccato giggles
static const melody_t MELODY_LAUGHING = {
    .notes = {
        {NOTE_E5, 50, false},
        {NOTE_G5, 50, true},
        {NOTE_E5, 50, false},
        {NOTE_G5, 50, true},
        {NOTE_A5, 50, false},
        {NOTE_G5, 50, true},
        {NOTE_E5, 50, false},
        {NOTE_C5, 100, true},
    },
    .length = 8,
    .loop = false
};

// Sad - slow descending with vibrato feel
static const melody_t MELODY_SAD = {
    .notes = {
        {NOTE_E5, 200, true},
        {NOTE_D5, 200, true},
        {NOTE_C5, 200, true},
        {NOTE_B4, 300, true},
        {NOTE_REST, 100, false},
    },
    .length = 5,
    .loop = false
};

// Angry - low aggressive growl
static const melody_t MELODY_ANGRY = {
    .notes = {
        {NOTE_E4, 100, false},
        {NOTE_DS4, 100, true},
        {NOTE_E4, 100, false},
        {NOTE_DS4, 150, true},
        {NOTE_C4, 200, true},
    },
    .length = 5,
    .loop = false
};

// Surprised - quick upward sweep
static const melody_t MELODY_SURPRISED = {
    .notes = {
        {NOTE_C4, 30, true},
        {NOTE_G4, 30, true},
        {NOTE_C5, 30, true},
        {NOTE_G5, 30, true},
        {NOTE_C6, 150, true},
        {NOTE_REST, 50, false},
    },
    .length = 6,
    .loop = false
};

// Sleepy - gentle descending lullaby
static const melody_t MELODY_SLEEPY = {
    .notes = {
        {NOTE_G5, 150, true},
        {NOTE_E5, 150, true},
        {NOTE_C5, 200, true},
        {NOTE_REST, 100, false},
    },
    .length = 4,
    .loop = false
};

// Sleeping - soft rhythmic "snore"
static const melody_t MELODY_SLEEPING = {
    .notes = {
        {NOTE_C4, 300, true},
        {NOTE_G4, 200, true},
        {NOTE_REST, 400, false},
    },
    .length = 3,
    .loop = false
};

// Crazy - wild random jumps
static const melody_t MELODY_CRAZY = {
    .notes = {
        {NOTE_C5, 40, true},
        {NOTE_G5, 40, true},
        {NOTE_D5, 40, true},
        {NOTE_A5, 40, true},
        {NOTE_E5, 40, true},
        {NOTE_B5, 40, true},
        {NOTE_F5, 40, true},
        {NOTE_C6, 100, true},
    },
    .length = 8,
    .loop = false
};

// Love - heartbeat-like with sweet melody
static const melody_t MELODY_LOVE = {
    .notes = {
        {NOTE_C5, 100, true},
        {NOTE_E5, 100, true},
        {NOTE_G5, 150, true},
        {NOTE_E5, 100, true},
        {NOTE_C5, 100, true},
        {NOTE_E5, 200, true},
    },
    .length = 6,
    .loop = false
};

// Wink - playful short boop
static const melody_t MELODY_WINK = {
    .notes = {
        {NOTE_E5, 60, true},
        {NOTE_G5, 100, true},
        {NOTE_REST, 30, false},
    },
    .length = 3,
    .loop = false
};

// Smug - sassy sliding notes
static const melody_t MELODY_SMUG = {
    .notes = {
        {NOTE_G5, 100, true},
        {NOTE_FS5, 100, true},
        {NOTE_G5, 150, true},
        {NOTE_REST, 50, false},
    },
    .length = 4,
    .loop = false
};

// Scared - trembling warble
static const melody_t MELODY_SCARED = {
    .notes = {
        {NOTE_E5, 50, true},
        {NOTE_F5, 50, true},
        {NOTE_E5, 50, true},
        {NOTE_F5, 50, true},
        {NOTE_E5, 50, true},
        {NOTE_D5, 100, true},
        {NOTE_REST, 50, false},
    },
    .length = 7,
    .loop = false
};

// Blink - tiny blip
static const melody_t MELODY_BLINK = {
    .notes = {
        {NOTE_C6, 30, false},
        {NOTE_REST, 20, false},
    },
    .length = 2,
    .loop = false
};

// Startup - cute boot jingle
static const melody_t MELODY_STARTUP = {
    .notes = {
        {NOTE_C5, 100, true},
        {NOTE_E5, 100, true},
        {NOTE_G5, 100, true},
        {NOTE_C6, 200, true},
        {NOTE_REST, 100, false},
    },
    .length = 5,
    .loop = false
};

// ============================================================================
// BUZZER FUNCTIONS
// ============================================================================

esp_err_t buzzer_init(int gpio_pin) {
    buzzer.gpio_pin = gpio_pin;
    
    // Configure LEDC timer
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = 1000,  // Initial frequency
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&timer_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC timer");
        return ret;
    }
    
    // Configure LEDC channel
    ledc_channel_config_t channel_conf = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = gpio_pin,
        .duty = 0,  // Start silent
        .hpoint = 0
    };
    ret = ledc_channel_config(&channel_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LEDC channel");
        return ret;
    }
    
    buzzer.initialized = true;
    buzzer.playing = false;
    buzzer.current_freq = 0;
    buzzer.target_freq = 0;
    
    ESP_LOGI(TAG, "Buzzer initialized on GPIO%d", gpio_pin);
    return ESP_OK;
}

void buzzer_tone(uint16_t freq_hz) {
    if (!buzzer.initialized) return;
    
    if (freq_hz == 0) {
        // Silence
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    } else {
        // Set frequency and 50% duty (adjusted by volume)
        ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq_hz);
        uint32_t duty = (LEDC_DUTY_50 * buzzer.volume) / 100;
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    }
    
    buzzer.current_freq = freq_hz;
}

void buzzer_stop(void) {
    buzzer_tone(0);
    buzzer.playing = false;
    buzzer.target_freq = 0;
    buzzer.play_mode = PLAY_MODE_NONE;
    buzzer.current_song = NULL;
}

static void start_melody(const melody_t *melody) {
    if (!melody || melody->length == 0) return;
    
    memcpy(&buzzer.current_melody, melody, sizeof(melody_t));
    buzzer.melody_index = 0;
    buzzer.playing = true;
    buzzer.play_mode = PLAY_MODE_MELODY;
    
    // Start first note
    note_t *note = &buzzer.current_melody.notes[0];
    buzzer.note_time_remaining = note->duration;
    
    if (note->portamento && buzzer.current_freq > 0 && note->freq > 0) {
        // Glide from current frequency
        buzzer.target_freq = note->freq;
        buzzer.porta_steps_remaining = PORTAMENTO_STEPS;
        buzzer.freq_step = (float)(note->freq - buzzer.current_freq) / PORTAMENTO_STEPS;
    } else {
        // Immediate jump to frequency
        buzzer.target_freq = note->freq;
        buzzer.current_freq = note->freq;
        buzzer_tone(note->freq);
        buzzer.porta_steps_remaining = 0;
    }
}

// Start playing a MIDI song
void buzzer_play_midi(const midi_song_t *song) {
    if (!buzzer.initialized || !song || song->event_count == 0) return;
    
    buzzer.current_song = song;
    buzzer.midi_event_index = 0;
    buzzer.playing = true;
    buzzer.play_mode = PLAY_MODE_MIDI;
    
    // Start first note
    const midi_event_t *event = &song->events[0];
    uint16_t freq = midi_note_to_freq(event->note);
    buzzer.note_time_remaining = event->duration;
    
    // Apply portamento only if enabled for this song
    if (song->portamento && buzzer.current_freq > 0 && freq > 0) {
        buzzer.target_freq = freq;
        buzzer.porta_steps_remaining = PORTAMENTO_STEPS / 2;
        buzzer.freq_step = (float)(freq - buzzer.current_freq) / (PORTAMENTO_STEPS / 2);
    } else {
        // Direct note change (no glide)
        buzzer.target_freq = freq;
        buzzer.current_freq = freq;
        buzzer_tone(freq);
        buzzer.porta_steps_remaining = 0;
    }
}

// Parse and play raw MIDI file data (simplified - format 0 only)
bool buzzer_play_midi_data(const uint8_t *data, size_t length) {
    if (!data || length < 14) return false;
    
    // Check MIDI header "MThd"
    if (data[0] != 'M' || data[1] != 'T' || data[2] != 'h' || data[3] != 'd') {
        ESP_LOGE(TAG, "Invalid MIDI header");
        return false;
    }
    
    // For now, just log that we received MIDI data
    // Full MIDI parsing would require more complex implementation
    ESP_LOGI(TAG, "MIDI data received (%d bytes) - using built-in songs", (int)length);
    
    // Play birthday song as fallback
    buzzer_play_midi(&BIRTHDAY_SONG);
    return true;
}

void buzzer_play_sfx(sound_effect_t sfx) {
    if (!buzzer.initialized) return;
    
    // Special case for Birthday - use MIDI playback
    if (sfx == SFX_BIRTHDAY) {
        buzzer_play_midi(&BIRTHDAY_SONG);
        return;
    }
    
    const melody_t *melody = NULL;
    
    switch (sfx) {
        case SFX_HAPPY:     melody = &MELODY_HAPPY; break;
        case SFX_LAUGHING:  melody = &MELODY_LAUGHING; break;
        case SFX_SAD:       melody = &MELODY_SAD; break;
        case SFX_ANGRY:     melody = &MELODY_ANGRY; break;
        case SFX_SURPRISED: melody = &MELODY_SURPRISED; break;
        case SFX_SLEEPY:    melody = &MELODY_SLEEPY; break;
        case SFX_SLEEPING:  melody = &MELODY_SLEEPING; break;
        case SFX_CRAZY:     melody = &MELODY_CRAZY; break;
        case SFX_LOVE:      melody = &MELODY_LOVE; break;
        case SFX_WINK:      melody = &MELODY_WINK; break;
        case SFX_SMUG:      melody = &MELODY_SMUG; break;
        case SFX_SCARED:    melody = &MELODY_SCARED; break;
        case SFX_BLINK:     melody = &MELODY_BLINK; break;
        case SFX_STARTUP:   melody = &MELODY_STARTUP; break;
        default:
            buzzer_stop();
            return;
    }
    
    if (melody) {
        start_melody(melody);
    }
}

void buzzer_update(uint32_t delta_ms) {
    if (!buzzer.initialized || !buzzer.playing) return;
    
    // Handle portamento glide
    if (buzzer.porta_steps_remaining > 0) {
        buzzer.current_freq += (int)buzzer.freq_step;
        buzzer_tone((uint16_t)buzzer.current_freq);
        buzzer.porta_steps_remaining--;
        
        if (buzzer.porta_steps_remaining == 0) {
            // Snap to exact target
            buzzer.current_freq = buzzer.target_freq;
            buzzer_tone(buzzer.target_freq);
        }
    }
    
    // Handle note timing
    if (buzzer.note_time_remaining > delta_ms) {
        buzzer.note_time_remaining -= delta_ms;
    } else {
        // Time to advance to next note
        if (buzzer.play_mode == PLAY_MODE_MELODY) {
            // Melody mode
            buzzer.melody_index++;
            
            if (buzzer.melody_index >= buzzer.current_melody.length) {
                if (buzzer.current_melody.loop) {
                    buzzer.melody_index = 0;
                } else {
                    buzzer_stop();
                    return;
                }
            }
            
            note_t *note = &buzzer.current_melody.notes[buzzer.melody_index];
            buzzer.note_time_remaining = note->duration;
            
            if (note->portamento && buzzer.current_freq > 0 && note->freq > 0) {
                buzzer.target_freq = note->freq;
                buzzer.porta_steps_remaining = PORTAMENTO_STEPS;
                buzzer.freq_step = (float)(note->freq - buzzer.current_freq) / PORTAMENTO_STEPS;
            } else {
                buzzer.target_freq = note->freq;
                buzzer.current_freq = note->freq;
                buzzer_tone(note->freq);
                buzzer.porta_steps_remaining = 0;
            }
            
        } else if (buzzer.play_mode == PLAY_MODE_MIDI) {
            // MIDI mode
            buzzer.midi_event_index++;
            
            if (buzzer.midi_event_index >= buzzer.current_song->event_count) {
                if (buzzer.current_song->loop) {
                    buzzer.midi_event_index = 0;
                } else {
                    buzzer_stop();
                    return;
                }
            }
            
            const midi_event_t *event = &buzzer.current_song->events[buzzer.midi_event_index];
            uint16_t freq = midi_note_to_freq(event->note);
            buzzer.note_time_remaining = event->duration;
            
            // Only use portamento if enabled for this song
            if (buzzer.current_song->portamento && buzzer.current_freq > 0 && freq > 0 && event->note != 0) {
                buzzer.target_freq = freq;
                buzzer.porta_steps_remaining = PORTAMENTO_STEPS / 2;
                buzzer.freq_step = (float)(freq - buzzer.current_freq) / (PORTAMENTO_STEPS / 2);
            } else {
                // Direct note change
                buzzer.target_freq = freq;
                buzzer.current_freq = freq;
                buzzer_tone(freq);
                buzzer.porta_steps_remaining = 0;
            }
        }
    }
}

bool buzzer_is_playing(void) {
    return buzzer.playing;
}

void buzzer_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    buzzer.volume = volume;
}

