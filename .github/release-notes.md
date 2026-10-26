Descant is a vocoder and vocal harmoniser for Ableton Live. Sing into a
microphone, play notes on a keyboard or send chords from Posy, and your words
go on the synth while up to four harmony voices and a double sing along in
your own voice.

## What's in 0.1.0

- **Vocoder**: 8 to 40 bands, SAW, SQUARE, SUPER and CHOIR waves, FORMANT,
  WIDTH, REACT, BRIGHT, CONSONANTS, SPREAD and FREEZE.
- **Three carriers**: KEYS (MIDI notes or chords, with MONO, GLIDE, HOLD and
  the sustain pedal), VOICE (the synth follows your singing, snapped to KEY
  and SCALE) and EXTERNAL (shape another track through the sidechain).
- **Harmony**: four voices made from your own voice. CHORD follows the chord
  you play (and remembers it after you let go), SCALE harmonises in a key,
  NOTES makes your voice sing exactly the notes you hold.
- **Double**: two more takes of you, tight to loose.
- 20 factory presets, including **Posy Pads** for use with Posy.

## Install

Unzip and copy `Descant.vst3` into your VST3 folder (macOS
`~/Library/Audio/Plug-Ins/VST3`, Windows `C:\Program Files\Common Files\VST3`,
Linux `~/.vst3`), then rescan plug-ins in Live. Put it on the audio track your
microphone comes in on. The getting started guide (PDF) in the download walks
you through the MIDI routing.

**macOS:** the plugin isn't notarised. If macOS refuses to load it, run
`xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Descant.vst3`.
