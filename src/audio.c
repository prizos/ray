#include "audio.h"
#include <math.h>

// Dual-mode synthwave generator:
// - Chill mode: Pachelbel's Canon (ambient)
// - Hardcore mode: Intense minor key (when hitting green letters)

#define SAMPLE_RATE 44100
#define SAMPLE_SIZE 16
#define CHANNELS 1

// === NOTE FREQUENCIES ===
// Chill mode (D major - Pachelbel's Canon)
#define NOTE_D3  146.83f
#define NOTE_FS3 185.00f
#define NOTE_G3  196.00f
#define NOTE_A3  220.00f
#define NOTE_B3  246.94f
#define NOTE_D4  293.66f
#define NOTE_E4  329.63f
#define NOTE_FS4 369.99f
#define NOTE_G4  392.00f
#define NOTE_A4  440.00f
#define NOTE_B4  493.88f

// Hardcore mode (A minor - dark and intense)
#define NOTE_A2  110.00f
#define NOTE_C3  130.81f
#define NOTE_E3  164.81f
#define NOTE_F3  174.61f
#define NOTE_G3H 196.00f
#define NOTE_A3H 220.00f
#define NOTE_C4  261.63f
#define NOTE_E4H 329.63f
#define NOTE_F4  349.23f
#define NOTE_G4H 392.00f
#define NOTE_A4H 440.00f

// === CHORD PROGRESSIONS ===
// Chill: D - A - Bm - F#m - G - D - G - A
static const float CHORD_D[]  = { NOTE_D3, NOTE_FS3, NOTE_A3, NOTE_D4 };
static const float CHORD_A[]  = { NOTE_A3, NOTE_E4, NOTE_A4, NOTE_E4 };
static const float CHORD_Bm[] = { NOTE_B3, NOTE_D4, NOTE_FS4, NOTE_B4 };
static const float CHORD_FSm[]= { NOTE_FS3, NOTE_A3, NOTE_FS4, NOTE_A4 };
static const float CHORD_G[]  = { NOTE_G3, NOTE_B3, NOTE_D4, NOTE_G4 };

static const float* CHILL_PROGRESSION[] = {
    CHORD_D, CHORD_A, CHORD_Bm, CHORD_FSm,
    CHORD_G, CHORD_D, CHORD_G, CHORD_A
};

// Hardcore: Am - F - C - G (epic minor progression)
static const float CHORD_Am[] = { NOTE_A2, NOTE_C3, NOTE_E3, NOTE_A3H };
static const float CHORD_F[]  = { NOTE_F3, NOTE_A3H, NOTE_C4, NOTE_F4 };
static const float CHORD_C[]  = { NOTE_C3, NOTE_E3, NOTE_G3H, NOTE_C4 };
static const float CHORD_Gh[] = { NOTE_G3H, NOTE_B3, NOTE_D4, NOTE_G4H };

static const float* HARDCORE_PROGRESSION[] = {
    CHORD_Am, CHORD_F, CHORD_C, CHORD_Gh
};

#define NUM_CHORDS_CHILL 8
#define NUM_CHORDS_HARDCORE 4
#define NOTES_PER_CHORD 4

// === AUDIO STATE ===
static AudioStream stream;
static bool music_playing = false;
static bool hardcore_mode = false;
static float streak_timer = 0.0f;  // Time since last green hit

// Phase accumulators
static float lead_phase = 0.0f;
static float lead_phase2 = 0.0f;
static float pad_phase[4] = { 0.0f };
static float bass_phase = 0.0f;
static float lfo_phase = 0.0f;
static float kick_phase = 0.0f;

// Musical state
static int current_chord = 0;
static int current_note = 0;
static float note_envelope = 1.0f;
static float kick_envelope = 0.0f;

// Timing
static float samples_per_note;
static float samples_per_chord;
static float samples_per_beat;
static float note_counter = 0.0f;
static float chord_counter = 0.0f;
static float beat_counter = 0.0f;

// Current BPM (changes between modes)
static float current_bpm;

// Recalculate timing based on BPM
static void recalc_timing(float bpm)
{
    current_bpm = bpm;
    float beat_duration = 60.0f / bpm;
    samples_per_beat = SAMPLE_RATE * beat_duration;
    samples_per_note = samples_per_beat / 4.0f;   // 16th notes
    samples_per_chord = samples_per_beat * 2.0f;  // 2 beats per chord
}

// Audio callback
static void audio_fill_buffer(void *buffer, unsigned int frames)
{
    short *samples = (short *)buffer;

    int num_chords = hardcore_mode ? NUM_CHORDS_HARDCORE : NUM_CHORDS_CHILL;
    const float** progression = hardcore_mode ? HARDCORE_PROGRESSION : CHILL_PROGRESSION;

    for (unsigned int i = 0; i < frames; i++) {
        const float* chord = progression[current_chord % num_chords];
        float sample = 0.0f;

        // Update timing
        note_counter += 1.0f;
        chord_counter += 1.0f;
        beat_counter += 1.0f;

        // Beat tick (for kick drum)
        if (beat_counter >= samples_per_beat) {
            beat_counter -= samples_per_beat;
            if (hardcore_mode) {
                kick_envelope = 1.0f;  // Trigger kick
            }
        }

        // Arpeggio note change
        if (note_counter >= samples_per_note) {
            note_counter -= samples_per_note;
            current_note = (current_note + 1) % NOTES_PER_CHORD;
            note_envelope = 1.0f;
        }

        // Chord change
        if (chord_counter >= samples_per_chord) {
            chord_counter -= samples_per_chord;
            current_chord = (current_chord + 1) % num_chords;
        }

        // Envelope decay (faster in hardcore mode)
        float decay = hardcore_mode ? 0.9999f : 0.99993f;
        note_envelope *= decay;
        float env = note_envelope * 0.6f + 0.4f;

        // LFO
        float lfo_speed = hardcore_mode ? 0.8f : 0.3f;
        lfo_phase += lfo_speed / SAMPLE_RATE;
        if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;
        float lfo = sinf(2.0f * PI * lfo_phase) * 0.5f + 0.5f;

        // === LEAD SYNTH ===
        float lead_freq = chord[current_note];
        float lead_inc = lead_freq / SAMPLE_RATE;

        float lead;
        if (hardcore_mode) {
            // Aggressive saw-ish lead
            float saw = lead_phase * 2.0f - 1.0f;
            lead = saw * 0.2f;
            lead += sinf(2.0f * PI * lead_phase2 * 2.0f) * 0.1f;  // Octave up
        } else {
            // Clean sine lead
            lead = sinf(2.0f * PI * lead_phase) * 0.22f;
            lead += sinf(2.0f * PI * lead_phase2) * 0.12f;
        }
        lead *= env;

        lead_phase += lead_inc;
        lead_phase2 += lead_inc * (hardcore_mode ? 1.005f : 1.002f);
        if (lead_phase >= 1.0f) lead_phase -= 1.0f;
        if (lead_phase2 >= 1.0f) lead_phase2 -= 1.0f;

        // === PAD SYNTH ===
        float pad = 0.0f;
        float pad_vol = hardcore_mode ? 0.025f : 0.035f;
        for (int j = 0; j < NOTES_PER_CHORD; j++) {
            float pad_freq = chord[j] * 0.5f;
            float pad_inc = pad_freq / SAMPLE_RATE;
            pad += sinf(2.0f * PI * pad_phase[j]) * pad_vol;
            pad_phase[j] += pad_inc;
            if (pad_phase[j] >= 1.0f) pad_phase[j] -= 1.0f;
        }
        pad *= (0.85f + lfo * 0.15f);

        // === BASS ===
        float bass_freq = chord[0] * (hardcore_mode ? 0.5f : 0.25f);
        float bass_inc = bass_freq / SAMPLE_RATE;
        float bass;
        if (hardcore_mode) {
            // Pulsing bass with sub
            float pulse = (bass_phase < 0.5f) ? 1.0f : -1.0f;
            bass = pulse * 0.15f;
            bass += sinf(2.0f * PI * bass_phase * 0.5f) * 0.2f;  // Sub octave
        } else {
            bass = sinf(2.0f * PI * bass_phase) * 0.25f;
        }
        bass_phase += bass_inc;
        if (bass_phase >= 1.0f) bass_phase -= 1.0f;

        // === KICK DRUM (hardcore only) ===
        float kick = 0.0f;
        if (hardcore_mode && kick_envelope > 0.01f) {
            // Pitch-dropping sine kick
            float kick_freq = 60.0f + kick_envelope * 100.0f;
            kick = sinf(2.0f * PI * kick_phase) * kick_envelope * 0.4f;
            kick_phase += kick_freq / SAMPLE_RATE;
            if (kick_phase >= 1.0f) kick_phase -= 1.0f;
            kick_envelope *= 0.997f;  // Fast decay
        }

        // === MIX ===
        sample = lead + pad + bass + kick;

        // Saturation (more aggressive in hardcore)
        float drive = hardcore_mode ? 1.5f : 1.2f;
        sample = tanhf(sample * drive) * 0.8f;

        // Convert to 16-bit
        samples[i] = (short)(sample * 24000.0f);
    }
}

void audio_init(void)
{
    InitAudioDevice();

    // Start in chill mode
    hardcore_mode = false;
    streak_timer = STREAK_TIMEOUT + 1.0f;  // Start expired
    recalc_timing(BPM_CHILL);

    // Reset state
    current_chord = 0;
    current_note = 0;
    note_counter = 0.0f;
    chord_counter = 0.0f;
    beat_counter = 0.0f;
    note_envelope = 1.0f;
    kick_envelope = 0.0f;
    lead_phase = 0.0f;
    lead_phase2 = 0.0f;
    bass_phase = 0.0f;
    lfo_phase = 0.0f;
    kick_phase = 0.0f;
    for (int i = 0; i < 4; i++) pad_phase[i] = 0.0f;

    // Create stream with callback
    SetAudioStreamBufferSizeDefault(4096);
    stream = LoadAudioStream(SAMPLE_RATE, SAMPLE_SIZE, CHANNELS);
    SetAudioStreamCallback(stream, audio_fill_buffer);
    SetAudioStreamVolume(stream, 0.7f);

    PlayAudioStream(stream);
    music_playing = true;
}

void audio_update(void)
{
    if (!music_playing) return;

    float delta = GetFrameTime();

    // Update streak timer
    streak_timer += delta;

    // Check for mode transitions
    if (hardcore_mode && streak_timer > STREAK_TIMEOUT) {
        // Timeout - switch back to chill
        hardcore_mode = false;
        recalc_timing(BPM_CHILL);
        current_chord = 0;  // Reset progression
    }
}

void audio_cleanup(void)
{
    UnloadAudioStream(stream);
    CloseAudioDevice();
}

void audio_toggle_music(void)
{
    music_playing = !music_playing;
    if (music_playing) {
        ResumeAudioStream(stream);
    } else {
        PauseAudioStream(stream);
    }
}

bool audio_is_playing(void)
{
    return music_playing;
}

void audio_on_green_hit(void)
{
    streak_timer = 0.0f;  // Reset timer

    if (!hardcore_mode) {
        // Activate hardcore mode!
        hardcore_mode = true;
        recalc_timing(BPM_HARDCORE);
        current_chord = 0;
        kick_envelope = 1.0f;  // Trigger kick on activation
    }
}

void audio_on_adversary_hit(void)
{
    // Force back to chill mode
    if (hardcore_mode) {
        hardcore_mode = false;
        recalc_timing(BPM_CHILL);
        current_chord = 0;
    }
    streak_timer = STREAK_TIMEOUT + 1.0f;  // Reset streak
}

bool audio_is_hardcore(void)
{
    return hardcore_mode;
}
