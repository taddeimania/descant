#include "Shifter.h"

namespace descant
{
namespace
{
int64_t nextPow2 (int64_t v)
{
    int64_t p = 1;
    while (p < v) p <<= 1;
    return p;
}
} // namespace

// ---------------------------------------------------------------------------
// GrainSource
// ---------------------------------------------------------------------------
void GrainSource::prepare (double sampleRate)
{
    sr = sampleRate;
    maxHalfLen = (float) (sr / 65.0);
    unvoicedHalf = (float) (sr * 0.005);
    // Long enough for the slowest grain, the loosest double and humanize.
    buf.assign ((size_t) nextPow2 ((int64_t) (sr * 0.35)), 0.0f);
    mask = (int64_t) buf.size() - 1;
    reset();
}

void GrainSource::reset()
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    count = 0;
    head = numMarks = 0;
    nextMark = 0.0;
}

void GrainSource::write (float x, float period)
{
    buf[(size_t) (count & mask)] = x;
    if ((double) count >= nextMark)
    {
        const bool v = period > 0.0f;
        const float half = v ? clampf (period, 8.0f, maxHalfLen) : unvoicedHalf;
        marks[(size_t) head] = { count, half, v };
        head = (head + 1) % kMarks;
        numMarks = std::min (numMarks + 1, kMarks);
        nextMark = std::max (nextMark + (double) half, (double) count + 1.0);
    }
    ++count;
}

float GrainSource::at (double pos) const noexcept
{
    if (pos < 0.0) return 0.0f;
    const auto i = (int64_t) pos;
    if (i + 1 >= count || count - i >= (int64_t) buf.size() - 1) return 0.0f;
    const float f = (float) (pos - (double) i);
    const float a = buf[(size_t) (i & mask)], b = buf[(size_t) ((i + 1) & mask)];
    return a + f * (b - a);
}

bool GrainSource::pick (double target, double limit, float reach, Mark& out) const
{
    bool any = false;
    double best = 1.0e30;
    const double oldest = (double) (count - (int64_t) buf.size() + 2);
    for (int k = 0; k < numMarks; ++k)
    {
        const auto& m = marks[(size_t) ((head - 1 - k + kMarks) % kMarks)];
        const double r = (double) m.half * reach;
        if ((double) m.pos - r < oldest)
            break;
        if ((double) m.pos + r > limit)
            continue;
        const double d = std::abs ((double) m.pos - target);
        if (d < best)
        {
            best = d;
            out = m;
            any = true;
        }
        else if ((double) m.pos < target)
            break;   // marks only get further away from here on
    }
    return any;
}

// ---------------------------------------------------------------------------
// ShiftVoice
// ---------------------------------------------------------------------------
void ShiftVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    acc.assign ((size_t) nextPow2 ((int64_t) (sr / 65.0 * 6.0) + 64), 0.0f);
    mask = (int64_t) acc.size() - 1;
    reset();
}

void ShiftVoice::reset()
{
    std::fill (acc.begin(), acc.end(), 0.0f);
    nextSynth = -1.0;
    lag = (float) (sr * 0.012);
    started = false;
}

void ShiftVoice::addGrain (const GrainSource& src, const GrainSource::Mark& m, double c, float formant, float gain, int64_t now)
{
    const double H = m.half;
    const auto k0 = std::max (now, (int64_t) std::ceil (c - H));   // never write into the past
    const auto k1 = (int64_t) std::floor (c + H);
    const double invH = 1.0 / H;
    for (int64_t k = k0; k <= k1; ++k)
    {
        const double x = ((double) k - c) * invH;   // -1..1
        if (x <= -1.0 || x >= 1.0) continue;
        const float w = 0.5f + 0.5f * std::cos ((float) x * kPi);
        const float s = src.at ((double) m.pos + ((double) k - c) * formant);
        acc[(size_t) (k & mask)] += w * s * gain;
    }
}

void ShiftVoice::render (const GrainSource& src, int64_t t0, int n, const Control& ctl, float* out, float gain)
{
    const float ratio = clampf (ctl.ratio, 0.25f, 4.0f);
    const float formant = clampf (ctl.formant, 0.5f, 2.0f);
    const float reach = std::max (1.0f, formant);
    const float delay = (float) (ctl.delayMs * 0.001 * sr);
    const float voicedGain = gain / std::sqrt (ratio);

    for (int i = 0; i < n; ++i)
    {
        const int64_t t = t0 + i;
        if (! started || nextSynth < (double) t)
        {
            nextSynth = (double) t + 1.0;
            started = true;
        }

        for (int guard = 0; guard < 64; ++guard)
        {
            GrainSource::Mark m;
            if (! src.pick (nextSynth - lag, (double) t, reach, m))
            {
                // Nothing recorded yet: try again a little later.
                nextSynth = (double) t + 1.0;
                break;
            }
            if (nextSynth - m.half > (double) t)
                break;   // not time for this grain yet

            addGrain (src, m, nextSynth, formant, m.voiced ? voicedGain : gain, t);

            // Steer the lag: just enough to keep whole grains behind the input.
            const float target = m.half * (1.0f + reach) + (float) (sr * 0.0015) + delay;
            lag += (target - lag) * 0.12f;
            nextSynth += m.voiced ? (double) m.half / ratio : (double) m.half;
        }

        const auto idx = (size_t) (t & mask);
        out[i] += acc[idx];
        acc[idx] = 0.0f;
    }
}
} // namespace descant
