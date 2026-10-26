// EngineTests.cpp — checks Descant's signal path without JUCE or a host.
// Run with ctest, or directly: ./DescantEngineTests [-v]
#include "Engine/Engine.h"
#include "TestSignals.h"

#include <cstdio>
#include <cstring>
#include <string>

using namespace descant;
using namespace descant::test;

namespace
{
int failures = 0, checks = 0;
bool verbose = false;

void check (bool ok, const std::string& what)
{
    ++checks;
    if (! ok)
    {
        ++failures;
        std::printf ("FAIL: %s\n", what.c_str());
    }
    else if (verbose)
        std::printf ("ok:   %s\n", what.c_str());
}

// Median pitch (MIDI) of a signal, read every 10 ms after the first 150 ms.
float measurePitch (const std::vector<float>& x, double sr, float* voicedShare = nullptr)
{
    PitchTracker t;
    t.prepare (sr);
    std::vector<float> got;
    const int hop = (int) (sr * 0.01);
    int frames = 0;
    for (int pos = 0; pos + hop <= (int) x.size(); pos += hop)
    {
        t.push (x.data() + pos, hop, true);
        if (pos < sr * 0.15) continue;
        ++frames;
        if (t.voiced()) got.push_back (t.midiNote());
    }
    if (voicedShare != nullptr) *voicedShare = frames > 0 ? (float) got.size() / (float) frames : 0.0f;
    if (got.empty()) return -1.0f;
    std::sort (got.begin(), got.end());
    return got[got.size() / 2];
}

std::vector<float> sing (double sr, float hz, float seconds, Vowel v = kAh, float vibrato = 0.0f, float level = 0.3f)
{
    Singer s (sr);
    s.setVowel (v);
    std::vector<float> x ((size_t) (sr * seconds));
    for (auto& y : x) y = s.next (hz, level, 0.0f, vibrato);
    return x;
}

bool finiteAndBounded (const std::vector<float>& x, float limit = 1.0001f)
{
    for (float v : x)
        if (! std::isfinite (v) || std::abs (v) > limit) return false;
    return true;
}

// ---------------------------------------------------------------------------
void testPitchTracker()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (float hz : { 82.4f, 110.0f, 146.8f, 196.0f, 261.6f, 392.0f, 523.3f, 784.0f })
        {
            const auto x = sing (sr, hz, 0.8f, hz > 400.0f ? kOo : kAh);
            float share = 0.0f;
            const float got = measurePitch (x, sr, &share);
            const float want = hzToMidi (hz);
            char buf[160];
            std::snprintf (buf, sizeof buf, "tracker %.0f Hz at %.0f: got %.2f want %.2f, voiced %.0f%%", hz, sr, got, want, share * 100.0f);
            check (std::abs (got - want) < 0.15f && share > 0.9f, buf);
        }

    // Silence and noise are not voiced.
    const double sr = 48000.0;
    std::vector<float> silence ((size_t) sr, 0.0f);
    float share = 1.0f;
    measurePitch (silence, sr, &share);
    check (share == 0.0f, "tracker: silence is unvoiced");
    Rng r (3);
    std::vector<float> noise ((size_t) sr);
    for (auto& v : noise) v = r.bi() * 0.3f;
    measurePitch (noise, sr, &share);
    check (share < 0.1f, "tracker: noise is unvoiced (" + std::to_string (share) + ")");
}

// ---------------------------------------------------------------------------
std::vector<float> shift (const std::vector<float>& in, double sr, float semis, float formant = 1.0f, float* latencyMs = nullptr)
{
    PitchTracker t;
    t.prepare (sr);
    GrainSource src;
    src.prepare (sr);
    ShiftVoice v;
    v.prepare (sr);
    std::vector<float> out (in.size(), 0.0f);
    const int block = 64;
    for (int pos = 0; pos + block <= (int) in.size(); pos += block)
    {
        t.push (in.data() + pos, block, true);
        const float p = t.voiced() ? t.period() : 0.0f;
        for (int i = 0; i < block; ++i) src.write (in[(size_t) (pos + i)], p);
        ShiftVoice::Control c;
        c.ratio = semisToRatio (semis);
        c.formant = formant;
        v.render (src, src.written() - block, block, c, out.data() + pos, 1.0f);
    }
    if (latencyMs != nullptr) *latencyMs = v.latencyMs();
    return out;
}

void testShifter()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (float base : { 110.0f, 220.0f, 330.0f })
            for (float st : { -12.0f, -7.0f, -5.0f, -3.0f, 0.0f, 3.0f, 4.0f, 7.0f, 12.0f })
            {
                const auto in = sing (sr, base, 1.2f, kAh, 0.0f);
                float lat = 0.0f;
                const auto out = shift (in, sr, st, 1.0f, &lat);
                float share = 0.0f;
                const float got = measurePitch (out, sr, &share);
                const float want = hzToMidi (base) + st;
                const int skip = (int) (sr * 0.3);
                const float gainDb = gainToDb (rms (out.data() + skip, (int) out.size() - skip) / rms (in.data() + skip, (int) in.size() - skip));
                char buf[200];
                std::snprintf (buf, sizeof buf, "shift %.0f Hz %+.0f st at %.0f: got %.2f want %.2f, level %+.1f dB, lag %.1f ms",
                               base, st, sr, got, want, gainDb, lat);
                // The tracker can't hear below 70 Hz; the level moves a little as
                // harmonics cross the vowel's formants, which is what a real voice does.
                const bool pitchOk = midiToHz (want) < 72.0f || (std::abs (got - want) < 0.2f && share > 0.85f);
                check (pitchOk && std::abs (gainDb) < 6.5f && lat < 40.0f && finiteAndBounded (out, 2.0f), buf);
            }

    // Unison through the shifter is close to the input (a clean double).
    const double sr = 48000.0;
    const auto in = sing (sr, 196.0f, 1.0f);
    float lat = 0.0f;
    const auto out = shift (in, sr, 0.0f, 1.0f, &lat);
    const int skip = (int) (sr * 0.3);
    const float a = rms (in.data() + skip, (int) in.size() - skip), b = rms (out.data() + skip, (int) out.size() - skip);
    check (std::abs (gainToDb (b / a)) < 1.5f, "shifter unison keeps the level");

    // Formant moves the vowel, not the pitch.
    for (float fm : { 0.8f, 1.25f })
    {
        const auto o = shift (in, sr, 0.0f, fm);
        const float got = measurePitch (o, sr);
        check (std::abs (got - hzToMidi (196.0f)) < 0.2f, "formant " + std::to_string (fm) + " keeps the pitch");
    }

    // Consonants pass through at unison level.
    {
        Rng r (4);
        std::vector<float> noise ((size_t) sr);
        for (auto& v : noise) v = r.bi() * 0.2f;
        const auto o = shift (noise, sr, 7.0f);
        const float g = gainToDb (rms (o.data() + skip, (int) o.size() - skip) / rms (noise.data() + skip, (int) noise.size() - skip));
        check (std::abs (g) < 2.0f && finiteAndBounded (o), "unvoiced passes at level (" + std::to_string (g) + " dB)");
    }
}

// ---------------------------------------------------------------------------
void testMusic()
{
    check (chordName ({ 60, 64, 67 }) == "C", "chord C");
    check (chordName ({ 57, 60, 64, 67 }) == "Am7", "chord Am7: " + chordName ({ 57, 60, 64, 67 }));
    check (chordName ({ 52, 55, 60 }) == "C/E", "chord C/E: " + chordName ({ 52, 55, 60 }));
    check (chordName ({ 60, 64, 67, 71, 74 }) == "CM9", "chord CM9: " + chordName ({ 60, 64, 67, 71, 74 }));
    check (chordName ({ 62, 65, 69, 72 }) == "Dm7", "chord Dm7");
    check (chordName ({ 59, 62, 65, 69 }) == "Bm7b5", "chord Bm7b5: " + chordName ({ 59, 62, 65, 69 }));
    check (chordName ({ 54, 58, 61 }) == "F#", "chord F#");
    check (chordName ({ 60, 65, 67 }) == "Csus4", "chord Csus4");
    check (chordName ({ 55, 59, 62, 65 }) == "G7", "chord G7");
    check (chordName ({ 64 }) == "E", "single note names itself");
    check (chordName ({}).empty(), "no notes, no name");
    check (noteName (60) == "C3", "middle C is C3, as in Live");

    check (snapToScale (61, 0, Major) == 60, "C# snaps down to C in C major");
    check (scaleStep (64, 2, 0, Major) == 67, "E up a diatonic 3rd is G");
    check (scaleStep (64, -2, 0, Major) == 60, "E down a diatonic 3rd is C");

    HarmonyContext ctx;
    ctx.mode = ChordMode;
    ctx.chord = { 48, 52, 55 };   // C major
    check (harmonyNote (ctx, 64, Up1, 0) == 67, "chord: E + UP 1 = G");
    check (harmonyNote (ctx, 64, Up2, 0) == 72, "chord: E + UP 2 = C");
    check (harmonyNote (ctx, 64, Down1, 0) == 60, "chord: E + DOWN 1 = C");
    check (harmonyNote (ctx, 64, Down2, 0) == 55, "chord: E + DOWN 2 = G");
    check (harmonyNote (ctx, 65, Up1, 0) == 67, "chord: F (not in chord) + UP 1 = G");
    check (harmonyNote (ctx, 64, OctDown, 0) == 52, "octave down");
    ctx.chord = { 57, 60, 64 };   // A minor
    check (harmonyNote (ctx, 64, Up1, 0) == 69, "chord Am: E + UP 1 = A");

    ctx.mode = ScaleMode;
    ctx.key = 0; ctx.scale = Major;
    check (harmonyNote (ctx, 64, Up1, 0) == 67, "scale: 3rd up from E = G");
    check (harmonyNote (ctx, 64, Up2, 0) == 71, "scale: 5th up from E = B");
    check (harmonyNote (ctx, 64, Up3, 0) == 72, "scale: 6th up from E = C");
    check (harmonyNote (ctx, 64, Down1, 0) == 60, "scale: 3rd down from E = C");
    check (harmonyNote (ctx, 64, Down2, 0) == 59, "scale: 4th down from E = B");
    check (harmonyNote (ctx, 64, Down3, 0) == 55, "scale: 6th down from E = G");
    ctx.key = 9; ctx.scale = Minor;
    check (harmonyNote (ctx, 60, Up1, 0) == 64, "A minor: 3rd up from C = E");

    ctx.mode = NotesMode;
    ctx.chord = { 55, 59, 62 };
    check (harmonyNote (ctx, -1, Up1, 0) == 55 && harmonyNote (ctx, -1, Up1, 2) == 62 && harmonyNote (ctx, -1, Up1, 3) == -1,
           "notes: voices take the held notes, lowest first");

    // Chord mode without a chord falls back to the key.
    ctx.mode = ChordMode;
    ctx.chord.clear();
    ctx.key = 0; ctx.scale = Major;
    check (harmonyNote (ctx, 64, Up1, 0) == 67, "chord mode with no chord uses the key");

    // NoteState: hold and chord memory.
    NoteState ns;
    ns.noteOn (60, 1.0f); ns.noteOn (64, 1.0f); ns.noteOn (67, 1.0f);
    ns.advance (100.0f);
    ns.noteOff (67); ns.advance (10.0f); ns.noteOff (64); ns.noteOff (60);
    check (ns.sounding().empty(), "notes stop when released");
    check (ns.lastChord() == std::vector<int> ({ 60, 64, 67 }), "the whole chord is remembered after letting go one by one");
    ns.setHold (true);
    ns.noteOn (62, 1.0f); ns.noteOn (65, 1.0f); ns.noteOff (62); ns.noteOff (65);
    check (ns.sounding() == std::vector<int> ({ 62, 65 }), "HOLD keeps the chord");
    ns.noteOn (67, 1.0f);
    check (ns.sounding() == std::vector<int> ({ 67 }), "HOLD: a new hand starts a new chord");
    ns.setHold (false);
    check (ns.sounding().size() == 1, "HOLD off keeps what's still down");
    ns.noteOff (67);
    // Overlapping chord change (new notes before old ones are let go).
    ns.noteOn (60, 1.0f); ns.noteOn (64, 1.0f); ns.advance (100.0f);
    ns.noteOn (65, 1.0f); ns.noteOn (69, 1.0f); ns.noteOff (60); ns.noteOff (64); ns.advance (100.0f);
    check (ns.lastChord() == std::vector<int> ({ 65, 69 }), "overlapping chord change remembers the new chord");
}

// ---------------------------------------------------------------------------
struct Run
{
    std::vector<float> l, r;
};

Run runEngine (Engine& e, const EngineSettings& s, const std::vector<float>& voice, double sr,
               const std::vector<MidiEvent>& atStart, const std::vector<float>* side = nullptr)
{
    Run out;
    out.l.assign (voice.size(), 0.0f);
    out.r.assign (voice.size(), 0.0f);
    const int block = 256;
    bool first = true;
    for (int pos = 0; pos + block <= (int) voice.size(); pos += block)
    {
        const float* sc = side != nullptr ? side->data() + pos : nullptr;
        e.process (voice.data() + pos, nullptr, sc, sc, out.l.data() + pos, out.r.data() + pos, block, s,
                   first ? atStart.data() : nullptr, first ? (int) atStart.size() : 0);
        first = false;
    }
    (void) sr;
    return out;
}

MidiEvent on (int note) { MidiEvent m; m.type = MidiEvent::NoteOn; m.note = (uint8_t) note; m.value = 0.8f; return m; }

EngineSettings vocoderOnly()
{
    EngineSettings s;
    s.dry = 0.0f;
    s.harmonyLevel = 0.0f;
    s.doubleLevel = 0.0f;
    s.gateDb = -80.0f;
    return s;
}

void testVocoder()
{
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const auto voice = sing (sr, 150.0f, 1.5f);
        const int skip = (int) (sr * 0.4);
        const float vin = rms (voice.data() + skip, (int) voice.size() - skip);

        // No keys, no vocoder.
        {
            Engine e; e.prepare (sr, 256);
            auto s = vocoderOnly();
            const auto r = runEngine (e, s, voice, sr, {});
            check (rms (r.l.data() + skip, (int) r.l.size() - skip) < 1.0e-4f, "vocoder is silent with no keys held");
        }
        // One key: the output sings that note, near the voice's level.
        {
            Engine e; e.prepare (sr, 256);
            auto s = vocoderOnly();
            const auto r = runEngine (e, s, voice, sr, { on (57) });   // A2 in Live, 220 Hz
            const float got = measurePitch (r.l, sr);
            const float lvl = gainToDb (rms (r.l.data() + skip, (int) r.l.size() - skip) / vin);
            char buf[160];
            std::snprintf (buf, sizeof buf, "vocoder at %.0f: one key sings A2 (got %.2f), level %+.1f dB vs voice", sr, got, lvl);
            check (std::abs (got - 57.0f) < 0.3f && lvl > -7.0f && lvl < 4.0f, buf);
        }
        // Every wave, band count and corner of the knobs stays finite and in range.
        for (int wave = 0; wave < NumWaves; ++wave)
            for (int bands : { 8, 20, 40 })
                for (float corner : { 0.0f, 1.0f })
                {
                    Engine e; e.prepare (sr, 256);
                    auto s = vocoderOnly();
                    s.carrier.wave = wave;
                    s.vocoder.bands = bands;
                    s.vocoder.width = corner;
                    s.vocoder.react = 1.0f - corner;
                    s.vocoder.formant = corner > 0.5f ? 12.0f : -12.0f;
                    s.vocoder.bright = corner > 0.5f ? 1.0f : -1.0f;
                    s.vocoder.consonants = corner;
                    s.carrier.octave = corner > 0.5f ? 2 : -2;
                    s.output = 4.0f;
                    const auto r = runEngine (e, s, voice, sr, { on (48), on (52), on (55), on (59), on (62), on (98), on (24) });
                    check (finiteAndBounded (r.l) && finiteAndBounded (r.r),
                           "vocoder finite: wave " + std::to_string (wave) + " bands " + std::to_string (bands)
                               + " corner " + std::to_string ((int) corner) + " sr " + std::to_string ((int) sr));
                }
    }

    // The sidechain carrier works and complains when it's missing.
    {
        const double sr = 48000.0;
        const auto voice = sing (sr, 150.0f, 1.2f);
        std::vector<float> side (voice.size());
        float ph = 0.0f;
        for (auto& v : side) { ph += 98.0f / (float) sr; if (ph >= 1.0f) ph -= 1.0f; v = 0.4f * (2.0f * ph - 1.0f); }
        Engine e; e.prepare (sr, 256);
        auto s = vocoderOnly();
        s.source = ExternalSource;
        const auto r = runEngine (e, s, voice, sr, {}, &side);
        check (std::abs (measurePitch (r.l, sr) - hzToMidi (98.0f)) < 0.3f, "sidechain carrier sets the pitch");
        Engine e2; e2.prepare (sr, 256);
        runEngine (e2, s, voice, sr, {});
        check (e2.externalMissing(), "missing sidechain is reported");
    }

    // VOICE source: the synth follows the singer, snapped to the key.
    {
        const double sr = 48000.0;
        const auto voice = sing (sr, 233.1f, 1.2f);   // A#, not in C major
        Engine e; e.prepare (sr, 256);
        auto s = vocoderOnly();
        s.source = VoiceSource;
        s.key = 0; s.scale = Major;
        const auto r = runEngine (e, s, voice, sr, {});
        const float got = measurePitch (r.l, sr);
        check (std::abs (got - 57.0f) < 0.3f, "VOICE source plays the sung note snapped to the key (got " + std::to_string (got) + ")");
    }
}

void testHarmonyEngine()
{
    const double sr = 48000.0;
    const auto voice = sing (sr, 220.0f, 1.6f, kAh, 0.0f);   // A3 = MIDI 57
    const int skip = (int) (sr * 0.5);

    // NOTES: one voice sings the held note from your voice.
    {
        Engine e; e.prepare (sr, 256);
        EngineSettings s;
        s.dry = 0.0f; s.vocoderLevel = 0.0f; s.doubleLevel = 0.0f; s.gateDb = -80.0f;
        s.harmonyMode = NotesMode;
        s.humanize = 0.0f;
        s.voices[0].level = 1.0f;
        const auto r = runEngine (e, s, voice, sr, { on (62) });   // D3
        const float got = measurePitch (r.l, sr);
        check (std::abs (got - 62.0f) < 0.25f, "NOTES: harmony sings the held note (got " + std::to_string (got) + ")");
        check (e.harmonyTarget (0) == 62, "NOTES: target shown");
    }
    // CHORD: UP 1 over an F major chord while singing A gives C.
    {
        Engine e; e.prepare (sr, 256);
        EngineSettings s;
        s.dry = 0.0f; s.vocoderLevel = 0.0f; s.doubleLevel = 0.0f; s.gateDb = -80.0f;
        s.harmonyMode = ChordMode;
        s.humanize = 0.0f;
        s.voices[0].level = 1.0f;
        s.voices[0].interval = Up1;
        const auto r = runEngine (e, s, voice, sr, { on (53), on (57), on (60) });
        const float got = measurePitch (r.l, sr);
        check (std::abs (got - 60.0f) < 0.25f, "CHORD: UP 1 over F sings C (got " + std::to_string (got) + ")");
        const float lvl = gainToDb (rms (r.l.data() + skip, (int) r.l.size() - skip) / rms (voice.data() + skip, (int) voice.size() - skip));
        check (lvl > -6.0f && lvl < 6.0f, "harmony voice level is near the voice (" + std::to_string (lvl) + " dB)");
    }
    // Everything on, loud, stays in range.
    {
        Engine e; e.prepare (sr, 256);
        EngineSettings s;
        s.inputDb = 12.0f;
        s.output = 4.0f;
        for (auto& v : s.voices) v.level = 1.0f;
        s.voices[1].interval = Up2; s.voices[2].interval = Down1; s.voices[3].interval = OctDown;
        s.doubleLevel = 1.0f;
        s.humanize = 1.0f;
        s.harmonyFormant = 6.0f;
        const auto r = runEngine (e, s, voice, sr, { on (48), on (52), on (55) });
        check (finiteAndBounded (r.l) && finiteAndBounded (r.r), "everything up stays finite and under full scale");
    }
    // The double sits near unison pitch and level.
    {
        Engine e; e.prepare (sr, 256);
        EngineSettings s;
        s.dry = 0.0f; s.vocoderLevel = 0.0f; s.harmonyLevel = 0.0f; s.gateDb = -80.0f;
        s.doubleLevel = 1.0f;
        const auto r = runEngine (e, s, voice, sr, {});
        const float got = measurePitch (r.l, sr);
        check (std::abs (got - 57.0f) < 0.3f, "double sings with you (got " + std::to_string (got) + ")");
    }
    // The gate keeps quiet rooms quiet.
    {
        Engine e; e.prepare (sr, 256);
        EngineSettings s;
        s.gateDb = -40.0f;
        Rng r (8);
        std::vector<float> hiss ((size_t) sr);
        for (auto& v : hiss) v = r.bi() * 0.001f;   // about -66 dB
        const auto o = runEngine (e, s, hiss, sr, { on (60) });
        check (rms (o.l.data() + skip, (int) o.l.size() - skip) < 1.0e-5f, "gate shuts out room noise");
    }
}
} // namespace

int main (int argc, char** argv)
{
    verbose = argc > 1 && std::strcmp (argv[1], "-v") == 0;
    testMusic();
    testPitchTracker();
    testShifter();
    testVocoder();
    testHarmonyEngine();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
