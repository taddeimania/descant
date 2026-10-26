// Carrier.h — the synth your voice shapes. Up to 16 voices, played from MIDI
// (or from the note you're singing), band-limited so high notes stay clean.
#pragma once

#include "Dsp.h"
#include <array>
#include <vector>

namespace descant
{
enum Wave { Saw = 0, Square, Super, Choir, NumWaves };

struct CarrierSettings
{
    int   wave = Saw;
    int   octave = 0;          // -2..2
    float detune = 0.3f;       // 0..1: unison spread on SUPER and CHOIR, drift on the others
    float glideMs = 0.0f;
    float attackMs = 4.0f;
    float releaseMs = 120.0f;
    float velocity = 0.3f;     // 0..1: how much key velocity sets the level
    float bend = 0.0f;         // semitones
};

class Carrier
{
public:
    static constexpr int kVoices = 16;

    void prepare (double sampleRate);
    void reset();

    // The notes that should sound, ascending, and their velocities (0..1).
    // `mono` plays only the first, gliding between notes without restarting.
    void setNotes (const int* notes, const float* velocities, int count, bool mono);

    void render (float* out, int n, const CarrierSettings&);

    int  activeVoices() const noexcept;
    bool sounding() const noexcept { return activeVoices() > 0; }

private:
    struct Voice
    {
        int note = -1;
        bool gate = false;
        float env = 0.0f, vel = 1.0f;
        float pitch = 60.0f, target = 60.0f;
        std::array<float, 5> phase {};
        Drift drift[3];
        uint32_t age = 0;
    };

    Voice* allocate();

    std::array<Voice, kVoices> voices;
    double sr = 48000.0;
    uint32_t clock = 0;
    float lastPitch = 60.0f;
    Rng rng { 11 };
};
} // namespace descant
