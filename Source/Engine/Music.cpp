#include "Music.h"

#include <algorithm>
#include <cstdlib>

namespace descant
{
// ---------------------------------------------------------------------------
// Scales
// ---------------------------------------------------------------------------
const char* scaleName (int s)
{
    static const char* n[] { "MAJOR", "MINOR", "DORIAN", "MIXOLYDIAN", "PHRYGIAN", "LYDIAN", "HARM MINOR", "CHROMATIC" };
    return n[std::clamp (s, 0, (int) NumScales - 1)];
}

const std::vector<int>& scaleSteps (int s)
{
    static const std::vector<int> steps[] {
        { 0, 2, 4, 5, 7, 9, 11 },
        { 0, 2, 3, 5, 7, 8, 10 },
        { 0, 2, 3, 5, 7, 9, 10 },
        { 0, 2, 4, 5, 7, 9, 10 },
        { 0, 1, 3, 5, 7, 8, 10 },
        { 0, 2, 4, 6, 7, 9, 11 },
        { 0, 2, 3, 5, 7, 8, 11 },
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } };
    return steps[std::clamp (s, 0, (int) NumScales - 1)];
}

const char* pitchName (int pc)
{
    static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((pc % 12) + 12) % 12];
}

std::string noteName (int note)
{
    if (note < 0 || note > 127) return {};
    return std::string (pitchName (note)) + std::to_string (note / 12 - 2);
}

namespace
{
bool inScale (int note, int key, int scale)
{
    const int pc = ((note - key) % 12 + 12) % 12;
    for (int s : scaleSteps (scale))
        if (s == pc) return true;
    return false;
}
} // namespace

int snapToScale (int note, int key, int scale)
{
    for (int d = 0; d < 12; ++d)
    {
        if (inScale (note - d, key, scale)) return note - d;
        if (inScale (note + d, key, scale)) return note + d;
    }
    return note;
}

int scaleStep (int note, int steps, int key, int scale)
{
    int n = snapToScale (note, key, scale);
    const int dir = steps >= 0 ? 1 : -1;
    for (int k = 0; k != steps; k += dir)
    {
        do n += dir; while (! inScale (n, key, scale));
    }
    return n;
}

// ---------------------------------------------------------------------------
// Chords
// ---------------------------------------------------------------------------
PitchSet pitchSet (const std::vector<int>& notes)
{
    PitchSet p;
    for (int n : notes) p.set ((size_t) (((n % 12) + 12) % 12));
    return p;
}

std::string chordName (const std::vector<int>& notes)
{
    if (notes.empty()) return {};
    const int bass = *std::min_element (notes.begin(), notes.end());
    const auto set = pitchSet (notes);
    if (set.count() == 1) return pitchName (bass);

    struct Shape { const char* suffix; std::vector<int> iv; };
    static const std::vector<Shape> shapes {
        { "", { 0, 4, 7 } }, { "m", { 0, 3, 7 } }, { "dim", { 0, 3, 6 } }, { "aug", { 0, 4, 8 } },
        { "sus2", { 0, 2, 7 } }, { "sus4", { 0, 5, 7 } }, { "5", { 0, 7 } },
        { "6", { 0, 4, 7, 9 } }, { "m6", { 0, 3, 7, 9 } }, { "7", { 0, 4, 7, 10 } }, { "M7", { 0, 4, 7, 11 } },
        { "m7", { 0, 3, 7, 10 } }, { "mM7", { 0, 3, 7, 11 } }, { "m7b5", { 0, 3, 6, 10 } }, { "dim7", { 0, 3, 6, 9 } },
        { "7sus4", { 0, 5, 7, 10 } }, { "add9", { 0, 2, 4, 7 } }, { "madd9", { 0, 2, 3, 7 } },
        { "9", { 0, 2, 4, 7, 10 } }, { "M9", { 0, 2, 4, 7, 11 } }, { "m9", { 0, 2, 3, 7, 10 } }, { "69", { 0, 2, 4, 7, 9 } },
        { "m69", { 0, 2, 3, 7, 9 } }, { "9sus4", { 0, 2, 5, 7, 10 } }, { "11", { 0, 2, 5, 7, 10 } },
        { "m11", { 0, 2, 3, 5, 7, 10 } }, { "13", { 0, 2, 4, 7, 9, 10 } }, { "M7#11", { 0, 4, 6, 7, 11 } },
        { "M9#11", { 0, 2, 4, 6, 7, 11 } }, { "7b9", { 0, 1, 4, 7, 10 } }, { "7#9", { 0, 3, 4, 7, 10 } },
        { "m9", { 0, 2, 3, 7, 10 } }, { "aug7", { 0, 4, 8, 10 } } };

    int bestScore = -1000, bestRoot = -1;
    const char* bestSuffix = "";
    for (int root = 0; root < 12; ++root)
    {
        if (! set.test ((size_t) root)) continue;
        for (const auto& s : shapes)
        {
            PitchSet want;
            for (int iv : s.iv) want.set ((size_t) ((root + iv) % 12));
            // The fifth may be left out of a chord of four notes or more.
            PitchSet required = want;
            if (s.iv.size() >= 4) required.reset ((size_t) ((root + 7) % 12));
            if ((required & ~set).any()) continue;
            const int extras = (int) (set & ~want).count();
            if (extras > 0) continue;
            int score = (int) s.iv.size() * 2 + (root == bass % 12 ? 3 : 0) - (int) (want & ~set).count();
            if (score > bestScore) { bestScore = score; bestRoot = root; bestSuffix = s.suffix; }
        }
    }
    if (bestRoot < 0)
    {
        std::string out;
        for (int pc = 0; pc < 12; ++pc)
            if (set.test ((size_t) ((bass + pc) % 12)))
                out += (out.empty() ? "" : " ") + std::string (pitchName (bass + pc));
        return out;
    }
    std::string name = std::string (pitchName (bestRoot)) + bestSuffix;
    if (bestRoot != bass % 12) name += "/" + std::string (pitchName (bass));
    return name;
}

// ---------------------------------------------------------------------------
// Harmony
// ---------------------------------------------------------------------------
const char* intervalLabel (int interval, int mode)
{
    static const char* chordLabels[] { "UP 1", "UP 2", "UP 3", "DOWN 1", "DOWN 2", "DOWN 3", "OCT UP", "OCT DOWN", "UNISON" };
    static const char* scaleLabels[] { "3RD UP", "5TH UP", "6TH UP", "3RD DOWN", "4TH DOWN", "6TH DOWN", "OCT UP", "OCT DOWN", "UNISON" };
    interval = std::clamp (interval, 0, (int) NumIntervals - 1);
    return mode == ScaleMode ? scaleLabels[interval] : chordLabels[interval];
}

int harmonyNote (const HarmonyContext& ctx, int sung, int interval, int voice)
{
    if (ctx.mode == NotesMode)
        return voice >= 0 && voice < (int) ctx.chord.size() ? ctx.chord[(size_t) voice] : -1;
    if (sung < 0) return -1;
    if (interval == OctUp)   return sung + 12;
    if (interval == OctDown) return sung - 12;
    if (interval == Unison)  return sung;

    const int count = interval <= Up3 ? interval - Up1 + 1 : interval - Down1 + 1;
    const int dir = interval <= Up3 ? 1 : -1;

    if (ctx.mode == ChordMode)
    {
        auto set = pitchSet (ctx.chord);
        set.reset ((size_t) (sung % 12));
        if (set.any())
        {
            int n = sung, found = 0;
            for (int step = 0; step < 36; ++step)
            {
                n += dir;
                if (set.test ((size_t) (((n % 12) + 12) % 12)) && ++found == count)
                    return n;
            }
        }
        // No chord: harmonise in the key instead.
    }

    static const int upSteps[] { 2, 4, 5 };
    static const int downSteps[] { -2, -3, -5 };
    const int steps = dir > 0 ? upSteps[count - 1] : downSteps[count - 1];
    return scaleStep (sung, steps, ctx.key, ctx.scale);
}

// ---------------------------------------------------------------------------
// NoteState
// ---------------------------------------------------------------------------
void NoteState::noteOn (int note, float velocity)
{
    note &= 127;
    if (down.none() && hold)
    {
        latched.reset();   // a fresh hand on the keys starts a new chord
    }
    down.set ((size_t) note);
    vel[(size_t) note] = velocity;
    order.erase (std::remove (order.begin(), order.end(), note), order.end());
    order.push_back (note);
    last = note;
    if (hold) latched.set ((size_t) note);
    rebuild();
    if (! soundingNotes.empty()) memory = soundingNotes;
}

void NoteState::noteOff (int note)
{
    note &= 127;
    if (! down.test ((size_t) note)) return;
    down.reset ((size_t) note);
    order.erase (std::remove (order.begin(), order.end(), note), order.end());
    if (mono && ! hold && ! order.empty()) last = order.back();
    rebuild();
}

void NoteState::allOff()
{
    down.reset();
    latched.reset();
    order.clear();
    rebuild();
}

void NoteState::setHold (bool on)
{
    if (on == hold) return;
    hold = on;
    latched = on ? down : std::bitset<128> {};
    rebuild();
}

NoteState::NoteState()
{
    // Room for every key, so nothing allocates on the audio thread.
    order.reserve (128);
    soundingNotes.reserve (128);
    memory.reserve (128);
    scratch.reserve (128);
}

void NoteState::rebuild()
{
    auto& next = scratch;
    next.clear();
    if (mono)
    {
        if (! order.empty()) next.push_back (order.back());
        else if (hold && latched.any() && last >= 0) next.push_back (last);
    }
    else
    {
        const auto& src = hold ? latched : down;
        for (int n = 0; n < 128; ++n)
            if (src.test ((size_t) n)) next.push_back (n);
    }
    if (next != soundingNotes)
    {
        soundingNotes.swap (next);
        ++changes;
        steadyMs = 0.0f;
    }
}

void NoteState::advance (float ms)
{
    // A chord that has sat still for a moment becomes the remembered chord.
    // Notes let go one at a time don't: the full chord stays remembered.
    if (soundingNotes.empty() || steadyMs >= kSettleMs) return;
    steadyMs += ms;
    if (steadyMs >= kSettleMs) memory = soundingNotes;
}
} // namespace descant
