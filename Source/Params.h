// Params.h — Descant's host parameters. Every knob, button and choice is
// automatable and can be MIDI-mapped in Live.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/Engine.h"

namespace descant::ids
{
// Input
inline constexpr const char* input = "input";
inline constexpr const char* gate  = "gate";

// Carrier
inline constexpr const char* source   = "source";
inline constexpr const char* wave     = "wave";
inline constexpr const char* octave   = "octave";
inline constexpr const char* detune   = "detune";
inline constexpr const char* glide    = "glide";
inline constexpr const char* attack   = "attack";
inline constexpr const char* release  = "release";
inline constexpr const char* velocity = "velocity";
inline constexpr const char* mono     = "mono";
inline constexpr const char* hold     = "hold";

// Vocoder
inline constexpr const char* bands      = "bands";
inline constexpr const char* formant    = "formant";
inline constexpr const char* width      = "width";
inline constexpr const char* react      = "react";
inline constexpr const char* bright     = "bright";
inline constexpr const char* consonants = "consonants";
inline constexpr const char* spread     = "spread";
inline constexpr const char* lowHz      = "lowHz";
inline constexpr const char* highHz     = "highHz";
inline constexpr const char* freeze     = "freeze";

// Harmony
inline constexpr const char* harmonyMode    = "harmonyMode";
inline constexpr const char* key            = "key";
inline constexpr const char* scale          = "scale";
inline constexpr const char* humanize       = "humanize";
inline constexpr const char* harmonyFormant = "harmonyFormant";
inline constexpr const char* harmonyGlide   = "harmonyGlide";
inline const char* voiceInterval (int v) { static const char* n[] { "v1Interval", "v2Interval", "v3Interval", "v4Interval" }; return n[juce::jlimit (0, 3, v)]; }
inline const char* voiceLevel (int v)    { static const char* n[] { "v1Level", "v2Level", "v3Level", "v4Level" }; return n[juce::jlimit (0, 3, v)]; }
inline const char* voicePan (int v)      { static const char* n[] { "v1Pan", "v2Pan", "v3Pan", "v4Pan" }; return n[juce::jlimit (0, 3, v)]; }

// Double
inline constexpr const char* doubleLevel  = "doubleLevel";
inline constexpr const char* doubleLoose  = "doubleLoose";
inline constexpr const char* doubleDetune = "doubleDetune";
inline constexpr const char* doubleWidth  = "doubleWidth";

// Mix
inline constexpr const char* dry          = "dry";
inline constexpr const char* vocoderLevel = "vocoderLevel";
inline constexpr const char* harmonyLevel = "harmonyLevel";
inline constexpr const char* output       = "output";

// ---------------------------------------------------------------------------
inline const juce::StringArray& sourceNames()  { static const juce::StringArray n { "KEYS", "VOICE", "EXTERNAL" }; return n; }
inline const juce::StringArray& waveNames()    { static const juce::StringArray n { "SAW", "SQUARE", "SUPER", "CHOIR" }; return n; }
inline const juce::StringArray& octaveNames()  { static const juce::StringArray n { "-2 OCT", "-1 OCT", "0", "+1 OCT", "+2 OCT" }; return n; }
inline const juce::StringArray& modeNames()    { static const juce::StringArray n { "CHORD", "SCALE", "NOTES" }; return n; }
inline const juce::StringArray& keyNames()     { static const juce::StringArray n { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return n; }
inline const juce::StringArray& bandCounts()   { static const juce::StringArray n { "8", "12", "16", "20", "24", "32", "40" }; return n; }
inline int bandCount (int index)               { static const int c[] { 8, 12, 16, 20, 24, 32, 40 }; return c[juce::jlimit (0, 6, index)]; }
inline juce::StringArray scaleNames()
{
    juce::StringArray n;
    for (int s = 0; s < NumScales; ++s) n.add (descant::scaleName (s));
    return n;
}
inline juce::StringArray intervalNames()
{
    // Host-side names; the panel relabels them for the mode (3RD UP in SCALE).
    juce::StringArray n;
    for (int i = 0; i < NumIntervals; ++i) n.add (intervalLabel (i, ChordMode));
    return n;
}

inline juce::String percent (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
inline juce::String decibels (float v)
{
    if (v <= -59.95f) return "OFF";
    const float r = std::round (v * 10.0f) / 10.0f;
    return (r > 0.0f ? "+" : "") + juce::String (r, 1) + " dB";
}
inline juce::String bipolar (float v)
{
    const int p = juce::roundToInt (v * 100.0f);
    return p == 0 ? juce::String ("centre") : (p > 0 ? "+" : "") + juce::String (p);
}
inline juce::String pan (float v)
{
    const int p = juce::roundToInt (v * 100.0f);
    return p == 0 ? juce::String ("C") : juce::String (std::abs (p)) + (p < 0 ? " L" : " R");
}
inline juce::String millis (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String (juce::roundToInt (v)) + " ms";
}
inline juce::String hertz (float v)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
}
inline juce::String semitones (float v)
{
    const float r = std::round (v * 10.0f) / 10.0f;
    return std::abs (r) < 0.05f ? juce::String ("0 st") : (r > 0 ? "+" : "") + juce::String (r, 1) + " st";
}
} // namespace descant::ids

namespace descant
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto num = [] (const char* id, const char* name, NormalisableRange<float> range, float def,
                   std::function<String (float)> text)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([text] (float v, int) { return text (v); }));
    };
    auto pct = [&] (const char* id, const char* name, float def) { return num (id, name, { 0.0f, 1.0f }, def, ids::percent); };
    auto bip = [&] (const char* id, const char* name, float def) { return num (id, name, { -1.0f, 1.0f }, def, ids::bipolar); };
    auto db = [&] (const char* id, const char* name, float lo, float hi, float def)
    {
        NormalisableRange<float> r (lo, hi);
        if (lo <= -60.0f) r.setSkewForCentre (-12.0f);
        return num (id, name, r, def, ids::decibels);
    };
    auto ms = [&] (const char* id, const char* name, float lo, float hi, float centre, float def)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return num (id, name, r, def, ids::millis);
    };
    auto choice = [] (const char* id, const char* name, const StringArray& items, int def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def);
    };
    // On/off switches are two-step choices: they read back exactly what was
    // saved, which a bool parameter doesn't for in-between host values.
    auto toggle = [] (const char* id, const char* name, bool def)
    {
        return std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, StringArray { "Off", "On" }, def ? 1 : 0);
    };

    // Input
    layout.add (num (ids::input, "Input", { -12.0f, 24.0f }, 0.0f, ids::decibels));
    layout.add (num (ids::gate, "Gate", { -80.0f, -20.0f }, -58.0f,
                     [] (float v) { return v <= -79.5f ? String ("OFF") : String (juce::roundToInt (v)) + " dB"; }));

    // Carrier
    layout.add (choice (ids::source, "Carrier", ids::sourceNames(), KeysSource));
    layout.add (choice (ids::wave, "Wave", ids::waveNames(), Saw));
    layout.add (choice (ids::octave, "Octave", ids::octaveNames(), 2));
    layout.add (pct (ids::detune, "Detune", 0.35f));
    layout.add (ms (ids::glide, "Glide", 0.0f, 1000.0f, 120.0f, 0.0f));
    layout.add (ms (ids::attack, "Attack", 0.5f, 2000.0f, 60.0f, 4.0f));
    layout.add (ms (ids::release, "Release", 5.0f, 4000.0f, 300.0f, 140.0f));
    layout.add (pct (ids::velocity, "Velocity", 0.3f));
    layout.add (toggle (ids::mono, "Mono", false));
    layout.add (toggle (ids::hold, "Hold", false));

    // Vocoder
    layout.add (choice (ids::bands, "Bands", ids::bandCounts(), 3));
    layout.add (num (ids::formant, "Formant", { -12.0f, 12.0f }, 0.0f, ids::semitones));
    layout.add (pct (ids::width, "Width", 0.45f));
    layout.add (pct (ids::react, "React", 0.5f));
    layout.add (bip (ids::bright, "Bright", 0.15f));
    layout.add (pct (ids::consonants, "Consonants", 0.5f));
    layout.add (pct (ids::spread, "Spread", 0.35f));
    {
        NormalisableRange<float> lo (50.0f, 500.0f); lo.setSkewForCentre (150.0f);
        NormalisableRange<float> hi (2000.0f, 16000.0f); hi.setSkewForCentre (6000.0f);
        layout.add (num (ids::lowHz, "Low Band", lo, 90.0f, ids::hertz));
        layout.add (num (ids::highHz, "High Band", hi, 9000.0f, ids::hertz));
    }
    layout.add (toggle (ids::freeze, "Freeze", false));

    // Harmony
    layout.add (choice (ids::harmonyMode, "Harmony Mode", ids::modeNames(), ChordMode));
    layout.add (choice (ids::key, "Key", ids::keyNames(), 0));
    layout.add (choice (ids::scale, "Scale", ids::scaleNames(), Major));
    static const int defIntervals[] { Up1, Down1, Up2, OctDown };
    static const float defPan[] { -0.45f, 0.45f, -0.8f, 0.8f };
    for (int v = 0; v < kHarmonyVoices; ++v)
    {
        const String n = "Voice " + String (v + 1);
        layout.add (choice (ids::voiceInterval (v), (n + " Interval").toRawUTF8(), ids::intervalNames(), defIntervals[v]));
        layout.add (pct (ids::voiceLevel (v), (n + " Level").toRawUTF8(), 0.0f));
        layout.add (num (ids::voicePan (v), (n + " Pan").toRawUTF8(), { -1.0f, 1.0f }, defPan[v], ids::pan));
    }
    layout.add (pct (ids::humanize, "Humanize", 0.35f));
    layout.add (num (ids::harmonyFormant, "Harmony Formant", { -6.0f, 6.0f }, 0.0f, ids::semitones));
    layout.add (ms (ids::harmonyGlide, "Harmony Glide", 0.0f, 500.0f, 80.0f, 40.0f));

    // Double
    layout.add (pct (ids::doubleLevel, "Double", 0.0f));
    layout.add (pct (ids::doubleLoose, "Double Loose", 0.35f));
    layout.add (pct (ids::doubleDetune, "Double Detune", 0.35f));
    layout.add (pct (ids::doubleWidth, "Double Width", 0.8f));

    // Mix
    layout.add (db (ids::dry, "Dry", -60.0f, 6.0f, -60.0f));
    layout.add (db (ids::vocoderLevel, "Vocoder", -60.0f, 6.0f, 0.0f));
    layout.add (db (ids::harmonyLevel, "Harmony", -60.0f, 6.0f, 0.0f));
    layout.add (num (ids::output, "Output", { -24.0f, 12.0f }, 0.0f, ids::decibels));
    return layout;
}
} // namespace descant
