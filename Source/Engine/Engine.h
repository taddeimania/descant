// Engine.h — Descant's whole signal path, without JUCE.
//
//   mic ─ INPUT ─ GATE ─┬─────────────────────────────── DRY ──────┐
//                       ├─ pitch ─ HARMONY (4 voices) ── HARMONY ──┤
//                       ├─ DOUBLE (2 voices) ─────────── DOUBLE ───┼─ OUTPUT
//                       └─ VOCODER ◄─ CARRIER ────────── VOCODER ──┘
//                                     ▲ MIDI keys, your own pitch, or the sidechain
#pragma once

#include "Carrier.h"
#include "Music.h"
#include "PitchTracker.h"
#include "Shifter.h"
#include "Vocoder.h"

#include <array>
#include <bitset>
#include <vector>

namespace descant
{
enum Source { KeysSource = 0, VoiceSource, ExternalSource, NumSources };
inline constexpr int kHarmonyVoices = 4;

struct HarmonyVoice
{
    int   interval = Up1;
    float level = 0.0f;   // linear gain
    float pan = 0.0f;     // -1..1
};

struct EngineSettings
{
    // Input
    float inputDb = 0.0f;
    float gateDb = -55.0f;      // -80 = off

    // Carrier
    int  source = KeysSource;
    CarrierSettings carrier;
    bool mono = false, hold = false;

    // Vocoder
    VocoderSettings vocoder;

    // Harmony
    int harmonyMode = ChordMode, key = 0, scale = Major;
    std::array<HarmonyVoice, kHarmonyVoices> voices {};
    float humanize = 0.3f;        // 0..1
    float harmonyFormant = 0.0f;  // semitones, -6..6
    float harmonyGlideMs = 40.0f;

    // Double
    float doubleLevel = 0.0f;     // linear
    float doubleLoose = 0.3f;     // 0..1
    float doubleDetune = 0.3f;    // 0..1 -> 0..25 cents
    float doubleWidth = 0.8f;     // 0..1

    // Mix (linear gains)
    float dry = 1.0f, vocoderLevel = 1.0f, harmonyLevel = 1.0f, output = 1.0f;
};

struct MidiEvent
{
    enum Type : uint8_t { NoteOn, NoteOff, Bend, Sustain, AllOff };
    int offset = 0;     // sample in the block
    Type type = NoteOn;
    uint8_t note = 0;
    float value = 0.0f; // velocity 0..1, bend -1..1, sustain 0/1
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock);
    void reset();

    // Mono or stereo in (inR may be null), optional sidechain (null if none).
    void process (const float* inL, const float* inR, const float* scL, const float* scR,
                  float* outL, float* outR, int n, const EngineSettings&,
                  const MidiEvent* events, int numEvents);

    // --- read by the panel (copy out after each block) --------------------------
    float sungNote() const noexcept         { return sungFrac; }    // fractional MIDI note, last voiced
    bool  singing() const noexcept          { return isSinging; }   // voiced and above the gate
    int   harmonyTarget (int v) const noexcept { return targets[(size_t) v]; }
    const std::bitset<128>& soundingNotes() const noexcept { return soundingMask; }
    const std::bitset<128>& chordNotes() const noexcept    { return chordMask; }
    int   carrierNote() const noexcept      { return followNote; }  // VOICE source: what the synth plays
    float inputPeak() const noexcept        { return inPeak; }
    float outputPeak() const noexcept       { return outPeak; }
    bool  gateOpen() const noexcept         { return gateIsOpen; }
    const Vocoder& bank() const noexcept    { return vocoder; }
    bool  externalMissing() const noexcept  { return noSidechain; }

private:
    void handle (const MidiEvent&, const EngineSettings&);
    void updateCarrierNotes (const EngineSettings&);
    void runChunk (const float* inL, const float* inR, const float* scL, const float* scR,
                   float* outL, float* outR, int n, const EngineSettings&);

    double sr = 48000.0;
    int maxChunk = 64;

    PitchTracker tracker;
    GrainSource source;
    std::array<ShiftVoice, kHarmonyVoices> harmony;
    std::array<ShiftVoice, 2> doubles;
    Carrier carrier;
    Vocoder vocoder;
    NoteState notes;

    Biquad hpf1, hpf2;
    Follower gateLevel;
    float gateGain = 0.0f;
    bool gateIsOpen = false;

    // MIDI
    bool sustain = false;
    std::bitset<128> sustained;
    float bend = 0.0f;
    int carrierVersion = -1, carrierFollow = -2;
    bool carrierMono = false;
    int carrierSource = -1;
    std::vector<float> velocityScratch;

    // Pitch
    float sungFrac = 0.0f;
    int sungQuant = -1, followNote = -1;
    bool isSinging = false;
    float unvoiced = 0.0f;
    float silentMs = 1000.0f;

    // Harmony state
    std::array<float, kHarmonyVoices> semis {}, voiceGain {};
    std::array<int, kHarmonyVoices> targets {};
    std::array<Drift, kHarmonyVoices> pitchDrift, timeDrift;
    std::array<Drift, 2> doubleDrift, doubleTime;
    HarmonyContext ctx;
    std::array<float, 2> doubleGain {};

    // Buffers
    std::vector<float> mono, gated, car, vocL, vocR, tmp, harmL, harmR, dblL, dblR;

    std::bitset<128> soundingMask, chordMask;
    float inPeak = 0.0f, outPeak = 0.0f;
    bool noSidechain = false;
    bool freezeArmed = false;
};
} // namespace descant
