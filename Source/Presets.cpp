#include "Presets.h"
#include "Params.h"
#include "Settings.h"

namespace descant
{
const std::vector<FactoryPreset>& factoryPresets()
{
    using namespace ids;
    static const std::vector<FactoryPreset> list {
        { "Start Here", "A classic vocoder. Play keys and sing.", {} },

        { "String Machine", "Slow, wide ensemble pads. Hold a chord and sing long vowels.", {
            { wave, Super }, { detune, 0.55f }, { attack, 160.0f }, { release, 900.0f },
            { bands, 2 }, { width, 0.7f }, { react, 0.3f }, { bright, -0.1f }, { spread, 0.8f } } },

        { "Choir of One", "Your voice as a choir on the keys, with harmony and a double behind it.", {
            { wave, Choir }, { detune, 0.6f }, { attack, 60.0f }, { release, 500.0f },
            { bands, 5 }, { width, 0.55f }, { react, 0.4f }, { spread, 0.6f },
            { harmonyMode, ChordMode }, { voiceLevel (0), 0.45f }, { voiceLevel (1), 0.4f },
            { doubleLevel, 0.4f }, { dry, -14.0f } } },

        { "Robot Voice", "No keys needed: the synth sings the note you sing, snapped to KEY and SCALE.", {
            { source, VoiceSource }, { wave, Square }, { bands, 2 }, { react, 0.75f }, { bright, 0.3f },
            { consonants, 0.7f }, { spread, 0.2f } } },

        { "Talk Box", "One note at a time with a little glide. Funky, mouthy, bright.", {
            { wave, Saw }, { mono, 1.0f }, { glide, 45.0f }, { release, 80.0f },
            { bands, 4 }, { width, 0.25f }, { react, 0.8f }, { bright, 0.45f }, { consonants, 0.6f }, { spread, 0.0f } } },

        { "Disco Machine", "Tight square-wave vocoder with fast bands and crisp consonants.", {
            { wave, Square }, { octave, 2 }, { bands, 2 }, { width, 0.35f }, { react, 0.85f },
            { bright, 0.5f }, { consonants, 0.75f }, { spread, 0.5f } } },

        { "Eight Bands", "Coarse and retro: eight wide bands, like the oldest machines.", {
            { wave, Saw }, { bands, 0 }, { width, 0.6f }, { react, 0.55f }, { bright, 0.3f }, { spread, 0.6f } } },

        { "Giant", "Formants down and an octave lower: a huge, dark voice.", {
            { wave, Saw }, { octave, 1 }, { formant, -5.0f }, { bands, 4 }, { bright, 0.2f }, { consonants, 0.4f } } },

        { "Helium", "Formants up: small and bright.", {
            { wave, Saw }, { octave, 3 }, { formant, 6.0f }, { bands, 4 }, { bright, -0.2f } } },

        { "Glass Pad", "Forty narrow bands ringing on a slow chord. Freeze a vowel and play.", {
            { wave, Super }, { detune, 0.3f }, { attack, 300.0f }, { release, 1800.0f },
            { bands, 6 }, { width, 0.1f }, { react, 0.25f }, { bright, 0.4f }, { spread, 1.0f }, { consonants, 0.3f } } },

        { "Three Part Harmony", "No vocoder. Two singers join you on the chord you hold (or last held).", {
            { vocoderLevel, -60.0f }, { dry, 0.0f },
            { harmonyMode, ChordMode }, { voiceInterval (0), Up1 }, { voiceLevel (0), 0.7f },
            { voiceInterval (1), Down1 }, { voiceLevel (1), 0.65f }, { humanize, 0.4f } } },

        { "Gospel Stack", "Four voices above and below you on the chord, plus a double.", {
            { vocoderLevel, -60.0f }, { dry, 0.0f },
            { harmonyMode, ChordMode }, { voiceInterval (0), Up1 }, { voiceLevel (0), 0.6f },
            { voiceInterval (1), Down1 }, { voiceLevel (1), 0.6f },
            { voiceInterval (2), Up2 }, { voiceLevel (2), 0.45f },
            { voiceInterval (3), Down2 }, { voiceLevel (3), 0.45f },
            { humanize, 0.5f }, { doubleLevel, 0.35f } } },

        { "Thirds in Key", "No keys needed: a third above you in KEY and SCALE.", {
            { vocoderLevel, -60.0f }, { dry, 0.0f },
            { harmonyMode, ScaleMode }, { voiceInterval (0), Up1 }, { voiceLevel (0), 0.7f }, { voicePan (0), -0.3f },
            { humanize, 0.35f } } },

        { "Your Voice, Their Chord", "The four voices sing exactly the notes you hold, in your voice.", {
            { vocoderLevel, -60.0f }, { dry, -60.0f },
            { harmonyMode, NotesMode },
            { voiceLevel (0), 0.65f }, { voiceLevel (1), 0.6f }, { voiceLevel (2), 0.6f }, { voiceLevel (3), 0.55f },
            { humanize, 0.45f }, { harmonyGlide, 25.0f } } },

        { "Octaves", "An octave below and a quiet octave above.", {
            { vocoderLevel, -60.0f }, { dry, 0.0f },
            { voiceInterval (0), OctDown }, { voiceLevel (0), 0.6f }, { voicePan (0), 0.0f },
            { voiceInterval (1), OctUp }, { voiceLevel (1), 0.3f }, { voicePan (1), 0.0f }, { humanize, 0.25f } } },

        { "Tight Double", "A second take of you, close behind, a little to each side.", {
            { vocoderLevel, -60.0f }, { dry, 0.0f },
            { doubleLevel, 0.6f }, { doubleLoose, 0.15f }, { doubleDetune, 0.25f }, { doubleWidth, 0.6f } } },

        { "Loose Double", "Two more takes, wide and a bit sloppy, like a stacked chorus vocal.", {
            { vocoderLevel, -60.0f }, { dry, 0.0f },
            { doubleLevel, 0.75f }, { doubleLoose, 0.7f }, { doubleDetune, 0.55f }, { doubleWidth, 1.0f } } },

        { "Posy Pads", "Made for Posy: play pads with one finger, sing, and get chords plus a harmony on top.", {
            { wave, Super }, { detune, 0.45f }, { attack, 90.0f }, { release, 700.0f },
            { bands, 4 }, { width, 0.55f }, { react, 0.4f }, { spread, 0.7f },
            { harmonyMode, ChordMode }, { voiceInterval (0), Up1 }, { voiceLevel (0), 0.4f },
            { dry, -18.0f }, { humanize, 0.4f } } },

        { "Robot Choir", "The synth follows your voice and a chord of harmony sings with it.", {
            { source, VoiceSource }, { wave, Choir }, { detune, 0.5f }, { bands, 3 },
            { harmonyMode, ScaleMode }, { voiceInterval (0), Up1 }, { voiceLevel (0), 0.5f },
            { voiceInterval (1), Down2 }, { voiceLevel (1), 0.45f }, { dry, -60.0f } } },

        { "Sidechain Synth", "Shape any synth track with your voice: route it to the sidechain.", {
            { source, ExternalSource }, { bands, 4 }, { react, 0.6f }, { bright, 0.2f }, { spread, 0.5f } } },
    };
    return list;
}

juce::File userPresetFolder()
{
    return Settings::file().getParentDirectory().getChildFile ("Presets");
}

juce::Array<juce::File> userPresets()
{
    auto files = userPresetFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + kPresetExtension);
    files.sort();
    return files;
}
} // namespace descant
