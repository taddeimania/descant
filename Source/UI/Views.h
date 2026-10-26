// Views.h — Descant's custom panel pieces: level meters, the band display,
// the keyboard strip and the LCD readouts.
#pragma once

#include "../PluginProcessor.h"
#include "Widgets.h"

namespace descant::ui
{
// Segmented LED meter with a falling peak, as on Mottle.
class Meter final : public juce::Component
{
public:
    void push (float peak);   // call from the panel timer
    void paint (juce::Graphics&) override;

private:
    float level = 0.0f, hold = 0.0f;
    int holdFrames = 0, shownSegs = -1, shownHold = -1;
};

// The vocoder's bands as amber bars, low on the left, with their range.
class BandDisplay final : public juce::Component, public juce::SettableTooltipClient
{
public:
    void push (const DescantProcessor::Readout&, bool frozen, float lowHz, float highHz, float formant);
    void paint (juce::Graphics&) override;

private:
    std::array<float, Vocoder::kMaxBands> level {};
    int bands = 20;
    bool frozen = false;
    float lo = 90.0f, hi = 9000.0f, formant = 0.0f;
};

// Five octaves of keys. Amber: keys sounding (held, or HOLD). Outlined: the
// remembered chord. Blue dot: the note you're singing. Coloured dots: where
// each harmony voice is singing.
class KeyboardView final : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int kLow = 36, kHigh = 96;   // C1..C6 in Live's naming

    // With the VOICE or EXTERNAL carrier the keys don't play the synth, so they
    // show as outlines.
    void push (const DescantProcessor::Readout&, int harmonyMode, int source, const std::array<float, kHarmonyVoices>& levels);
    void paint (juce::Graphics&) override;

private:
    juce::Rectangle<float> keyRect (int note) const;
    static bool isBlack (int note) { const int pc = note % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

    std::bitset<128> sounding, chord;
    int sung = -1, carrier = -1;
    std::array<int, kHarmonyVoices> targets {};
    juce::String key;
};

// A black LCD with a big line and a small line.
class LcdReadout final : public juce::Component, public juce::SettableTooltipClient
{
public:
    juce::String caption;          // small, top-left, dim
    float bigH = 30.0f;
    juce::Colour bigColour = colours::lcd;

    void set (const juce::String& big, const juce::String& small, bool led, juce::Colour ledColour);
    void paint (juce::Graphics&) override;

private:
    juce::String bigText, smallText;
    bool ledOn = false;
    juce::Colour ledCol = colours::ledGreen;
};
} // namespace descant::ui
