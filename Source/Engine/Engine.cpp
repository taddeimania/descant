#include "Engine.h"

namespace descant
{
namespace
{
// How much each harmony voice's character differs, in semitones of formant,
// when HUMANIZE is up: some darker, some brighter, like different singers.
constexpr float kVariety[kHarmonyVoices] { -1.3f, 0.9f, -0.7f, 1.2f };

// Safety: above 0.9 the level bends smoothly towards 1.0 instead of clipping.
inline float softCeiling (float x)
{
    const float a = std::abs (x);
    if (a <= 0.9f) return x;
    return std::copysign (0.9f + 0.1f * std::tanh ((a - 0.9f) * 10.0f), x);
}

inline void panGains (float pan, float& l, float& r)
{
    const float a = (clampf (pan, -1.0f, 1.0f) + 1.0f) * 0.25f * kPi;
    l = std::cos (a) * 1.41421356f;
    r = std::sin (a) * 1.41421356f;
}
} // namespace

void Engine::prepare (double sampleRate, int maxBlock)
{
    sr = sampleRate;
    maxChunk = 64;
    const auto size = (size_t) std::max (maxChunk, 64);
    for (auto* v : { &mono, &gated, &car, &vocL, &vocR, &tmp, &harmL, &harmR, &dblL, &dblR })
        v->assign (size, 0.0f);
    velocityScratch.assign (128, 0.0f);
    ctx.chord.reserve (128);
    (void) maxBlock;

    tracker.prepare (sr);
    source.prepare (sr);
    for (auto& v : harmony) v.prepare (sr);
    for (auto& v : doubles) v.prepare (sr);
    carrier.prepare (sr);
    vocoder.prepare (sr);
    hpf1.highpass (70.0, 0.54, sr);
    hpf2.highpass (70.0, 1.31, sr);
    gateLevel.set (1.0f, 120.0f, sr);

    uint32_t seed = 101;
    for (auto& d : pitchDrift) d = Drift (seed += 13);
    for (auto& d : timeDrift)  d = Drift (seed += 13);
    for (auto& d : doubleDrift) d = Drift (seed += 13);
    for (auto& d : doubleTime)  d = Drift (seed += 13);
    reset();
}

void Engine::reset()
{
    tracker.reset();
    source.reset();
    for (auto& v : harmony) v.reset();
    for (auto& v : doubles) v.reset();
    carrier.reset();
    vocoder.reset();
    hpf1.reset();
    hpf2.reset();
    gateLevel.env = 0.0f;
    gateGain = 0.0f;
    semis.fill (0.0f);
    voiceGain.fill (0.0f);
    targets.fill (-1);
    doubleGain.fill (0.0f);
    sungQuant = followNote = -1;
    sungFrac = 0.0f;
    unvoiced = 0.0f;
    isSinging = false;
    carrierVersion = -1;
    carrierFollow = -2;
    carrierSource = -1;
    freezeArmed = false;
}

// ---------------------------------------------------------------------------
// MIDI
// ---------------------------------------------------------------------------
void Engine::handle (const MidiEvent& e, const EngineSettings&)
{
    switch (e.type)
    {
        case MidiEvent::NoteOn:
            sustained.reset (e.note);
            notes.noteOn (e.note, e.value);
            break;
        case MidiEvent::NoteOff:
            if (sustain) sustained.set (e.note);
            else notes.noteOff (e.note);
            break;
        case MidiEvent::Bend:
            bend = clampf (e.value, -1.0f, 1.0f);
            break;
        case MidiEvent::Sustain:
            sustain = e.value > 0.5f;
            if (! sustain)
            {
                for (int n = 0; n < 128; ++n)
                    if (sustained.test ((size_t) n)) notes.noteOff (n);
                sustained.reset();
            }
            break;
        case MidiEvent::AllOff:
            sustained.reset();
            sustain = false;
            notes.allOff();
            break;
    }
}

void Engine::updateCarrierNotes (const EngineSettings& s)
{
    const bool follow = s.source == VoiceSource;
    const int f = follow ? followNote : -2;
    const bool monoNow = follow || s.mono;
    if (notes.version() == carrierVersion && f == carrierFollow && monoNow == carrierMono && s.source == carrierSource)
        return;
    carrierVersion = notes.version();
    carrierFollow = f;
    carrierMono = monoNow;
    carrierSource = s.source;

    if (s.source == ExternalSource)
    {
        carrier.setNotes (nullptr, nullptr, 0, false);
        return;
    }
    if (follow)
    {
        const float v = 1.0f;
        if (followNote >= 0) carrier.setNotes (&followNote, &v, 1, true);
        else                 carrier.setNotes (nullptr, nullptr, 0, true);
        return;
    }
    const auto& list = notes.sounding();
    for (size_t i = 0; i < list.size(); ++i)
        velocityScratch[i] = notes.velocity (list[i]);
    carrier.setNotes (list.data(), velocityScratch.data(), (int) list.size(), monoNow);
}

// ---------------------------------------------------------------------------
void Engine::process (const float* inL, const float* inR, const float* scL, const float* scR,
                      float* outL, float* outR, int n, const EngineSettings& s,
                      const MidiEvent* events, int numEvents)
{
    inPeak = outPeak = 0.0f;
    notes.setHold (s.hold);
    notes.setMono (s.mono);

    int ev = 0;
    for (int pos = 0; pos < n;)
    {
        while (ev < numEvents && events[ev].offset <= pos)
            handle (events[ev++], s);
        int end = std::min (n, pos + maxChunk);
        if (ev < numEvents && events[ev].offset < end)
            end = std::max (pos + 1, events[ev].offset);
        const int len = end - pos;
        runChunk (inL + pos, inR != nullptr ? inR + pos : nullptr,
                  scL != nullptr ? scL + pos : nullptr, scR != nullptr ? scR + pos : nullptr,
                  outL + pos, outR + pos, len, s);
        pos = end;
    }
    while (ev < numEvents)
        handle (events[ev++], s);

    soundingMask.reset();
    for (int note : notes.sounding()) soundingMask.set ((size_t) note);
    chordMask.reset();
    for (int note : notes.lastChord()) chordMask.set ((size_t) note);
}

void Engine::runChunk (const float* inL, const float* inR, const float* scL, const float* scR,
                       float* outL, float* outR, int n, const EngineSettings& s)
{
    const float chunkMs = (float) (1000.0 * n / sr);
    notes.advance (chunkMs);

    // --- input, low cut and gate ---------------------------------------------------
    const float inGain = dbToGain (s.inputDb);
    const float gateThresh = s.gateDb <= -79.0f ? 0.0f : dbToGain (s.gateDb);
    const float gateAtt = onePoleCoef (1.0f, sr), gateRel = onePoleCoef (90.0f, sr);
    for (int i = 0; i < n; ++i)
    {
        float x = inR != nullptr ? 0.5f * (inL[i] + inR[i]) : inL[i];
        x = hpf2.process (hpf1.process (x * inGain));
        mono[(size_t) i] = x;
        inPeak = std::max (inPeak, std::abs (x));
        const float lvl = gateLevel.process (x);
        // 4 dB of hysteresis so a held note doesn't chatter at the threshold.
        if (gateIsOpen) gateIsOpen = lvl > gateThresh * 0.63f;
        else            gateIsOpen = lvl > gateThresh;
        const float target = gateIsOpen ? 1.0f : 0.0f;
        gateGain = target + (target > gateGain ? gateAtt : gateRel) * (gateGain - target);
        gated[(size_t) i] = x * gateGain;
    }

    // --- pitch -------------------------------------------------------------------------
    tracker.push (mono.data(), n, gateIsOpen);
    const bool voiced = tracker.voiced();
    const float period = voiced ? tracker.period() : 0.0f;
    for (int i = 0; i < n; ++i)
        source.write (gated[(size_t) i], period);
    const int64_t t0 = source.written() - n;

    if (voiced)
    {
        sungFrac = tracker.midiNote();
        if (sungQuant < 0 || std::abs (sungFrac - (float) sungQuant) > 0.68f)
            sungQuant = (int) std::lround (sungFrac);
        silentMs = 0.0f;
    }
    else
        silentMs += chunkMs;
    isSinging = voiced && gateIsOpen;

    // Breath and consonants: the gate is open but there's no pitch.
    {
        const float target = (gateIsOpen && ! voiced) ? 1.0f : 0.0f;
        const float c = target > unvoiced ? std::exp (-chunkMs / 6.0f) : std::exp (-chunkMs / 35.0f);
        unvoiced = target + c * (unvoiced - target);
    }

    // --- carrier ------------------------------------------------------------------------
    if (s.source == VoiceSource && sungQuant >= 0)
        followNote = std::clamp (snapToScale (sungQuant, s.key, s.scale), 0, 127);
    else if (s.source != VoiceSource)
        followNote = -1;
    updateCarrierNotes (s);

    auto cs = s.carrier;
    cs.bend = bend * 2.0f;
    if (s.source == VoiceSource) cs.glideMs = std::max (cs.glideMs, 30.0f);
    float* c = car.data();
    noSidechain = false;
    if (s.source == ExternalSource)
    {
        if (scL == nullptr) { std::fill (c, c + n, 0.0f); noSidechain = true; }
        else for (int i = 0; i < n; ++i) c[i] = scR != nullptr ? 0.5f * (scL[i] + scR[i]) : scL[i];
    }
    else
        carrier.render (c, n, cs);

    // --- vocoder ------------------------------------------------------------------------
    std::fill (vocL.begin(), vocL.begin() + n, 0.0f);
    std::fill (vocR.begin(), vocR.begin() + n, 0.0f);
    // FREEZE holds what it hears when you press it. Opened with it already on
    // (a saved set), there's nothing to hold yet, so it waits to be pressed.
    if (! s.vocoder.freeze) freezeArmed = true;
    auto vs = s.vocoder;
    vs.freeze = vs.freeze && freezeArmed;
    vocoder.process (gated.data(), c, vocL.data(), vocR.data(), n, vs, unvoiced, 1.0f);

    // --- harmony ------------------------------------------------------------------------
    std::fill (harmL.begin(), harmL.begin() + n, 0.0f);
    std::fill (harmR.begin(), harmR.begin() + n, 0.0f);

    ctx.mode = s.harmonyMode;
    ctx.key = s.key;
    ctx.scale = s.scale;
    const auto& chord = s.harmonyMode == NotesMode ? notes.sounding() : notes.lastChord();
    ctx.chord.assign (chord.begin(), chord.end());

    const float glide = std::exp (-chunkMs / std::max (1.0f, s.harmonyGlideMs));
    const float gainStep = std::exp (-chunkMs / 25.0f);
    const float duck = 1.0f - 0.55f * unvoiced;   // stacked consonants get loud: pull them back
    const float hum = clampf (s.humanize, 0.0f, 1.0f);
    const bool fresh = silentMs > 400.0f;         // long pause: jump to new notes

    for (int v = 0; v < kHarmonyVoices; ++v)
    {
        const auto& hv = s.voices[(size_t) v];
        const int target = sungQuant >= 0 || s.harmonyMode == NotesMode
                               ? harmonyNote (ctx, sungQuant, hv.interval, v) : -1;
        targets[(size_t) v] = target;
        const float drift = pitchDrift[(size_t) v].advance (n, 1.3f, sr);
        const float wobble = timeDrift[(size_t) v].advance (n, 0.4f, sr);

        float want = voiceGain[(size_t) v];
        if (target >= 0 && sungQuant >= 0 && hv.level > 0.0f)
        {
            float st = s.harmonyMode == NotesMode ? (float) target - sungFrac : (float) (target - sungQuant);
            st += drift * hum * 0.16f;   // up to about 16 cents of drift
            if (fresh || std::abs (semis[(size_t) v] - st) > 24.0f) semis[(size_t) v] = st;
            else semis[(size_t) v] = st + glide * (semis[(size_t) v] - st);
            want = hv.level;
        }
        else
            want = 0.0f;
        voiceGain[(size_t) v] = want + gainStep * (voiceGain[(size_t) v] - want);
        if (voiceGain[(size_t) v] < 1.0e-4f && want == 0.0f)
        {
            voiceGain[(size_t) v] = 0.0f;
            continue;
        }

        ShiftVoice::Control ctl;
        ctl.ratio = semisToRatio (semis[(size_t) v]);
        ctl.formant = semisToRatio (s.harmonyFormant + kVariety[v] * hum);
        ctl.delayMs = hum * (4.0f + 14.0f * (0.5f + 0.5f * wobble));
        std::fill (tmp.begin(), tmp.begin() + n, 0.0f);
        harmony[(size_t) v].render (source, t0, n, ctl, tmp.data(), voiceGain[(size_t) v] * duck);
        float gl, gr;
        panGains (hv.pan, gl, gr);
        for (int i = 0; i < n; ++i)
        {
            harmL[(size_t) i] += tmp[(size_t) i] * gl;
            harmR[(size_t) i] += tmp[(size_t) i] * gr;
        }
    }

    // --- double ---------------------------------------------------------------------------
    std::fill (dblL.begin(), dblL.begin() + n, 0.0f);
    std::fill (dblR.begin(), dblR.begin() + n, 0.0f);
    for (int d = 0; d < 2; ++d)
    {
        const float want = s.doubleLevel;
        doubleGain[(size_t) d] = want + gainStep * (doubleGain[(size_t) d] - want);
        const float drift = doubleDrift[(size_t) d].advance (n, 0.9f, sr);
        const float wob = doubleTime[(size_t) d].advance (n, 0.35f, sr);
        if (doubleGain[(size_t) d] < 1.0e-4f && want == 0.0f) { doubleGain[(size_t) d] = 0.0f; continue; }

        const float side = d == 0 ? 1.0f : -1.0f;
        const float cents = side * clampf (s.doubleDetune, 0.0f, 1.0f) * 14.0f + drift * (3.0f + 9.0f * s.doubleDetune);
        ShiftVoice::Control ctl;
        ctl.ratio = std::pow (2.0f, cents / 1200.0f);
        ctl.formant = semisToRatio (side * 0.35f * s.doubleDetune);
        const float loose = clampf (s.doubleLoose, 0.0f, 1.0f);
        ctl.delayMs = 9.0f + (d == 0 ? 6.0f : 0.0f) + loose * (22.0f + 14.0f * (0.5f + 0.5f * wob));
        std::fill (tmp.begin(), tmp.begin() + n, 0.0f);
        doubles[(size_t) d].render (source, t0, n, ctl, tmp.data(), doubleGain[(size_t) d] * (1.0f - 0.3f * unvoiced));
        float gl, gr;
        panGains (side * clampf (s.doubleWidth, 0.0f, 1.0f), gl, gr);
        for (int i = 0; i < n; ++i)
        {
            dblL[(size_t) i] += tmp[(size_t) i] * gl * 0.75f;
            dblR[(size_t) i] += tmp[(size_t) i] * gr * 0.75f;
        }
    }

    // --- mix ----------------------------------------------------------------------------------
    for (int i = 0; i < n; ++i)
    {
        const float dry = gated[(size_t) i] * s.dry;
        float l = (dry + vocL[(size_t) i] * s.vocoderLevel + harmL[(size_t) i] * s.harmonyLevel + dblL[(size_t) i]) * s.output;
        float r = (dry + vocR[(size_t) i] * s.vocoderLevel + harmR[(size_t) i] * s.harmonyLevel + dblR[(size_t) i]) * s.output;
        l = softCeiling (l);
        r = softCeiling (r);
        outL[i] = l;
        outR[i] = r;
        outPeak = std::max (outPeak, std::max (std::abs (l), std::abs (r)));
    }
}
} // namespace descant
