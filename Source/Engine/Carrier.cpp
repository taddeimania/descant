#include "Carrier.h"

namespace descant
{
void Carrier::prepare (double sampleRate)
{
    sr = sampleRate;
    reset();
}

void Carrier::reset()
{
    uint32_t seed = 21;
    for (auto& v : voices)
    {
        v = Voice();
        for (auto& p : v.phase) p = rng.uni();
        for (auto& d : v.drift) d = Drift (seed += 7);
    }
}

int Carrier::activeVoices() const noexcept
{
    int c = 0;
    for (const auto& v : voices)
        c += (v.gate || v.env > 1.0e-4f) ? 1 : 0;
    return c;
}

Carrier::Voice* Carrier::allocate()
{
    Voice* best = nullptr;
    // A silent voice, else the quietest releasing one, else the oldest.
    for (auto& v : voices)
        if (! v.gate && v.env <= 1.0e-4f) return &v;
    for (auto& v : voices)
        if (! v.gate && (best == nullptr || v.env < best->env)) best = &v;
    if (best != nullptr) return best;
    best = &voices[0];
    for (auto& v : voices)
        if (v.age < best->age) best = &v;
    return best;
}

void Carrier::setNotes (const int* notes, const float* velocities, int count, bool mono)
{
    if (mono)
    {
        auto& v = voices[0];
        for (size_t i = 1; i < voices.size(); ++i)
            voices[i].gate = false;
        if (count == 0)
        {
            v.gate = false;
            return;
        }
        const int note = notes[0];
        if (! v.gate && v.env <= 1.0e-4f)
            v.pitch = lastPitch;   // glide from the last note played
        v.note = note;
        v.target = (float) note;
        v.vel = velocities[0];
        v.gate = true;
        v.age = ++clock;
        lastPitch = (float) note;
        return;
    }

    // Release voices whose note is gone.
    for (auto& v : voices)
    {
        if (! v.gate) continue;
        bool keep = false;
        for (int i = 0; i < count; ++i)
            keep = keep || notes[i] == v.note;
        if (! keep) v.gate = false;
    }
    // Start the new ones.
    for (int i = 0; i < count; ++i)
    {
        bool playing = false;
        for (const auto& v : voices)
            playing = playing || (v.gate && v.note == notes[i]);
        if (playing) continue;
        auto* v = allocate();
        if (! v->gate && v->env <= 1.0e-4f)
            v->pitch = (float) notes[i];
        v->note = notes[i];
        v->target = (float) notes[i];
        v->vel = velocities[i];
        v->gate = true;
        v->age = ++clock;
        lastPitch = (float) notes[i];
    }
}

void Carrier::render (float* out, int n, const CarrierSettings& s)
{
    for (int i = 0; i < n; ++i) out[i] = 0.0f;

    const float attackStep = s.attackMs <= 0.1f ? 1.0f : (float) (1.0 / (s.attackMs * 0.001 * sr));
    const float releaseCoef = onePoleCoef (std::max (1.0f, s.releaseMs) / 4.6f, sr);   // ~-40 dB at releaseMs
    const float glideCoef = onePoleCoef (s.glideMs / 3.0f, sr);
    const float offset = 12.0f * (float) s.octave + s.bend;
    const int wave = s.wave;
    const float detune = clampf (s.detune, 0.0f, 1.0f);

    static constexpr float superSpread[5] { -1.0f, -0.45f, 0.0f, 0.5f, 1.0f };
    static constexpr float choirSpread[3] { -1.0f, 0.0f, 1.0f };
    const int unison = wave == Super ? 5 : wave == Choir ? 3 : 1;
    const float unisonGain = 1.0f / std::sqrt ((float) unison);

    constexpr int kChunk = 16;   // pitch and drift update rate
    for (auto& v : voices)
    {
        if (! v.gate && v.env <= 1.0e-4f) { v.env = 0.0f; continue; }
        const float level = 1.0f - s.velocity * (1.0f - v.vel);

        for (int start = 0; start < n; start += kChunk)
        {
            const int len = std::min (kChunk, n - start);
            for (int c = 0; c < len; ++c)
                v.pitch = v.target + glideCoef * (v.pitch - v.target);

            // Per-oscillator frequency for this chunk.
            float inc[5];
            for (int u = 0; u < unison; ++u)
            {
                float cents = 0.0f;
                if (wave == Super)
                    cents = superSpread[u] * detune * 35.0f;
                else if (wave == Choir)
                    cents = choirSpread[u] * detune * 14.0f
                          + v.drift[u].advance (len, 5.5f, sr) * (4.0f + 10.0f * detune);
                else
                    cents = v.drift[0].advance (len, 0.7f, sr) * detune * 6.0f;
                const float hz = midiToHz (v.pitch + offset + cents * 0.01f);
                inc[u] = clampf (hz / (float) sr, 0.0f, 0.45f);
            }

            for (int c = 0; c < len; ++c)
            {
                // Envelope.
                if (v.gate) v.env = std::min (1.0f, v.env + attackStep);
                else        v.env *= releaseCoef;

                float x = 0.0f;
                for (int u = 0; u < unison; ++u)
                {
                    float& ph = v.phase[(size_t) u];
                    const float dt = inc[u];
                    if (wave == Square)
                    {
                        float sq = ph < 0.5f ? 1.0f : -1.0f;
                        sq += polyBlep (ph, dt);
                        float t2 = ph + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
                        sq -= polyBlep (t2, dt);
                        x += sq * 0.7f;
                    }
                    else
                        x += (2.0f * ph - 1.0f) - polyBlep (ph, dt);
                    ph += dt;
                    if (ph >= 1.0f) ph -= 1.0f;
                }
                out[start + c] += x * unisonGain * v.env * level * 0.5f;
            }
        }
    }
}
} // namespace descant
