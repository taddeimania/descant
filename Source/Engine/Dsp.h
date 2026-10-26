// Dsp.h — small building blocks for Descant's signal path. No JUCE, so the
// engine tests build without it.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace descant
{
inline constexpr float kPi    = 3.14159265358979f;
inline constexpr float kTwoPi = 6.28318530717959f;

inline float clampf (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float dbToGain (float db)    { return db <= -80.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)     { return g <= 1.0e-5f ? -100.0f : 20.0f * std::log10 (g); }
inline float midiToHz (float note)  { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
inline float hzToMidi (float hz)    { return 69.0f + 12.0f * std::log2 (std::max (1.0f, hz) / 440.0f); }
inline float semisToRatio (float s) { return std::pow (2.0f, s / 12.0f); }

// Exponential map of 0..1 onto lo..hi (both > 0).
inline float expMap (float x, float lo, float hi) { return lo * std::pow (hi / lo, clampf (x, 0.0f, 1.0f)); }

// One-pole coefficient for a time constant in milliseconds.
inline float onePoleCoef (float ms, double sr)
{
    if (ms <= 0.0f) return 0.0f;
    return (float) std::exp (-1.0 / (0.001 * (double) ms * sr));
}

// ---------------------------------------------------------------------------
// Biquad, transposed direct form II. RBJ cookbook designs.
// ---------------------------------------------------------------------------
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0.0f; }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setNormalised (double B0, double B1, double B2, double A0, double A1, double A2)
    {
        b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
        a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
    }

    // Band-pass with 0 dB at the centre.
    void bandpass (double fc, double q, double sr)
    {
        fc = std::min (fc, sr * 0.49);
        const double w = 2.0 * 3.141592653589793 * fc / sr;
        const double alpha = std::sin (w) / (2.0 * q);
        setNormalised (alpha, 0.0, -alpha, 1.0 + alpha, -2.0 * std::cos (w), 1.0 - alpha);
    }
    void lowpass (double fc, double q, double sr)
    {
        fc = std::min (fc, sr * 0.49);
        const double w = 2.0 * 3.141592653589793 * fc / sr;
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        setNormalised ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }
    void highpass (double fc, double q, double sr)
    {
        fc = std::min (fc, sr * 0.49);
        const double w = 2.0 * 3.141592653589793 * fc / sr;
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        setNormalised ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }
    // Peaking EQ.
    void peak (double fc, double q, double db, double sr)
    {
        fc = std::min (fc, sr * 0.49);
        const double A = std::pow (10.0, db / 40.0);
        const double w = 2.0 * 3.141592653589793 * fc / sr;
        const double alpha = std::sin (w) / (2.0 * q), c = std::cos (w);
        setNormalised (1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
    }
};

// ---------------------------------------------------------------------------
// Random numbers: a fast xorshift, and slowly wandering noise for drift.
// ---------------------------------------------------------------------------
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    explicit Rng (uint32_t seed = 1) : s (seed * 2654435761u + 12345u) { if (s == 0) s = 1; }
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni()     { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    inline float bi()      { return uni() * 2.0f - 1.0f; }                              // -1..1
};

// Smoothed random walk in -1..1 that changes roughly `hz` times a second.
struct Drift
{
    Rng rng;
    float value = 0, target = 0, step = 0;
    int left = 0;

    explicit Drift (uint32_t seed = 3) : rng (seed) {}

    // Advance by n samples.
    float advance (int n, float hz, double sr)
    {
        left -= n;
        if (left <= 0)
        {
            target = rng.bi();
            left = std::max (1, (int) (sr / std::max (0.05f, hz) * (0.6f + 0.8f * rng.uni())));
            step = (target - value) / (float) left;
        }
        value += step * (float) n;
        return value;
    }
};

// ---------------------------------------------------------------------------
// PolyBLEP residual for band-limited saw and square.
// ---------------------------------------------------------------------------
inline float polyBlep (float t, float dt)
{
    if (t < dt)       { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// Envelope follower with separate attack and release.
struct Follower
{
    float env = 0, att = 0, rel = 0;
    void set (float attackMs, float releaseMs, double sr) { att = onePoleCoef (attackMs, sr); rel = onePoleCoef (releaseMs, sr); }
    inline float process (float x)
    {
        x = std::abs (x);
        const float c = x > env ? att : rel;
        env = x + c * (env - x);
        return env;
    }
};
} // namespace descant
