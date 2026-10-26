// Vocoder.h — the band bank. Your voice is split into bands and each band's
// level is followed; the carrier is split into matching bands and each is set
// to that level. Each carrier band is first levelled out, so the result takes
// its tone from your voice rather than from the synth: words stay clear on any
// wave and any chord.
#pragma once

#include "Dsp.h"
#include <array>

namespace descant
{
struct VocoderSettings
{
    int   bands = 20;        // 8..40
    float lowHz = 90.0f, highHz = 9000.0f;
    float formant = 0.0f;    // semitones the carrier bands sit away from the voice bands, -12..12
    float width = 0.5f;      // 0 narrow, resonant bands .. 1 wide, smooth
    float react = 0.5f;      // 0 slow and smooth .. 1 fast and snappy
    float bright = 0.0f;     // -1..1 tilt, about ±4 dB per octave around 1 kHz
    float consonants = 0.5f; // 0..1: sibilance let through and noise on unvoiced sounds
    float spread = 0.4f;     // 0..1: alternate bands left and right
    bool  freeze = false;    // hold the band levels where they are
};

class Vocoder
{
public:
    static constexpr int kMaxBands = 40;

    void prepare (double sampleRate);
    void reset();

    // `mod` is your voice, `car` the carrier (mono). `unvoiced` 0..1 says how
    // much of what you're doing right now is breath and consonants. Adds the
    // result into outL and outR.
    void process (const float* mod, const float* car, float* outL, float* outR, int n,
                  const VocoderSettings&, float unvoiced, float gain);

    // Band levels for the panel, 0..1-ish, and the band centres.
    float bandLevel (int b) const noexcept { return shown[(size_t) b]; }
    float bandHz (int b) const noexcept    { return centre[(size_t) b]; }
    int   numBands() const noexcept        { return current.bands; }

private:
    void design (const VocoderSettings&);

    struct Band
    {
        Biquad m1, m2, c1, c2;
        Follower modEnv, carEnv;
        float tilt = 1.0f, panL = 1.0f, panR = 1.0f;
        bool live = true;
    };

    double sr = 48000.0;
    std::array<Band, kMaxBands> bands;
    std::array<float, kMaxBands> centre {}, shown {}, peak {};
    VocoderSettings current;
    bool designed = false;
    Biquad hiss1, hiss2;          // sibilance pass: the voice above ~5 kHz
    Follower carrierLevel;
    Rng noise { 5 };
    float noiseLp = 0.0f;
};
} // namespace descant
