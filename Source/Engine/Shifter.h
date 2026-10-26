// Shifter.h — pitch-synchronous grain shifting (PSOLA) for the harmony and
// double voices.
//
// GrainSource keeps the recent input and cuts it into grains one pitch period
// apart while you sing (5 ms apart on consonants and breaths). Each ShiftVoice
// lays those grains back down at its own spacing: closer for a higher voice,
// further apart for a lower one. The grains keep the shape of your mouth and
// throat, so a shifted voice keeps your vowels instead of turning into a
// chipmunk; FORMANT squeezes or stretches each grain to make it sound like a
// different singer.
#pragma once

#include "Dsp.h"
#include <array>
#include <vector>

namespace descant
{
class GrainSource
{
public:
    struct Mark
    {
        int64_t pos = 0;
        float half = 0.0f;   // half the grain length: one pitch period when voiced
        bool voiced = false;
    };

    void prepare (double sampleRate);
    void reset();

    // One input sample. `period` is the pitch period in samples, 0 if unvoiced.
    void write (float x, float period);

    int64_t written() const noexcept { return count; }
    float at (double pos) const noexcept;   // linear interpolation; 0 outside history
    float maxHalf() const noexcept { return maxHalfLen; }

    // The mark nearest `target` whose grain (half * reach either side) is
    // already recorded by `limit`. False if there's none.
    bool pick (double target, double limit, float reach, Mark& out) const;

private:
    static constexpr int kMarks = 512;
    std::vector<float> buf;
    int64_t mask = 0, count = 0;
    std::array<Mark, kMarks> marks {};
    int head = 0, numMarks = 0;
    double nextMark = 0.0;
    float unvoicedHalf = 240.0f, maxHalfLen = 700.0f;
    double sr = 48000.0;
};

class ShiftVoice
{
public:
    struct Control
    {
        float ratio = 1.0f;      // pitch ratio, 0.25..4
        float formant = 1.0f;    // grain read speed: >1 brighter, smaller singer
        float delayMs = 0.0f;    // extra lag behind the input, for doubles and humanize
    };

    void prepare (double sampleRate);
    void reset();

    // Adds `n` samples to `out`. Input sample `t0 + i` must already be written.
    void render (const GrainSource&, int64_t t0, int n, const Control&, float* out, float gain);

    float latencyMs() const noexcept { return lag / (float) sr * 1000.0f; }

private:
    void addGrain (const GrainSource&, const GrainSource::Mark&, double centre, float formant, float gain, int64_t now);

    std::vector<float> acc;
    int64_t mask = 0;
    double nextSynth = -1.0;
    float lag = 0.0f;
    double sr = 48000.0;
    bool started = false;
};
} // namespace descant
