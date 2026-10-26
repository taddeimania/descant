// TestSignals.h — a synthetic singer for the tests and the snapshot tool:
// a glottal pulse train through three vowel formants, with vibrato, and
// noise for consonants. No JUCE.
#pragma once

#include "Engine/Dsp.h"
#include <vector>

namespace descant::test
{
struct Vowel { float f1, f2, f3; };
inline constexpr Vowel kAh { 730.0f, 1090.0f, 2440.0f };
inline constexpr Vowel kEe { 270.0f, 2290.0f, 3010.0f };
inline constexpr Vowel kOo { 300.0f, 870.0f, 2240.0f };

class Singer
{
public:
    explicit Singer (double sampleRate, uint32_t seed = 9) : sr (sampleRate), rng (seed) { setVowel (kAh); }

    void setVowel (Vowel v)
    {
        f[0].bandpass (v.f1, 8.0, sr);
        f[1].bandpass (v.f2, 10.0, sr);
        f[2].bandpass (v.f3, 12.0, sr);
    }

    // One sample at `hz` (0 = consonant noise only), `level` ~ peak amplitude.
    float next (float hz, float level, float noise = 0.0f, float vibratoCents = 0.0f)
    {
        vib += 5.3 / sr;
        if (vib >= 1.0) vib -= 1.0;
        float x = 0.0f;
        if (hz > 0.0f)
        {
            const float f0 = hz * std::pow (2.0f, vibratoCents / 1200.0f * std::sin ((float) vib * kTwoPi));
            phase += f0 / (float) sr;
            if (phase >= 1.0f) phase -= 1.0f;
            // Rosenberg-style glottal flow; its slope is the excitation.
            const float open = 0.6f;
            float g = 0.0f;
            if (phase < open * 0.66f)      g = 0.5f * (1.0f - std::cos (kPi * phase / (open * 0.66f)));
            else if (phase < open)         g = std::cos (0.5f * kPi * (phase - open * 0.66f) / (open * 0.34f));
            x = g - last;
            last = g;
            x *= 6.0f;
        }
        x += noise * rng.bi() * 0.3f;
        const float y = f[0].process (x) * 1.0f + f[1].process (x) * 0.6f + f[2].process (x) * 0.35f + x * 0.05f;
        return y * level * 2.2f;
    }

private:
    double sr;
    Rng rng;
    Biquad f[3];
    float phase = 0.0f, last = 0.0f;
    double vib = 0.0;
};

inline float rms (const float* x, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; ++i) s += (double) x[i] * x[i];
    return n > 0 ? (float) std::sqrt (s / n) : 0.0f;
}
} // namespace descant::test
