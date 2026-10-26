#include "PitchTracker.h"

namespace descant
{
void PitchTracker::prepare (double sampleRate)
{
    sr = sampleRate;
    factor = std::max (1, (int) std::lround (sr / 11025.0));
    dsr = sr / factor;
    aa1.lowpass (dsr * 0.36, 0.54, sr);
    aa2.lowpass (dsr * 0.36, 1.31, sr);
    minLag = std::max (2, (int) std::floor (dsr / kMaxHz));
    maxLag = (int) std::ceil (dsr / kMinHz) + 2;
    window = std::max (maxLag + 16, (int) (dsr * 0.022));
    hop = std::max (16, (int) (dsr * 0.004));
    ring.assign ((size_t) (window + maxLag + 4), 0.0f);
    frame.assign (ring.size(), 0.0f);
    diff.assign ((size_t) maxLag + 2, 0.0f);
    reset();
}

void PitchTracker::reset()
{
    aa1.reset();
    aa2.reset();
    std::fill (ring.begin(), ring.end(), 0.0f);
    ringPos = phase = sinceHop = 0;
    isVoiced = false;
    periodSamples = 0.0f;
    lastClarity = 0.0f;
    history[0] = history[1] = history[2] = 0.0f;
    unvoicedRun = voicedRun = 0;
}

void PitchTracker::push (const float* x, int n, bool gateOpen)
{
    gate = gateOpen;
    const int size = (int) ring.size();
    for (int i = 0; i < n; ++i)
    {
        const float y = aa2.process (aa1.process (x[i]));
        if (++phase >= factor)
        {
            phase = 0;
            ring[(size_t) ringPos] = y;
            ringPos = (ringPos + 1) % size;
            if (++sinceHop >= hop)
            {
                sinceHop = 0;
                analyse();
            }
        }
    }
}

void PitchTracker::analyse()
{
    const int size = (int) ring.size();
    const int need = window + maxLag;
    // Oldest first.
    int start = ringPos - need;
    while (start < 0) start += size;
    float energy = 0.0f;
    for (int i = 0; i < need; ++i)
    {
        frame[(size_t) i] = ring[(size_t) ((start + i) % size)];
        if (i < window) energy += frame[(size_t) i] * frame[(size_t) i];
    }

    bool found = false;
    float bestTau = 0.0f, bestVal = 1.0f;
    if (energy / (float) window > 1.0e-8f)
    {
        // Difference function and its cumulative mean normalisation.
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            const float* a = frame.data();
            const float* b = frame.data() + tau;
            float d = 0.0f;
            for (int j = 0; j < window; ++j)
            {
                const float e = a[j] - b[j];
                d += e * e;
            }
            running += d;
            diff[(size_t) tau] = running > 0.0 ? (float) (d * tau / running) : 1.0f;
        }

        // First dip under the threshold, then down to its floor.
        int tau = -1;
        constexpr float threshold = 0.14f;
        for (int t = minLag; t < maxLag; ++t)
            if (diff[(size_t) t] < threshold)
            {
                while (t + 1 < maxLag && diff[(size_t) t + 1] < diff[(size_t) t]) ++t;
                tau = t;
                break;
            }
        if (tau < 0)
        {
            tau = minLag;
            for (int t = minLag + 1; t < maxLag; ++t)
                if (diff[(size_t) t] < diff[(size_t) tau]) tau = t;
        }

        // Parabolic refinement.
        float refined = (float) tau;
        if (tau > 1 && tau < maxLag)
        {
            const float s0 = diff[(size_t) tau - 1], s1 = diff[(size_t) tau], s2 = diff[(size_t) tau + 1];
            const float den = s0 - 2.0f * s1 + s2;
            if (std::abs (den) > 1.0e-9f)
                refined += clampf (0.5f * (s0 - s2) / den, -0.5f, 0.5f);
        }
        bestTau = refined;
        bestVal = diff[(size_t) tau];
        found = bestVal < 0.3f;
    }

    lastClarity = clampf (1.0f - bestVal, 0.0f, 1.0f);
    const bool frameVoiced = found && gate;
    if (frameVoiced)
    {
        const float p = bestTau * (float) factor;
        history[0] = history[1];
        history[1] = history[2];
        history[2] = p;
        ++voicedRun;
        unvoicedRun = 0;
        if (voicedRun >= 2)
        {
            // Median of three steadies octave slips.
            float a = history[0] > 0.0f ? history[0] : p, b = history[1] > 0.0f ? history[1] : p, c = p;
            const float med = std::max (std::min (a, b), std::min (std::max (a, b), c));
            periodSamples = med;
            isVoiced = true;
        }
    }
    else
    {
        voicedRun = 0;
        if (++unvoicedRun >= 3)
        {
            isVoiced = false;
            history[0] = history[1] = history[2] = 0.0f;
        }
    }
}
} // namespace descant
