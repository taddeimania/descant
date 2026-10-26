#include "Views.h"

namespace descant::ui
{
using namespace juce;

namespace
{
constexpr int kSegments = 16;
} // namespace

// ===========================================================================
// Meter
// ===========================================================================
void Meter::push (float peak)
{
    level = std::max (peak, level * 0.82f);
    if (peak >= hold) { hold = peak; holdFrames = 24; }
    else if (--holdFrames < 0) hold *= 0.9f;

    auto segs = [] (float v)
    {
        if (v <= 0.0f) return 0;
        const float db = gainToDb (v);
        return jlimit (0, kSegments, (int) std::ceil ((db + 48.0f) / 48.0f * kSegments));
    };
    const int s = segs (level), h = segs (hold);
    if (s != shownSegs || h != shownHold)
    {
        shownSegs = s;
        shownHold = h;
        repaint();
    }
}

void Meter::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float segH = r.getHeight() / (float) kSegments;
    for (int i = 0; i < kSegments; ++i)
    {
        const auto seg = Rectangle<float> (r.getX(), r.getBottom() - (float) (i + 1) * segH, r.getWidth(), segH).reduced (0.0f, 1.5f);
        const Colour c = i >= kSegments - 1 ? colours::ledRed : i >= kSegments - 4 ? colours::ledYellow : colours::ledGreen;
        const bool lit = i < shownSegs || i == shownHold - 1;
        g.setColour (lit ? c : colours::socket);
        g.fillRect (seg);
    }
}

// ===========================================================================
// BandDisplay
// ===========================================================================
void BandDisplay::push (const DescantProcessor::Readout& r, bool isFrozen, float lowHz, float highHz, float f)
{
    auto moved = [] (float a, float b) { return std::abs (a - b) > 1.0e-4f; };
    bool changed = r.numBands != bands || isFrozen != frozen || moved (lowHz, lo) || moved (highHz, hi) || moved (f, formant);
    bands = jlimit (1, Vocoder::kMaxBands, r.numBands > 0 ? r.numBands : bands);
    frozen = isFrozen;
    lo = lowHz; hi = highHz; formant = f;
    for (int b = 0; b < Vocoder::kMaxBands; ++b)
    {
        // Bars in dB, -70 to -16 dBFS, falling at a steady rate.
        const float v = r.bands[(size_t) b];
        const float h = v > 1.0e-5f ? jlimit (0.0f, 1.0f, (gainToDb (v) + 70.0f) / 54.0f) : 0.0f;
        const float next = h > level[(size_t) b] ? h : (frozen ? level[(size_t) b] : std::max (h, level[(size_t) b] - 0.035f));
        if (std::abs (next - level[(size_t) b]) > 0.004f) changed = true;
        level[(size_t) b] = next;
    }
    if (changed) repaint();
}

void BandDisplay::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    drawLcdBox (g, r);
    auto area = r.reduced (12.0f, 10.0f).withTrimmedBottom (18.0f).withTrimmedTop (14.0f);

    drawText (g, String (bands) + " BANDS", { r.getX() + 12.0f, r.getY() + 4.0f, 120.0f, 14.0f }, 10.0f, colours::inkDim, true,
              Justification::centredLeft);
    if (frozen)
        drawText (g, "FROZEN", { r.getRight() - 112.0f, r.getY() + 4.0f, 100.0f, 14.0f }, 10.0f, colours::brand, true,
                  Justification::centredRight);
    else if (std::abs (formant) >= 0.05f)
        drawText (g, "FORMANT " + ids::semitones (formant), { r.getRight() - 162.0f, r.getY() + 4.0f, 150.0f, 14.0f }, 10.0f,
                  colours::inkDim, true, Justification::centredRight);

    const float gap = bands > 24 ? 2.0f : 3.0f;
    const float w = (area.getWidth() - gap * (float) (bands - 1)) / (float) bands;
    const int segs = 18;
    const float segH = area.getHeight() / (float) segs;
    for (int b = 0; b < bands; ++b)
    {
        const float x = area.getX() + (float) b * (w + gap);
        const int lit = (int) std::round (level[(size_t) b] * (float) segs);
        for (int s = 0; s < segs; ++s)
        {
            const auto seg = Rectangle<float> (x, area.getBottom() - (float) (s + 1) * segH, w, segH).reduced (0.0f, 1.2f);
            g.setColour (s < lit ? (frozen ? colours::brand : colours::lcd) : colours::socket.withAlpha (0.55f));
            g.fillRect (seg);
        }
    }

    // Frequency labels under the bars.
    auto label = [&] (float hz, float x, Justification j)
    {
        drawText (g, ids::hertz (hz), { x, r.getBottom() - 20.0f, 80.0f, 14.0f }, 10.0f, colours::inkDim, true, j);
    };
    label (lo, area.getX(), Justification::centredLeft);
    label (std::sqrt (lo * hi), area.getCentreX() - 40.0f, Justification::centred);
    label (hi, area.getRight() - 80.0f, Justification::centredRight);
}

// ===========================================================================
// KeyboardView
// ===========================================================================
Rectangle<float> KeyboardView::keyRect (int note) const
{
    const auto r = getLocalBounds().toFloat();
    int whites = 0;
    for (int n = kLow; n <= kHigh; ++n) whites += isBlack (n) ? 0 : 1;
    const float ww = r.getWidth() / (float) whites;
    int idx = 0;
    for (int n = kLow; n < note; ++n) idx += isBlack (n) ? 0 : 1;
    if (! isBlack (note))
        return { r.getX() + (float) idx * ww, r.getY(), ww, r.getHeight() };
    const float bw = ww * 0.62f;
    return { r.getX() + (float) idx * ww - bw * 0.5f, r.getY(), bw, r.getHeight() * 0.6f };
}

void KeyboardView::push (const DescantProcessor::Readout& r, int harmonyMode, int source, const std::array<float, kHarmonyVoices>& levels)
{
    const bool keysPlay = source == KeysSource;
    sounding = keysPlay ? r.sounding : std::bitset<128> {};
    chord = harmonyMode == ChordMode ? r.chord : std::bitset<128> {};
    if (! keysPlay) chord |= r.sounding;
    sung = r.singing ? (int) std::lround (r.sung) : -1;
    carrier = r.carrierNote;
    for (int v = 0; v < kHarmonyVoices; ++v)
        targets[(size_t) v] = (r.singing || harmonyMode == NotesMode) && levels[(size_t) v] > 0.0f ? r.targets[(size_t) v] : -1;

    String k = sounding.to_string().c_str();
    k << "|" << String (chord.to_string().c_str()) << "|" << sung << "|" << carrier;
    for (int t : targets) k << "|" << t;
    if (k != key)
    {
        key = k;
        repaint();
    }
}

void KeyboardView::paint (Graphics& g)
{
    auto fillKey = [&] (int n, bool black)
    {
        const auto kr = keyRect (n);
        const bool on = sounding.test ((size_t) n) || n == carrier;
        Colour face = black ? colours::black : colours::cream;
        if (on) face = black ? colours::lcd.darker (0.15f) : colours::lcd;
        g.setColour (face);
        if (black) g.fillRect (kr);
        else g.fillRect (kr.reduced (1.0f, 0.0f));
        if (! on && chord.test ((size_t) n))
        {
            g.setColour (colours::lcd);
            g.drawRect (black ? kr.reduced (1.0f) : kr.reduced (2.5f, 1.5f), 2.0f);
        }
        if (! black)
        {
            g.setColour (colours::creamShade.darker (0.3f));
            g.drawRect (kr.reduced (1.0f, 0.0f), 1.0f);
        }
    };
    for (int n = kLow; n <= kHigh; ++n)
        if (! isBlack (n)) fillKey (n, false);
    for (int n = kLow; n <= kHigh; ++n)
        if (isBlack (n)) fillKey (n, true);

    // Octave names on the C keys.
    for (int n = kLow; n <= kHigh; n += 12)
    {
        const auto kr = keyRect (n);
        drawText (g, noteName (n), kr.withTrimmedTop (kr.getHeight() - 16.0f), 9.0f,
                  sounding.test ((size_t) n) ? colours::black : colours::inkDim.darker (0.4f), true);
    }

    // Dots: harmony voices, then you on top.
    auto dot = [&] (int n, Colour c, float size, float lift)
    {
        if (n < kLow || n > kHigh) return;
        const auto kr = keyRect (n);
        const float y = isBlack (n) ? kr.getBottom() - 9.0f - lift : kr.getBottom() - 24.0f - lift;
        const auto d = Rectangle<float> (size, size).withCentre ({ kr.getCentreX(), y });
        g.setColour (colours::black);
        g.fillEllipse (d.expanded (1.5f));
        g.setColour (c);
        g.fillEllipse (d);
    };
    for (int v = 0; v < kHarmonyVoices; ++v)
    {
        int stack = 0;
        for (int u = 0; u < v; ++u) stack += targets[(size_t) u] == targets[(size_t) v] ? 1 : 0;
        dot (targets[(size_t) v], colours::voice (v), 9.0f, (float) stack * 11.0f);
    }
    dot (sung, colours::brand, 12.0f, -1.0f);
}

// ===========================================================================
// LcdReadout
// ===========================================================================
void LcdReadout::set (const String& big, const String& small, bool led, Colour c)
{
    if (big != bigText || small != smallText || led != ledOn || c != ledCol)
    {
        bigText = big;
        smallText = small;
        ledOn = led;
        ledCol = c;
        repaint();
    }
}

void LcdReadout::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    drawLcdBox (g, r);
    if (caption.isNotEmpty())
        drawText (g, caption, { r.getX() + 8.0f, r.getY() + 4.0f, r.getWidth() - 30.0f, 12.0f }, 9.5f, colours::inkDim, true,
                  Justification::centredLeft);
    drawLed (g, { r.getRight() - 12.0f, r.getY() + 11.0f }, 4.5f, ledCol, ledOn);
    const float smallH = smallText.isNotEmpty() ? 16.0f : 0.0f;
    auto body = r.reduced (8.0f, 4.0f).withTrimmedTop (caption.isNotEmpty() ? 10.0f : 0.0f);
    if (smallH > 0.0f)
        drawText (g, smallText, body.removeFromBottom (smallH), 10.5f, colours::inkDim, true, Justification::centred);
    g.setColour (bigColour);
    g.setFont (panelFont (bigH, true));
    g.drawFittedText (bigText, body.toNearestInt(), Justification::centred, 1, 0.7f);
}
} // namespace descant::ui
