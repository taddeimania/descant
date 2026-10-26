// Music.h — notes, keys, scales, chord names and the harmony rules.
#pragma once

#include <array>
#include <bitset>
#include <string>
#include <vector>

namespace descant
{
// ---------------------------------------------------------------------------
// Scales
// ---------------------------------------------------------------------------
enum Scale { Major = 0, Minor, Dorian, Mixolydian, Phrygian, Lydian, HarmonicMinor, Chromatic, NumScales };
const char* scaleName (int scale);              // "MAJOR", ...
const std::vector<int>& scaleSteps (int scale);  // semitones above the key, ascending
const char* pitchName (int pitchClass);          // "C", "C#", ...
std::string noteName (int midiNote);             // "A3" (middle C is C3, as in Live)

// Nearest note in the key and scale; ties go down.
int snapToScale (int note, int key, int scale);
// The note `steps` scale steps from `note` (snapped first).
int scaleStep (int note, int steps, int key, int scale);

// ---------------------------------------------------------------------------
// Chords
// ---------------------------------------------------------------------------
using PitchSet = std::bitset<12>;
PitchSet pitchSet (const std::vector<int>& notes);
// "Am7", "C/E", "F#sus4"... Empty if there's nothing to name. One note names itself.
std::string chordName (const std::vector<int>& notes);

// ---------------------------------------------------------------------------
// Harmony
// ---------------------------------------------------------------------------
enum HarmonyMode { ChordMode = 0, ScaleMode, NotesMode, NumHarmonyModes };
enum Interval { Up1 = 0, Up2, Up3, Down1, Down2, Down3, OctUp, OctDown, Unison, NumIntervals };

// Panel label for an interval in a mode: CHORD "UP 1", SCALE "3RD UP".
const char* intervalLabel (int interval, int mode);

struct HarmonyContext
{
    int mode = ChordMode;
    int key = 0, scale = Major;
    std::vector<int> chord;     // the chord to harmonise with (held or remembered), ascending
};

// The note harmony voice `voice` sings against `sung`, or -1 for silence.
// CHORD: UP n is the nth chord tone above what you sing; DOWN n the nth below.
//        Without a chord it falls back to SCALE.
// SCALE: diatonic 3rd, 5th, 6th above; 3rd, 4th, 6th below.
// NOTES: voice v sings the vth note of the chord, lowest first. `sung` is ignored.
// OCT UP, OCT DOWN and UNISON are the same in every mode except NOTES.
int harmonyNote (const HarmonyContext&, int sung, int interval, int voice);

// ---------------------------------------------------------------------------
// The keys you're holding, with HOLD (latch) and a memory of the last chord.
// ---------------------------------------------------------------------------
class NoteState
{
public:
    NoteState();
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allOff();
    void setHold (bool on);
    void setMono (bool on) { mono = on; }
    void advance (float ms);   // call as time passes

    // What should sound now: held keys, or the latched chord with HOLD.
    const std::vector<int>& sounding() const noexcept { return soundingNotes; }
    float velocity (int note) const noexcept { return vel[(size_t) (note & 127)]; }
    // The last chord that sounded; kept after you let go.
    const std::vector<int>& lastChord() const noexcept { return memory; }
    // Changes each time `sounding` changes.
    int version() const noexcept { return changes; }
    int lastPressed() const noexcept { return last; }

private:
    void rebuild();

    std::bitset<128> down, latched;
    std::array<float, 128> vel {};
    std::vector<int> order;            // keys down, oldest first (mono priority)
    std::vector<int> soundingNotes, memory, scratch;
    bool hold = false, mono = false;
    float steadyMs = 0.0f;
    static constexpr float kSettleMs = 60.0f;
    int changes = 0, last = -1;
};
} // namespace descant
