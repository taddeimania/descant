#include "PluginEditor.h"
#include "Presets.h"

using namespace descant;
using namespace descant::ui;
using juce::Rectangle;
using juce::String;

namespace
{
// Section boxes (design space).
const Rectangle<float> kInputBox   { 12.0f, 86.0f, 300.0f, 230.0f };
const Rectangle<float> kCarrierBox { 324.0f, 86.0f, 700.0f, 230.0f };
const Rectangle<float> kMixBox     { 1036.0f, 86.0f, 452.0f, 230.0f };
const Rectangle<float> kNotesBox   { 12.0f, 334.0f, 1476.0f, 100.0f };
const Rectangle<float> kVocoderBox { 12.0f, 452.0f, 1476.0f, 230.0f };
const Rectangle<float> kHarmonyBox { 12.0f, 700.0f, 1060.0f, 228.0f };
const Rectangle<float> kDoubleBox  { 1084.0f, 700.0f, 404.0f, 228.0f };

const Rectangle<float> kBlurb       { 548.0f, 61.0f, 500.0f, 16.0f };
const Rectangle<float> kCarrierText { 340.0f, 276.0f, 570.0f, 34.0f };
const Rectangle<float> kMixText     { 1052.0f, 230.0f, 380.0f, 70.0f };
const Rectangle<float> kVocoderText { 606.0f, 604.0f, 860.0f, 64.0f };
const Rectangle<float> kModeText    { 316.0f, 716.0f, 740.0f, 44.0f };
const Rectangle<float> kDoubleText  { 1100.0f, 716.0f, 372.0f, 60.0f };

const char* kModeBlurbs[] {
    "UP and DOWN count the notes of the chord you're holding (or held last). UP 1 is the next chord note above what you sing. "
    "With no chord yet, the voices follow KEY and SCALE.",
    "Each voice sings a fixed step in KEY and SCALE: a 3rd, 5th or 6th above you, or a 3rd, 4th or 6th below. No keys needed.",
    "Each voice sings one of the notes you hold, lowest first, in your voice. Play a chord and sing anything: you become the chord." };

const char* kSourceTips[] {
    "KEYS: the synth plays the notes you send it: a MIDI keyboard, a clip, or Posy on a MIDI track. Single notes or chords.",
    "VOICE: the synth plays the note you're singing, snapped to KEY and SCALE. A robot voice with no keyboard.",
    "EXTERNAL: your voice shapes another track. In Live, pick that track in Descant's sidechain (the triangle at the top of the device)." };

const char* kWaveTips[] {
    "SAW: bright and buzzy. The classic vocoder sound and the clearest words.",
    "SQUARE: hollow and woody. An octave up it's very disco.",
    "SUPER: five detuned saws for wide string-machine pads. DETUNE sets the spread.",
    "CHOIR: three saws per note that each drift a little, like singers. DETUNE sets how loose they are." };
} // namespace

// ===========================================================================
DescantPanel::DescantPanel (DescantProcessor& p) : proc (p)
{
    setLookAndFeel (&lnf);
    setSize (designW, designH);

    // --- header ------------------------------------------------------------------
    presetChoice.getText = [this]
    {
        const int n = proc.presetNumber();
        return (n > 0 ? String (n).paddedLeft ('0', 2) + "   " : String()) + proc.presetName().toUpperCase()
             + (proc.presetEdited() ? "  *" : "");
    };
    presetChoice.fontH = 18.0f;
    presetChoice.onOpen = [this] { openPresetMenu(); };
    presetChoice.setTooltip ("Presets. Click for the list, or step with < >. A * means you've changed it since loading.");
    addAndMakeVisible (presetChoice);
    presetChoice.setBounds (594, 12, 408, 46);
    choices.push_back (&presetChoice);

    addButton (prevBtn, { 548, 14, 40, 42 }, "Previous preset.", [this] (const juce::ModifierKeys&) { proc.stepPreset (-1); });
    addButton (nextBtn, { 1008, 14, 40, 42 }, "Next preset.", [this] (const juce::ModifierKeys&) { proc.stepPreset (1); });
    prevBtn.setFontHeight (18.0f);
    nextBtn.setFontHeight (18.0f);
    addButton (saveBtn, { 1062, 18, 66, 34 }, "Save these settings as a preset.",
               [this] (const juce::ModifierKeys&) { savePresetDialog(); });
    saveBtn.setFontHeight (11.0f);

    addChoice (keyChoice, ids::key, "KEY", { 1146, 8, 78, 54 },
               "The key for SCALE harmony, for the VOICE carrier, and for CHORD harmony before any chord has been played.");
    addChoice (scaleChoice, ids::scale, "SCALE", { 1230, 8, 150, 54 },
               "The scale that goes with KEY. CHROMATIC lets the VOICE carrier follow every semitone.");
    addButton (tipsBtn, { 1400, 18, 62, 34 }, "Show or hide these tips (remembered for every Descant).",
               [this] (const juce::ModifierKeys&)
               {
                   settings.setTooltipsEnabled (! settings.tooltipsEnabled());
                   syncTooltips();
               });
    tipsBtn.setFontHeight (11.0f);

    // --- input -------------------------------------------------------------------------
    addAndMakeVisible (inMeter);
    inMeter.setBounds (30, 112, 14, 186);
    inMeter.setInterceptsMouseClicks (false, false);
    addKnob (inputKnob, ids::input, "INPUT", { 56, 100, 96, 118 },
             "Microphone level into Descant. Sing at full voice and turn it until the meter's top yellow lights on loud notes.");
    inputKnob.bipolar = true;
    addKnob (gateKnob, ids::gate, "GATE", { 158, 100, 96, 118 },
             "Below this level the microphone is shut, so room noise and headphone spill don't get vocoded or harmonised. "
             "Raise it until the pitch display goes quiet when you stop singing.");
    addAndMakeVisible (pitchLcd);
    pitchLcd.caption = "YOU'RE SINGING";
    pitchLcd.bigH = 28.0f;
    pitchLcd.bigColour = colours::brand;
    pitchLcd.setBounds (56, 226, 242, 76);
    pitchLcd.setTooltip ("The note Descant hears you singing, and how far off the nearest note you are. The light is on while it hears a pitch.");

    // --- carrier -------------------------------------------------------------------------
    for (int s = 0; s < NumSources; ++s)
    {
        auto& b = sourceBtns[(size_t) s];
        b.setLabel (ids::sourceNames()[s]);
        b.setFontHeight (12.0f);
        addRadio (b, ids::source, s, { 340 + s * 98, 102, s == 2 ? 104 : 92, 34 }, kSourceTips[s]);
    }
    for (int w = 0; w < NumWaves; ++w)
    {
        auto& b = waveBtns[(size_t) w];
        b.setLabel (ids::waveNames()[w]);
        b.setFontHeight (12.0f);
        addRadio (b, ids::wave, w, { 656 + w * 90, 102, 84, 34 }, kWaveTips[w]);
    }
    addKnob (detuneKnob, ids::detune, "DETUNE", { 336, 156, 88, 112 }, "SUPER and CHOIR: how far apart their oscillators are. SAW and SQUARE: a little analog drift.");
    addKnob (glideKnob, ids::glide, "GLIDE", { 428, 156, 88, 112 }, "How long notes take to slide to a new pitch. Most useful with MONO.");
    addKnob (attackKnob, ids::attack, "ATTACK", { 520, 156, 88, 112 }, "How slowly a note fades in when you press a key.");
    addKnob (releaseKnob, ids::release, "RELEASE", { 612, 156, 88, 112 }, "How long a note rings on after you let go of the key.");
    addKnob (velocityKnob, ids::velocity, "VELOCITY", { 704, 156, 88, 112 }, "How much harder key presses play louder. All the way down, every note is the same level.");
    for (auto* k : { &detuneKnob, &glideKnob, &attackKnob, &releaseKnob, &velocityKnob })
        k->small = true;
    addChoice (octaveChoice, ids::octave, "OCTAVE", { 806, 166, 96, 56 }, "Moves the synth up or down an octave or two from the keys you play.");
    addToggle (monoBtn, ids::mono, { 918, 160, 92, 32 }, "One note at a time: the last key you press. Pair it with GLIDE for talk-box lines.");
    addToggle (holdBtn, ids::hold, { 918, 198, 92, 32 },
               "Keeps the chord sounding after you let go, so your hands are free. Press new keys to change it.");
    addButton (allOffBtn, { 918, 236, 92, 28 }, "Let go of every note, held or stuck.",
               [this] (const juce::ModifierKeys&) { proc.setParam (ids::hold, 0.0f); proc.panic(); });
    monoBtn.setFontHeight (12.0f);
    holdBtn.setFontHeight (12.0f);
    allOffBtn.setFontHeight (10.0f);

    // --- mix -------------------------------------------------------------------------------
    addKnob (dryKnob, ids::dry, "DRY", { 1052, 100, 92, 118 }, "Your own voice, untouched. Off for a pure vocoder; up for harmony and doubles.");
    addKnob (vocoderKnob, ids::vocoderLevel, "VOCODER", { 1148, 100, 92, 118 }, "Level of the vocoder.");
    addKnob (harmonyKnob, ids::harmonyLevel, "HARMONY", { 1244, 100, 92, 118 }, "Level of all four harmony voices together.");
    addKnob (outputKnob, ids::output, "OUTPUT", { 1340, 100, 92, 118 }, "Overall output level.");
    outputKnob.bipolar = true;
    addAndMakeVisible (outMeter);
    outMeter.setBounds (1452, 112, 14, 186);
    outMeter.setInterceptsMouseClicks (false, false);

    // --- notes -------------------------------------------------------------------------------
    addAndMakeVisible (chordLcd);
    chordLcd.caption = "CHORD";
    chordLcd.bigH = 30.0f;
    chordLcd.setBounds (28, 352, 210, 68);
    chordLcd.setTooltip ("The chord coming in over MIDI, from your keyboard or from Posy. The light flickers with every note that arrives.");
    addAndMakeVisible (keyboard);
    keyboard.setBounds (254, 350, 1218, 72);
    keyboard.setTooltip ("Amber keys are sounding. An outline is the remembered chord CHORD harmony uses after you let go. "
                         "The blue dot is you; the coloured dots are the harmony voices.");

    // --- vocoder -----------------------------------------------------------------------------
    addAndMakeVisible (bandDisplay);
    bandDisplay.setBounds (28, 472, 560, 194);
    bandDisplay.setTooltip ("Your voice, band by band, low on the left. This is the shape the vocoder puts on the synth.");
    addKnob (formantKnob, ids::formant, "FORMANT", { 606, 470, 96, 118 },
             "Moves the shape of your voice up or down without changing the notes: left is bigger and darker, right is smaller and brighter.");
    formantKnob.bipolar = true;
    addKnob (widthKnob, ids::width, "WIDTH", { 702, 470, 96, 118 }, "Band width. Narrow bands ring and whistle; wide bands are smooth and soft.");
    addKnob (reactKnob, ids::react, "REACT", { 798, 470, 96, 118 }, "How fast the bands follow you. Slow is smooth and pad-like; fast is crisp and gritty.");
    addKnob (brightKnob, ids::bright, "BRIGHT", { 894, 470, 96, 118 }, "Tilts the bands: left darker, right brighter and more present.");
    brightKnob.bipolar = true;
    addKnob (consonantKnob, ids::consonants, "CONSONANTS", { 990, 470, 96, 118 },
             "Lets S, T and other hissy sounds through, and turns breathy sounds into noise, so the words are easy to follow.");
    addKnob (spreadKnob, ids::spread, "SPREAD", { 1086, 470, 96, 118 }, "Sends alternate bands left and right for a wide stereo vocoder.");
    addKnob (lowKnob, ids::lowHz, "LOW", { 1182, 470, 96, 118 }, "The lowest band.");
    addKnob (highKnob, ids::highHz, "HIGH", { 1278, 470, 96, 118 }, "The highest band.");
    addChoice (bandsChoice, ids::bands, "BANDS", { 1388, 470, 88, 56 },
               "How many bands. Fewer is coarse and robotic; more is clearer and more like speech.");
    addToggle (freezeBtn, ids::freeze, { 1388, 540, 88, 44 },
               "Holds the vowel you're singing. Stop singing and play the keys: the frozen vowel sings them.");
    freezeBtn.setFontHeight (13.0f);

    // --- harmony ------------------------------------------------------------------------------
    for (int m = 0; m < NumHarmonyModes; ++m)
    {
        auto& b = modeBtns[(size_t) m];
        b.setLabel (ids::modeNames()[m]);
        b.setFontHeight (12.0f);
        addRadio (b, ids::harmonyMode, m, { 28 + m * 94, 720, 88, 34 }, kModeBlurbs[m]);
    }
    for (int v = 0; v < kHarmonyVoices; ++v)
    {
        auto& st = strips[(size_t) v];
        const int x = 28 + v * 176;
        st.interval.getText = [this, v]
        {
            const int mode = proc.geti (ids::harmonyMode);
            const int t = shownTargets[(size_t) v];
            String s = mode == NotesMode ? "NOTE " + String (v + 1) : String (intervalLabel (proc.geti (ids::voiceInterval (v)), mode));
            if (t >= 0) s << "   " << noteName (t);
            return s;
        };
        st.interval.fontH = 14.0f;
        st.interval.onOpen = [this, v] { chooseInterval (v); };
        st.interval.setTooltip ("What this voice sings. The note on the right is where it is right now.");
        addAndMakeVisible (st.interval);
        st.interval.setBounds (x, 780, 166, 38);
        choices.push_back (&st.interval);

        addKnob (st.level, ids::voiceLevel (v), "LEVEL", { x, 824, 82, 100 }, "This voice's level. All the way down, it's off.");
        addKnob (st.pan, ids::voicePan (v), "PAN", { x + 84, 824, 82, 100 }, "Where this voice sits, left to right.");
        st.level.small = st.pan.small = true;
        st.pan.bipolar = true;
    }
    addKnob (humanizeKnob, ids::humanize, "HUMANIZE", { 744, 790, 96, 118 },
             "Makes the voices sound like other people: each drifts in pitch and timing a little, and each gets its own tone.");
    addKnob (hFormantKnob, ids::harmonyFormant, "FORMANT", { 840, 790, 96, 118 },
             "The harmony voices' tone: left sounds like bigger, deeper singers, right like smaller, lighter ones.");
    hFormantKnob.bipolar = true;
    addKnob (hGlideKnob, ids::harmonyGlide, "GLIDE", { 936, 790, 96, 118 },
             "How quickly the voices move to a new note. Short is tight; long scoops like a real singer.");

    // --- double -----------------------------------------------------------------------------------
    addKnob (dLevelKnob, ids::doubleLevel, "LEVEL", { 1098, 790, 92, 118 }, "Two more takes of you, one each side. All the way down, they're off.");
    addKnob (dLooseKnob, ids::doubleLoose, "LOOSE", { 1192, 790, 92, 118 }, "How late and uneven the doubles come in: tight overdub to sloppy gang vocal.");
    addKnob (dDetuneKnob, ids::doubleDetune, "DETUNE", { 1286, 790, 92, 118 }, "How far off pitch the doubles wander, and how different they sound.");
    addKnob (dWidthKnob, ids::doubleWidth, "WIDTH", { 1380, 790, 92, 118 }, "How far apart the doubles sit, left and right.");

    shownTargets.fill (-1);
    startTimerHz (30);
    timerCallback();
}

DescantPanel::~DescantPanel()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ---------------------------------------------------------------------------
void DescantPanel::addKnob (Knob& k, const char* id, const String& caption, Rectangle<int> r, const String& tip)
{
    k.caption = caption;
    k.setTooltip (tip);
    if (auto* param = proc.apvts.getParameter (id))
        k.format = [param] (double v) { return param->getText (param->convertTo0to1 ((float) v), 32); };
    addAndMakeVisible (k);
    k.setBounds (r);
    attachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, id, k));
}

void DescantPanel::addToggle (PanelButton& b, const char* id, Rectangle<int> r, const String& tip)
{
    addButton (b, r, tip, [this, id] (const juce::ModifierKeys&)
    {
        proc.setParam (id, proc.getParam (id) > 0.5f ? 0.0f : 1.0f);
    });
    toggles.push_back ({ &b, id });
}

void DescantPanel::addRadio (PanelButton& b, const char* id, int value, Rectangle<int> r, const String& tip)
{
    addButton (b, r, tip, [this, id, value] (const juce::ModifierKeys&) { proc.setParam (id, (float) value); });
    radios.push_back ({ &b, id, value });
}

void DescantPanel::addButton (PanelButton& b, Rectangle<int> r, const String& tip,
                              std::function<void (const juce::ModifierKeys&)> click)
{
    b.setTooltip (tip);
    b.onClick = std::move (click);
    addAndMakeVisible (b);
    b.setBounds (r);
}

void DescantPanel::addChoice (ChoiceDisplay& c, const char* id, const String& caption, Rectangle<int> r, const String& tip)
{
    c.caption = caption;
    c.setTooltip (tip);
    if (! c.getText)
        c.getText = [this, id]
        {
            if (auto* param = proc.apvts.getParameter (id))
                return param->getCurrentValueAsText();
            return String();
        };
    c.onOpen = [this, &c, id] { chooseParam (c, id); };
    addAndMakeVisible (c);
    c.setBounds (r);
    choices.push_back (&c);
}

void DescantPanel::chooseParam (juce::Component& target, const char* id)
{
    auto* param = proc.apvts.getParameter (id);
    if (param == nullptr)
        return;
    const auto range = proc.apvts.getParameterRange (id);
    const int lo = juce::roundToInt (range.start), hi = juce::roundToInt (range.end);
    const int cur = proc.geti (id);

    juce::PopupMenu m;
    for (int v = lo; v <= hi; ++v)
        m.addItem (param->getText (param->convertTo0to1 ((float) v), 32), true, v == cur,
                   [this, id, v] { proc.setParam (id, (float) v); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target));
}

void DescantPanel::chooseInterval (int voice)
{
    const int mode = proc.geti (ids::harmonyMode);
    if (mode == NotesMode)
        return;   // NOTES ignores intervals
    const char* id = ids::voiceInterval (voice);
    const int cur = proc.geti (id);
    juce::PopupMenu m;
    m.addSectionHeader (mode == ScaleMode ? "IN KEY AND SCALE" : "CHORD NOTES FROM YOU");
    for (int i = 0; i < NumIntervals; ++i)
    {
        if (i == OctUp) m.addSeparator();
        m.addItem (intervalLabel (i, mode), true, i == cur, [this, id, i] { proc.setParam (id, (float) i); });
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&strips[(size_t) voice].interval));
}

// ---------------------------------------------------------------------------
void DescantPanel::openPresetMenu()
{
    juce::PopupMenu m;
    const int cur = proc.presetNumber();
    m.addSectionHeader ("FACTORY");
    const auto& f = factoryPresets();
    for (int i = 0; i < (int) f.size(); ++i)
        m.addItem (String (i + 1).paddedLeft ('0', 2) + "   " + f[(size_t) i].name, true, cur == i + 1,
                   [this, i] { proc.loadFactory (i); });

    const auto users = userPresets();
    m.addSectionHeader ("YOURS");
    if (users.isEmpty())
        m.addItem ("(none yet: press SAVE)", false, false, [] {});
    for (int i = 0; i < users.size(); ++i)
    {
        const auto file = users[i];
        const int n = (int) f.size() + i + 1;
        m.addItem (String (n).paddedLeft ('0', 2) + "   " + file.getFileNameWithoutExtension(), true, cur == n,
                   [this, file] { proc.loadUserPreset (file); });
    }
    m.addSeparator();
    m.addItem ("Save as...", [this] { savePresetDialog(); });
    m.addItem ("Show presets folder", []
    {
        const auto folder = userPresetFolder();
        folder.createDirectory();
        folder.revealToUser();
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetChoice));
}

void DescantPanel::savePresetDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name it. Saving under an existing name replaces it.",
                                                      juce::MessageBoxIconType::NoIcon, this);
    saveWindow->setLookAndFeel (&lnf);
    saveWindow->addTextEditor ("name", proc.presetName());
    saveWindow->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result)
    {
        if (result == 1 && saveWindow != nullptr)
        {
            String error;
            if (! proc.saveUserPreset (saveWindow->getTextEditorContents ("name"), error))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save", error);
        }
        saveWindow.reset();
    }), false);
}

void DescantPanel::syncTooltips()
{
    const bool on = settings.tooltipsEnabled();
    tipsBtn.setLit (on);
    if ((int) on != tooltipsApplied && onTooltips)
    {
        tooltipsApplied = (int) on;
        onTooltips (on);
    }
}

// ---------------------------------------------------------------------------
void DescantPanel::updateStatus (const DescantProcessor::Readout& r)
{
    const int source = proc.geti (ids::source);
    String c;
    if (source == ExternalSource)
        c = r.externalMissing ? "Nothing in the sidechain yet. In Live, open the triangle at the top of Descant and pick the synth's track under Sidechain."
                              : "Shaping the sidechain. Whatever plays on that track takes the shape of your voice.";
    else if (source == VoiceSource)
        c = r.carrierNote >= 0 ? "Following you: the synth is on " + String (noteName (r.carrierNote)) + ", snapped to "
                                     + ids::keyNames()[proc.geti (ids::key)] + " " + String (scaleName (proc.geti (ids::scale))).toLowerCase() + "."
                               : "Sing a note and the synth will follow it.";
    else if (r.midiCount == 0)
        c = "Waiting for notes. On a MIDI track (your keyboard, a clip, or Posy), set MIDI To to this track, then to Descant.";
    else
    {
        const int n = (int) r.sounding.count();
        c = n == 0 ? "No keys down. Play a note or a chord, or press HOLD to keep the last one."
                   : String (n) + (n == 1 ? " note" : " notes") + " sounding" + (proc.getParam (ids::hold) > 0.5f ? " (HOLD)." : ".");
    }

    const float dry = proc.getParam (ids::dry);
    const float voc = proc.getParam (ids::vocoderLevel);
    bool anyVoice = false;
    for (int v = 0; v < kHarmonyVoices; ++v) anyVoice = anyVoice || proc.getParam (ids::voiceLevel (v)) > 0.0f;
    const bool dbl = proc.getParam (ids::doubleLevel) > 0.0f;
    String m;
    if (voc <= -59.9f && ! anyVoice && ! dbl)
        m = "Only your dry voice is on. Turn up VOCODER, a harmony voice or the double.";
    else if (dry <= -59.9f && (anyVoice || dbl) && voc <= -59.9f)
        m = "DRY is off: you'll hear the harmony without your own voice under it.";
    else
        m = "DRY is your own voice. A pure vocoder wants it off; harmony and doubles want it up.";

    const String b = proc.presetBlurb();
    const String t = kModeBlurbs[juce::jlimit (0, 2, proc.geti (ids::harmonyMode))];
    if (c != carrierStatus || m != mixHint || b != blurb || t != modeText)
    {
        carrierStatus = c;
        mixHint = m;
        blurb = b;
        modeText = t;
        repaint (kCarrierText.toNearestInt());
        repaint (kMixText.toNearestInt());
        repaint (kBlurb.toNearestInt());
        repaint (kModeText.toNearestInt());
    }
}

void DescantPanel::timerCallback()
{
    const auto r = proc.readout();
    inMeter.push (proc.takeInputPeak());
    outMeter.push (proc.takeOutputPeak());

    for (int v = 0; v < kHarmonyVoices; ++v)
        shownTargets[(size_t) v] = r.targets[(size_t) v];
    for (auto* c : choices)
        c->refresh();
    for (auto& [b, id] : toggles)
        b->setLit (proc.getParam (id) > 0.5f);
    for (auto& rd : radios)
        rd.button->setLit (proc.geti (rd.id) == rd.value);

    // Input readout.
    {
        String big = "-", small;
        if (r.singing)
        {
            const int q = (int) std::lround (r.sung);
            const int cents = juce::roundToInt ((r.sung - (float) q) * 100.0f);
            big = noteName (q);
            small = cents == 0 ? String ("in tune") : (cents > 0 ? "+" : "") + String (cents) + " cents";
        }
        else
            small = r.gateOpen ? "breath or consonant" : (proc.getParam (ids::gate) <= -79.5f ? "listening" : "below the gate");
        pitchLcd.set (big, small, r.singing, colours::ledGreen);
    }

    // Chord readout.
    {
        if (r.midiCount != lastMidiCount) { lastMidiCount = r.midiCount; midiFlash = 4; }
        std::vector<int> held;
        for (int n = 0; n < 128; ++n)
            if (r.sounding.test ((size_t) n)) held.push_back (n);
        String big = chordName (held);
        String small;
        if (big.isEmpty() && r.chord.any() && proc.geti (ids::harmonyMode) == ChordMode)
        {
            std::vector<int> mem;
            for (int n = 0; n < 128; ++n)
                if (r.chord.test ((size_t) n)) mem.push_back (n);
            big = chordName (mem);
            small = "remembered";
        }
        else if (big.isNotEmpty() && proc.getParam (ids::hold) > 0.5f)
            small = "held";
        if (big.isEmpty()) big = "-";
        chordLcd.set (big, small, midiFlash > 0, colours::lcd);
        if (midiFlash > 0) --midiFlash;
    }

    std::array<float, kHarmonyVoices> levels {};
    for (int v = 0; v < kHarmonyVoices; ++v)
        levels[(size_t) v] = proc.getParam (ids::voiceLevel (v)) * (proc.getParam (ids::harmonyLevel) > -59.9f ? 1.0f : 0.0f);
    keyboard.push (r, proc.geti (ids::harmonyMode), proc.geti (ids::source), levels);
    bandDisplay.push (r, proc.getParam (ids::freeze) > 0.5f, proc.getParam (ids::lowHz), proc.getParam (ids::highHz),
                      proc.getParam (ids::formant));

    // Dim what isn't in use.
    const int source = proc.geti (ids::source);
    const bool synth = source != ExternalSource;
    for (auto* c : std::initializer_list<juce::Component*> { &detuneKnob, &glideKnob, &attackKnob, &releaseKnob, &velocityKnob,
                                                             &octaveChoice, &monoBtn, &holdBtn })
        c->setAlpha (synth ? 1.0f : 0.35f);
    for (auto& b : waveBtns) b.setAlpha (synth ? 1.0f : 0.35f);
    const bool notesMode = proc.geti (ids::harmonyMode) == NotesMode;
    for (int v = 0; v < kHarmonyVoices; ++v)
    {
        auto& st = strips[(size_t) v];
        const float a = proc.getParam (ids::voiceLevel (v)) > 0.0f ? 1.0f : 0.5f;
        st.interval.setAlpha (notesMode ? 0.75f * a : a);
    }
    for (auto* k : { &formantKnob, &widthKnob, &reactKnob, &brightKnob, &consonantKnob, &spreadKnob, &lowKnob, &highKnob })
        k->setAlpha (proc.getParam (ids::vocoderLevel) > -59.9f ? 1.0f : 0.45f);

    updateStatus (r);
}

// ---------------------------------------------------------------------------
void DescantPanel::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    drawText (g, "DESCANT", { 24.0f, 14.0f, 220.0f, 46.0f }, 38.0f, colours::brand, true, juce::Justification::centredLeft);
    drawText (g, "vocoder & harmony", { 212.0f, 22.0f, 200.0f, 34.0f }, 17.0f, colours::ink, true, juce::Justification::centredLeft);
    if (blurb.isNotEmpty())
        drawText (g, blurb, kBlurb, 11.0f, colours::inkDim, false);

    drawSection (g, kInputBox, "INPUT");
    drawSection (g, kCarrierBox, "CARRIER");
    drawSection (g, kMixBox, "MIX");
    drawSection (g, kNotesBox, "NOTES");
    drawSection (g, kVocoderBox, "VOCODER");
    drawSection (g, kHarmonyBox, "HARMONY");
    drawSection (g, kDoubleBox, "DOUBLE");

    // Small labels and status lines.
    drawText (g, "IN", { 22.0f, 96.0f, 30.0f, 12.0f }, 10.0f, colours::inkDim, true);
    drawText (g, "OUT", { 1444.0f, 96.0f, 30.0f, 12.0f }, 10.0f, colours::inkDim, true);
    drawText (g, "SOURCE", { 340.0f, 138.0f, 300.0f, 12.0f }, 9.5f, colours::inkDim, true, juce::Justification::centredLeft);
    drawText (g, "WAVE", { 656.0f, 138.0f, 300.0f, 12.0f }, 9.5f, colours::inkDim, true, juce::Justification::centredLeft);

    g.setFont (panelFont (12.5f, false));
    g.setColour (colours::inkDim);
    g.drawFittedText (carrierStatus, kCarrierText.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
    g.drawFittedText (mixHint, kMixText.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
    g.drawFittedText ("Your voice is split into bands and the synth is shaped to match, band by band. "
                      "More bands and some CONSONANTS make the words clearer; FORMANT changes who's singing without changing the notes.",
                      kVocoderText.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
    g.drawFittedText (modeText, kModeText.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);
    g.drawFittedText ("Two more takes of you, one each side, a little late and a little off: a double-tracked vocal without singing it twice.",
                      kDoubleText.toNearestInt(), juce::Justification::topLeft, 3, 1.0f);

    // Keyboard legend, at the right of the NOTES tab row.
    {
        float x = 1150.0f;
        const float y = kNotesBox.getY() - 7.0f;
        auto item = [&] (juce::Colour c, int kind, const String& t)
        {
            const auto sw = Rectangle<float> (x, y - 5.0f, 10.0f, 10.0f);
            g.setColour (colours::panel);
            g.fillRect (sw.expanded (4.0f, 3.0f).withRight (x + 18.0f + juce::GlyphArrangement::getStringWidth (panelFont (10.0f), t)));
            g.setColour (c);
            if (kind == 0) g.fillRect (sw);
            else if (kind == 1) g.drawRect (sw, 2.0f);
            else g.fillEllipse (sw);
            drawText (g, t, { x + 14.0f, y - 7.0f, 120.0f, 14.0f }, 10.0f, colours::inkDim, true, juce::Justification::centredLeft);
            x += 30.0f + juce::GlyphArrangement::getStringWidth (panelFont (10.0f), t);
        };
        item (colours::lcd, 0, "SOUNDING");
        item (colours::lcd, 1, "CHORD");
        item (colours::brand, 2, "YOU");
        item (colours::voice1, 2, "HARMONY");
    }

    // Voice labels over the harmony strips.
    for (int v = 0; v < kHarmonyVoices; ++v)
    {
        const float x = 28.0f + (float) v * 176.0f;
        drawLed (g, { x + 6.0f, 769.0f }, 5.0f, colours::voice (v), proc.getParam (ids::voiceLevel (v)) > 0.0f);
        drawText (g, "VOICE " + String (v + 1), { x + 16.0f, 762.0f, 100.0f, 14.0f }, 11.0f, colours::ink, true,
                  juce::Justification::centredLeft);
    }
}

// ===========================================================================
DescantEditor::DescantEditor (DescantProcessor& p)
    : AudioProcessorEditor (p), panel (p)
{
    addAndMakeVisible (panel);

    panel.onTooltips = [this] (bool on)
    {
        if (on && tooltips == nullptr)
            tooltips = std::make_unique<juce::TooltipWindow> (this, 700);
        else if (! on)
            tooltips.reset();
    };
    panel.syncTooltips();

    setResizable (true, true);
    const double ratio = (double) DescantPanel::designW / DescantPanel::designH;
    setResizeLimits (900, (int) (900 / ratio), 2600, (int) (2600 / ratio));
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (ratio);
    setSize (1200, (int) (1200 / ratio));
}

void DescantEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
}

void DescantEditor::resized()
{
    const float s = (float) getWidth() / (float) DescantPanel::designW;
    panel.setTransform (juce::AffineTransform::scale (s));
    panel.setBounds (0, 0, DescantPanel::designW, DescantPanel::designH);
}
