#include "Vocoder.h"

namespace descant
{
void Vocoder::prepare (double sampleRate)
{
    sr = sampleRate;
    designed = false;
    hiss1.highpass (5200.0, 0.54, sr);
    hiss2.highpass (5200.0, 1.31, sr);
    carrierLevel.set (3.0f, 120.0f, sr);
    reset();
}

void Vocoder::reset()
{
    for (auto& b : bands)
    {
        b.m1.reset(); b.m2.reset(); b.c1.reset(); b.c2.reset();
        b.modEnv.env = b.carEnv.env = 0.0f;
    }
    shown.fill (0.0f);
    peak.fill (0.0f);
    hiss1.reset();
    hiss2.reset();
    carrierLevel.env = 0.0f;
    noiseLp = 0.0f;
}

void Vocoder::design (const VocoderSettings& s)
{
    const int nb = std::clamp (s.bands, 4, kMaxBands);
    const float lo = clampf (s.lowHz, 40.0f, 1000.0f);
    const float hi = clampf (s.highHz, lo * 4.0f, (float) (sr * 0.45));
    const float span = std::log2 (hi / lo);
    const float bwOct = span / (float) nb * (0.55f + 1.6f * clampf (s.width, 0.0f, 1.0f));
    const double q = 1.0 / (2.0 * std::sinh (std::log (2.0) * 0.5 * bwOct));
    const float shift = semisToRatio (clampf (s.formant, -12.0f, 12.0f));

    // React: 0 = 18 ms / 260 ms, 1 = 0.6 ms / 9 ms.
    const float r = clampf (s.react, 0.0f, 1.0f);
    const float att = expMap (r, 18.0f, 0.6f);
    const float rel = expMap (r, 260.0f, 9.0f);

    for (int b = 0; b < kMaxBands; ++b)
    {
        auto& band = bands[(size_t) b];
        if (b >= nb) { band.live = false; centre[(size_t) b] = 0.0f; continue; }
        const float fc = lo * std::pow (hi / lo, ((float) b + 0.5f) / (float) nb);
        centre[(size_t) b] = fc;
        band.m1.bandpass (fc, q, sr);
        band.m2.bandpass (fc, q, sr);
        const float cf = fc * shift;
        band.live = cf < sr * 0.45 && cf > 25.0f;
        if (band.live)
        {
            band.c1.bandpass (cf, q, sr);
            band.c2.bandpass (cf, q, sr);
        }
        band.modEnv.set (att, rel, sr);
        band.carEnv.set (1.5f, 40.0f, sr);
        band.tilt = dbToGain (clampf (s.bright, -1.0f, 1.0f) * 4.0f * std::log2 (fc / 1000.0f));
        const float sp = clampf (s.spread, 0.0f, 1.0f);
        band.panL = (b & 1) ? 1.0f : 1.0f - sp;
        band.panR = (b & 1) ? 1.0f - sp : 1.0f;
    }
    current = s;
    designed = true;
}

void Vocoder::process (const float* mod, const float* car, float* outL, float* outR, int n,
                       const VocoderSettings& s, float unvoiced, float gain)
{
    if (! designed || s.bands != current.bands || s.lowHz != current.lowHz || s.highHz != current.highHz
        || s.formant != current.formant || s.width != current.width || s.react != current.react
        || s.bright != current.bright || s.spread != current.spread)
        design (s);
    current.freeze = s.freeze;
    current.consonants = s.consonants;

    const int nb = current.bands;
    const float cons = clampf (s.consonants, 0.0f, 1.0f);
    const float noiseMix = clampf (unvoiced, 0.0f, 1.0f) * cons;
    // Overall level: wider bands overlap more, more bands add up more.
    const float norm = gain * 4.2f / std::sqrt ((float) nb * (0.45f + 0.9f * current.width)) * std::pow ((float) nb / 8.0f, 0.6f);

    for (int b = 0; b < nb; ++b) peak[(size_t) b] = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float cl = carrierLevel.process (car[i]);
        const float carrierOn = clampf (cl * 12.0f, 0.0f, 1.0f);   // no notes, no vocoder
        // White-ish noise, slightly darkened, at the carrier's level.
        const float w = noise.bi();
        noiseLp += 0.5f * (w - noiseLp);
        const float nz = (w - 0.3f * noiseLp) * std::max (cl, 0.02f) * 1.6f;
        const float c = car[i] * (1.0f - noiseMix) + nz * noiseMix * carrierOn;
        const float floor = 0.02f * cl + 1.0e-6f;
        const float m = mod[i];

        float l = 0.0f, r = 0.0f;
        for (int b = 0; b < nb; ++b)
        {
            auto& band = bands[(size_t) b];
            const float mb = band.m2.process (band.m1.process (m));
            const float e = current.freeze ? band.modEnv.env : band.modEnv.process (mb);
            if (! band.live) continue;
            const float cb = band.c2.process (band.c1.process (c));
            const float ce = band.carEnv.process (cb);
            const float y = cb * (e / (ce + floor)) * band.tilt;
            l += y * band.panL;
            r += y * band.panR;
            peak[(size_t) b] = std::max (peak[(size_t) b], e * band.tilt);
        }
        const float hs = hiss2.process (hiss1.process (m)) * cons * 0.8f * carrierOn;
        outL[i] += l * norm + hs * gain;
        outR[i] += r * norm + hs * gain;
    }
    for (int b = 0; b < nb; ++b)
        shown[(size_t) b] = peak[(size_t) b];
}
} // namespace descant
