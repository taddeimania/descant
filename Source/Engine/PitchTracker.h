// PitchTracker.h — finds the pitch you're singing. YIN on a decimated copy of
// the input (about 11 kHz), every few milliseconds, 70 Hz to 1.1 kHz.
#pragma once

#include "Dsp.h"
#include <vector>

namespace descant
{
class PitchTracker
{
public:
    static constexpr float kMinHz = 70.0f;
    static constexpr float kMaxHz = 1100.0f;

    void prepare (double sampleRate);
    void reset();

    // Feed input samples. `gateOpen` says the singer is loud enough to count.
    void push (const float* x, int n, bool gateOpen);

    bool  voiced() const noexcept     { return isVoiced; }
    float period() const noexcept     { return periodSamples; }        // full-rate samples; last voiced value
    float frequency() const noexcept  { return periodSamples > 0.0f ? (float) sr / periodSamples : 0.0f; }
    float midiNote() const noexcept   { return periodSamples > 0.0f ? hzToMidi (frequency()) : 0.0f; }
    float clarity() const noexcept    { return lastClarity; }           // 0..1, 1 = clean periodic

private:
    void analyse();

    double sr = 48000.0, dsr = 12000.0;
    int factor = 4, phase = 0;
    Biquad aa1, aa2;
    std::vector<float> ring;     // decimated history
    int ringPos = 0;
    int window = 256, minLag = 10, maxLag = 170, hop = 48, sinceHop = 0;
    std::vector<float> frame, diff;

    bool isVoiced = false, gate = false;
    float periodSamples = 0.0f, lastClarity = 0.0f;
    float history[3] {};
    int unvoicedRun = 0, voicedRun = 0;
};
} // namespace descant
