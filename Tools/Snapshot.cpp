// Snapshot.cpp — dev tool: run Descant without a host, render its panel to a
// PNG and, optionally, every factory preset to WAV files.
//   DescantSnapshot out.png [wavFolder] [voice.wav] [preset index]
// Without a voice file it sings a short built-in phrase (a synthetic voice
// with vowels and consonants) over four chords sent as MIDI.
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"
#include "TestSignals.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
using namespace descant;

struct Phrase
{
    juce::AudioBuffer<float> voice, side;
    std::vector<std::pair<int, juce::MidiMessage>> midi;   // sample, message
};

// Eight seconds: a melody sung over C, Am, F and G, two seconds each.
Phrase testPhrase (double rate)
{
    Phrase p;
    const int n = (int) (rate * 8.5);
    p.voice.setSize (1, n);
    p.voice.clear();
    p.side.setSize (2, n);
    test::Singer singer (rate);
    // (start s, length s, MIDI note, vowel)
    struct Note { float t, len; int note; int vowel; };
    const Note melody[] {
        { 0.10f, 0.45f, 64, 0 }, { 0.60f, 0.40f, 67, 1 }, { 1.05f, 0.85f, 72, 0 },
        { 2.10f, 0.45f, 69, 2 }, { 2.60f, 0.40f, 67, 0 }, { 3.05f, 0.85f, 64, 1 },
        { 4.10f, 0.45f, 65, 0 }, { 4.60f, 0.40f, 69, 2 }, { 5.05f, 0.85f, 72, 0 },
        { 6.10f, 0.45f, 71, 1 }, { 6.60f, 0.40f, 67, 0 }, { 7.05f, 1.10f, 62, 2 } };
    const test::Vowel vowels[] { test::kAh, test::kEe, test::kOo };
    int current = -1;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / (float) rate;
        int active = -1;
        for (int k = 0; k < (int) std::size (melody); ++k)
            if (t >= melody[k].t && t < melody[k].t + melody[k].len) active = k;
        if (active != current && active >= 0) singer.setVowel (vowels[melody[active].vowel]);
        current = active;
        float x = 0.0f;
        if (active >= 0)
        {
            const auto& m = melody[active];
            const float into = t - m.t, left = m.t + m.len - t;
            const float env = juce::jmin (1.0f, into / 0.03f, left / 0.05f);
            x = singer.next (440.0f * std::pow (2.0f, (float) (m.note - 12 - 69) / 12.0f), 0.25f * env, 0.0f, 25.0f);
        }
        else
        {
            // An "s" or a "t" between notes.
            const float gap = std::fmod (t, 0.5f);
            x = singer.next (0.0f, gap < 0.06f ? 0.18f : 0.0f, 1.0f);
        }
        p.voice.setSample (0, i, x);
        // Sidechain: a plain saw on the chord roots.
        const float roots[] { 130.8f, 110.0f, 87.3f, 98.0f };
        const float f = roots[juce::jlimit (0, 3, (int) (t / 2.0f))];
        const float ph = std::fmod ((float) i * f / (float) rate, 1.0f);
        p.side.setSample (0, i, 0.3f * (2.0f * ph - 1.0f));
        p.side.setSample (1, i, 0.3f * (2.0f * ph - 1.0f));
    }
    // Chords, in Live's numbering: C3 is 60.
    const int chords[4][3] { { 48, 52, 55 }, { 45, 48, 52 }, { 41, 45, 48 }, { 43, 47, 50 } };
    for (int c = 0; c < 4; ++c)
    {
        const int on = (int) (rate * 2.0 * c), off = (int) (rate * (2.0 * c + 1.98));
        for (int note : chords[c])
        {
            p.midi.push_back ({ on, juce::MidiMessage::noteOn (1, note, (juce::uint8) 100) });
            p.midi.push_back ({ off, juce::MidiMessage::noteOff (1, note) });
        }
    }
    std::sort (p.midi.begin(), p.midi.end(), [] (auto& a, auto& b) { return a.first < b.first; });
    return p;
}

void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double rate)
{
    f.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (new juce::FileOutputStream (f), rate,
                                                                     (unsigned int) b.getNumChannels(), 24, {}, 0));
    if (w)
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

// Runs `phrase` through `proc`, returns the stereo output.
juce::AudioBuffer<float> render (DescantProcessor& proc, const Phrase& phrase, double rate, int block, int startSample = 0, int length = -1)
{
    const int n = length < 0 ? phrase.voice.getNumSamples() : length;
    juce::AudioBuffer<float> out (2, n);
    juce::AudioBuffer<float> io (4, block);
    for (int pos = 0; pos < n; pos += block)
    {
        const int len = juce::jmin (block, n - pos);
        io.setSize (4, len, false, false, true);
        io.clear();
        const int src = startSample + pos;
        io.copyFrom (0, 0, phrase.voice, 0, src, len);
        io.copyFrom (1, 0, phrase.voice, 0, src, len);
        io.copyFrom (2, 0, phrase.side, 0, src, len);
        io.copyFrom (3, 0, phrase.side, 1, src, len);
        juce::MidiBuffer midi;
        for (const auto& [at, m] : phrase.midi)
            if (at >= src && at < src + len) midi.addEvent (m, at - src);
        proc.processBlock (io, midi);
        out.copyFrom (0, pos, io, 0, 0, len);
        out.copyFrom (1, pos, io, 1, 0, len);
    }
    return out;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2)
    {
        std::printf ("usage: DescantSnapshot out.png [wavFolder] [voice.wav] [preset index]\n");
        return 1;
    }
    const double rate = 48000.0;
    const int block = 256;

    DescantProcessor proc;
    // Main in stereo, sidechain stereo, out stereo.
    auto layout = proc.getBusesLayout();
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    if (layout.inputBuses.size() > 1) layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
    layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    proc.setBusesLayout (layout);
    proc.setRateAndBufferSizeDetails (rate, block);
    proc.prepareToPlay (rate, block);

    auto phrase = testPhrase (rate);
    if (argc > 3 && juce::String (argv[3]).isNotEmpty() && juce::String (argv[3]) != "-")
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]))));
        if (r)
        {
            const int len = (int) juce::jmin<juce::int64> (r->lengthInSamples, (juce::int64) (rate * 8.5));
            juce::AudioBuffer<float> v ((int) r->numChannels, len);
            r->read (&v, 0, len, 0, true, true);
            phrase.voice.clear();
            phrase.voice.copyFrom (0, 0, v, 0, 0, len);
        }
    }

    if (argc > 2)
    {
        const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        dir.createDirectory();
        juce::AudioBuffer<float> dry (2, phrase.voice.getNumSamples());
        dry.copyFrom (0, 0, phrase.voice, 0, 0, dry.getNumSamples());
        dry.copyFrom (1, 0, phrase.voice, 0, 0, dry.getNumSamples());
        writeWav (dir.getChildFile ("00 dry.wav"), dry, rate);
        const auto& presets = factoryPresets();
        const float rmsIn = phrase.voice.getRMSLevel (0, 0, phrase.voice.getNumSamples());
        for (int p = 0; p < (int) presets.size(); ++p)
        {
            proc.loadFactory (p);
            proc.prepareToPlay (rate, block);   // start from silence each time
            const auto out = render (proc, phrase, rate, block);
            const float peak = out.getMagnitude (0, out.getNumSamples());
            const float rmsOut = 0.5f * (out.getRMSLevel (0, 0, out.getNumSamples()) + out.getRMSLevel (1, 0, out.getNumSamples()));
            std::printf ("%02d %-24s peak %.3f  level %+5.1f dB vs dry\n", p + 1, presets[(size_t) p].name, peak,
                         juce::Decibels::gainToDecibels (rmsOut / juce::jmax (1.0e-6f, rmsIn)));
            writeWav (dir.getChildFile (juce::String (p + 1).paddedLeft ('0', 2) + " " + presets[(size_t) p].name + ".wav"), out, rate);
        }
    }

    // A moment of singing over a chord, so every display has something to show.
    proc.loadFactory (argc > 4 ? juce::String (argv[4]).getIntValue() : 2);
    proc.prepareToPlay (rate, block);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (DescantPanel::designW, DescantPanel::designH);
    const int stop = (int) (rate * 5.4);
    for (int pos = 0; pos < stop; pos += block * 8)
    {
        render (proc, phrase, rate, block, pos, juce::jmin (block * 8, stop - pos));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (pos + block * 8 >= stop ? 120 : 4);
    }
    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);

    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (img, stream);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    ed.reset();
    return 0;
}
