#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

#include "Engine/Engine.h"
#include "Params.h"

// An audio effect that also takes MIDI: put it on the track your microphone is
// on and send it notes from a keyboard, or from Posy on a MIDI track.
class DescantProcessor final : public juce::AudioProcessor
{
public:
    DescantProcessor();
    ~DescantProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    // Factory presets double as the host's programs.
    int getNumPrograms() override;
    int getCurrentProgram() override { return juce::jmax (0, factoryIndex); }
    void setCurrentProgram (int index) override { loadFactory (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- parameters (message thread) ---------------------------------------------
    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const { return params.at (id)->load(); }
    int   geti (const char* id) const { return juce::roundToInt (getParam (id)); }

    // --- presets (message thread) --------------------------------------------------
    void loadFactory (int index);
    bool loadUserPreset (const juce::File&);
    bool saveUserPreset (const juce::String& name, juce::String& error);
    void stepPreset (int delta);              // through factory, then user presets
    juce::String presetName() const { return currentName; }
    juce::String presetBlurb() const;
    int  presetNumber() const;                // 1-based across factory and user; 0 if none
    bool presetEdited() const;

    // --- actions from the panel -----------------------------------------------------
    void panic() { panics.fetch_add (1); }    // let go of every note

    // --- read by the panel --------------------------------------------------------------
    struct Readout
    {
        float sung = 0.0f;
        bool singing = false, gateOpen = false, externalMissing = false, sidechainConnected = false;
        std::array<int, descant::kHarmonyVoices> targets {};
        std::bitset<128> sounding, chord;
        int carrierNote = -1;
        int numBands = 0;
        std::array<float, descant::Vocoder::kMaxBands> bands {};
        int midiCount = 0;
    };
    Readout readout();   // message thread; takes the band peaks
    float takeInputPeak()  { return uiIn.exchange (0.0f); }
    float takeOutputPeak() { return uiOut.exchange (0.0f); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void cacheParams();
    void readSettings();
    std::unique_ptr<juce::XmlElement> stateXml();
    void applyState (const juce::XmlElement&);
    std::vector<float> snapshot() const;

    std::map<juce::String, std::atomic<float>*> params;
    struct VoicePtrs { std::atomic<float>* interval; std::atomic<float>* level; std::atomic<float>* pan; };
    struct
    {
        std::atomic<float> *input, *gate, *source, *wave, *octave, *detune, *glide, *attack, *release, *velocity, *mono, *hold;
        std::atomic<float> *bands, *formant, *width, *react, *bright, *consonants, *spread, *lowHz, *highHz, *freeze;
        std::atomic<float> *harmonyMode, *key, *scale, *humanize, *harmonyFormant, *harmonyGlide;
        std::array<VoicePtrs, descant::kHarmonyVoices> voices;
        std::atomic<float> *doubleLevel, *doubleLoose, *doubleDetune, *doubleWidth;
        std::atomic<float> *dry, *vocoderLevel, *harmonyLevel, *output;
    } fast {};

    descant::Engine engine;
    descant::EngineSettings settings;
    std::vector<descant::MidiEvent> events;
    std::vector<float> silence, scratch;
    std::atomic<int> panics { 0 };
    int panicsSeen = 0;

    // Presets.
    int factoryIndex = 0;
    juce::File userFile;
    juce::String currentName;
    std::vector<float> loadedValues;

    // Audio thread -> panel.
    std::atomic<float> uiIn { 0.0f }, uiOut { 0.0f }, uiSung { 0.0f };
    std::atomic<bool> uiSinging { false }, uiGate { false }, uiExtMissing { false }, uiSidechain { false };
    std::array<std::atomic<int>, descant::kHarmonyVoices> uiTargets {};
    std::array<std::atomic<uint32_t>, 4> uiSounding {}, uiChord {};
    std::atomic<int> uiCarrierNote { -1 }, uiBands { 0 }, uiMidi { 0 };
    std::array<std::atomic<float>, descant::Vocoder::kMaxBands> uiBandLevels {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DescantProcessor)
};
