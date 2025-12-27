#ifndef AUDIO_H
#define AUDIO_H

#include "raylib.h"

// Music tempo and timing
#define BPM_CHILL 110
#define BPM_HARDCORE 150
#define BEATS_PER_MEASURE 4

// Streak timeout (seconds without hitting green = back to chill)
#define STREAK_TIMEOUT 4.0f

// Initialize audio system and load/generate music
void audio_init(void);

// Update music (call each frame)
void audio_update(void);

// Cleanup audio resources
void audio_cleanup(void);

// Toggle music on/off
void audio_toggle_music(void);

// Check if music is playing
bool audio_is_playing(void);

// Call when player hits a green (safe) letter - activates hardcore mode
void audio_on_green_hit(void);

// Call when adversary hits player - stops hardcore mode
void audio_on_adversary_hit(void);

// Check if hardcore mode is active
bool audio_is_hardcore(void);

#endif // AUDIO_H
