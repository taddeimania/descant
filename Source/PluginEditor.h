#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "Settings.h"
#include "UI/Views.h"
#include "UI/Widgets.h"

// The panel is laid out in a fixed design space and scaled to fit the window,
// as on Beadwork, Posy, Seyir and Mottle.
class DescantPanel final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designW = 1500;
    static constexpr int designH = 940;

    explicit DescantPanel (DescantProcessor&);
    ~DescantPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override {}

    std::function<void (bool)> onTooltips;
    void syncTooltips();

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using Knob = descant::ui::Knob;
    using PanelButton = descant::ui::PanelButton;
    using ChoiceDisplay = descant::ui::ChoiceDisplay;

    struct VoiceStrip
    {
        ChoiceDisplay interval;
        Knob level, pan;
    };

    void timerCallback() override;

    void addKnob (Knob&, const char* id, const juce::String& caption, juce::Rectangle<int>, const juce::String& tip);
    void addToggle (PanelButton&, const char* id, juce::Rectangle<int>, const juce::String& tip);
    void addButton (PanelButton&, juce::Rectangle<int>, const juce::String& tip,
                    std::function<void (const juce::ModifierKeys&)> click);
    void addChoice (ChoiceDisplay&, const char* id, const juce::String& caption, juce::Rectangle<int>, const juce::String& tip);
    void addRadio (PanelButton&, const char* id, int value, juce::Rectangle<int>, const juce::String& tip);
    void chooseParam (juce::Component& target, const char* id);
    void chooseInterval (int voice);

    void openPresetMenu();
    void savePresetDialog();
    void updateStatus (const DescantProcessor::Readout&);

    DescantProcessor& proc;
    descant::ui::PanelLookAndFeel lnf;
    descant::Settings settings;

    // Header
    ChoiceDisplay presetChoice, keyChoice, scaleChoice;
    PanelButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, tipsBtn { "TIPS" };

    // Input
    Knob inputKnob, gateKnob;
    descant::ui::Meter inMeter, outMeter;
    descant::ui::LcdReadout pitchLcd;

    // Carrier
    std::array<PanelButton, descant::NumSources> sourceBtns;
    std::array<PanelButton, descant::NumWaves> waveBtns;
    Knob detuneKnob, glideKnob, attackKnob, releaseKnob, velocityKnob;
    ChoiceDisplay octaveChoice;
    PanelButton monoBtn { "MONO" }, holdBtn { "HOLD" }, allOffBtn { "ALL OFF" };

    // Mix
    Knob dryKnob, vocoderKnob, harmonyKnob, outputKnob;

    // Notes
    descant::ui::LcdReadout chordLcd;
    descant::ui::KeyboardView keyboard;

    // Vocoder
    descant::ui::BandDisplay bandDisplay;
    Knob formantKnob, widthKnob, reactKnob, brightKnob, consonantKnob, spreadKnob, lowKnob, highKnob;
    ChoiceDisplay bandsChoice;
    PanelButton freezeBtn { "FREEZE" };

    // Harmony
    std::array<PanelButton, descant::NumHarmonyModes> modeBtns;
    std::array<VoiceStrip, descant::kHarmonyVoices> strips;
    Knob humanizeKnob, hFormantKnob, hGlideKnob;

    // Double
    Knob dLevelKnob, dLooseKnob, dDetuneKnob, dWidthKnob;

    struct Radio { PanelButton* button; const char* id; int value; };
    std::vector<Radio> radios;
    std::vector<std::pair<PanelButton*, const char*>> toggles;
    std::vector<ChoiceDisplay*> choices;
    std::vector<std::unique_ptr<SliderAttachment>> attachments;
    std::unique_ptr<juce::AlertWindow> saveWindow;

    int tooltipsApplied = -1;
    int lastMidiCount = 0, midiFlash = 0;
    juce::String carrierStatus, mixHint, blurb, modeText;
    std::array<int, descant::kHarmonyVoices> shownTargets {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DescantPanel)
};

class DescantEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DescantEditor (DescantProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    DescantPanel panel;
    std::unique_ptr<juce::TooltipWindow> tooltips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DescantEditor)
};
