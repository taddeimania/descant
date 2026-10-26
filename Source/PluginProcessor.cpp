#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

using namespace descant;

DescantProcessor::DescantProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "DESCANT", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* wp = dynamic_cast<juce::RangedAudioParameter*> (p))
            params[wp->getParameterID()] = apvts.getRawParameterValue (wp->getParameterID());

    cacheParams();
    events.reserve (1024);
    currentName = factoryPresets().front().name;
    loadedValues = snapshot();
}

bool DescantProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const auto monoOrStereo = [] (const juce::AudioChannelSet& s)
    {
        return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo();
    };
    if (! monoOrStereo (in) || ! monoOrStereo (out))
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && ! monoOrStereo (sc))
            return false;
    }
    return true;
}

void DescantProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    silence.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);
    scratch.assign (silence.size(), 0.0f);
}

// ---------------------------------------------------------------------------
void DescantProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void DescantProcessor::cacheParams()
{
    auto ptr = [this] (const char* id) { return params.at (id); };
    fast.input = ptr (ids::input);       fast.gate = ptr (ids::gate);
    fast.source = ptr (ids::source);     fast.wave = ptr (ids::wave);       fast.octave = ptr (ids::octave);
    fast.detune = ptr (ids::detune);     fast.glide = ptr (ids::glide);     fast.attack = ptr (ids::attack);
    fast.release = ptr (ids::release);   fast.velocity = ptr (ids::velocity);
    fast.mono = ptr (ids::mono);         fast.hold = ptr (ids::hold);
    fast.bands = ptr (ids::bands);       fast.formant = ptr (ids::formant); fast.width = ptr (ids::width);
    fast.react = ptr (ids::react);       fast.bright = ptr (ids::bright);   fast.consonants = ptr (ids::consonants);
    fast.spread = ptr (ids::spread);     fast.lowHz = ptr (ids::lowHz);     fast.highHz = ptr (ids::highHz);
    fast.freeze = ptr (ids::freeze);
    fast.harmonyMode = ptr (ids::harmonyMode); fast.key = ptr (ids::key); fast.scale = ptr (ids::scale);
    fast.humanize = ptr (ids::humanize); fast.harmonyFormant = ptr (ids::harmonyFormant); fast.harmonyGlide = ptr (ids::harmonyGlide);
    for (int v = 0; v < kHarmonyVoices; ++v)
        fast.voices[(size_t) v] = { ptr (ids::voiceInterval (v)), ptr (ids::voiceLevel (v)), ptr (ids::voicePan (v)) };
    fast.doubleLevel = ptr (ids::doubleLevel); fast.doubleLoose = ptr (ids::doubleLoose);
    fast.doubleDetune = ptr (ids::doubleDetune); fast.doubleWidth = ptr (ids::doubleWidth);
    fast.dry = ptr (ids::dry); fast.vocoderLevel = ptr (ids::vocoderLevel);
    fast.harmonyLevel = ptr (ids::harmonyLevel); fast.output = ptr (ids::output);
}

// Audio thread: no lookups by name here.
void DescantProcessor::readSettings()
{
    auto get = [] (const std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };
    auto geti = [&] (const std::atomic<float>* p) { return juce::roundToInt (get (p)); };
    auto level = [&] (const std::atomic<float>* p) { const float db = get (p); return db <= -59.95f ? 0.0f : dbToGain (db); };

    auto& s = settings;
    s.inputDb = get (fast.input);
    s.gateDb = get (fast.gate);
    s.source = geti (fast.source);
    s.carrier.wave = geti (fast.wave);
    s.carrier.octave = geti (fast.octave) - 2;
    s.carrier.detune = get (fast.detune);
    s.carrier.glideMs = get (fast.glide);
    s.carrier.attackMs = get (fast.attack);
    s.carrier.releaseMs = get (fast.release);
    s.carrier.velocity = get (fast.velocity);
    s.mono = get (fast.mono) > 0.5f;
    s.hold = get (fast.hold) > 0.5f;

    s.vocoder.bands = ids::bandCount (geti (fast.bands));
    s.vocoder.formant = get (fast.formant);
    s.vocoder.width = get (fast.width);
    s.vocoder.react = get (fast.react);
    s.vocoder.bright = get (fast.bright);
    s.vocoder.consonants = get (fast.consonants);
    s.vocoder.spread = get (fast.spread);
    s.vocoder.lowHz = get (fast.lowHz);
    s.vocoder.highHz = get (fast.highHz);
    s.vocoder.freeze = get (fast.freeze) > 0.5f;

    s.harmonyMode = geti (fast.harmonyMode);
    s.key = geti (fast.key);
    s.scale = geti (fast.scale);
    for (int v = 0; v < kHarmonyVoices; ++v)
    {
        const auto& f = fast.voices[(size_t) v];
        auto& hv = s.voices[(size_t) v];
        hv.interval = geti (f.interval);
        const float l = get (f.level);
        hv.level = l * l * 1.4f;   // a knob that feels even; 70% is about the voice's own level
        hv.pan = get (f.pan);
    }
    s.humanize = get (fast.humanize);
    s.harmonyFormant = get (fast.harmonyFormant);
    s.harmonyGlideMs = get (fast.harmonyGlide);

    const float d = get (fast.doubleLevel);
    s.doubleLevel = d * d * 1.4f;
    s.doubleLoose = get (fast.doubleLoose);
    s.doubleDetune = get (fast.doubleDetune);
    s.doubleWidth = get (fast.doubleWidth);

    s.dry = level (fast.dry);
    s.vocoderLevel = level (fast.vocoderLevel);
    s.harmonyLevel = level (fast.harmonyLevel);
    s.output = dbToGain (get (fast.output));
}

void DescantProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    auto mainIn = getBusBuffer (buffer, true, 0);
    auto mainOut = getBusBuffer (buffer, false, 0);
    if (n == 0 || mainOut.getNumChannels() == 0)
        return;

    readSettings();

    // MIDI into engine events.
    events.clear();
    int midiSeen = 0;
    if (panics.load (std::memory_order_relaxed) != panicsSeen)
    {
        panicsSeen = panics.load();
        MidiEvent e; e.type = MidiEvent::AllOff; events.push_back (e);
    }
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        MidiEvent e;
        e.offset = juce::jlimit (0, n - 1, meta.samplePosition);
        if (m.isNoteOn())
        {
            e.type = MidiEvent::NoteOn; e.note = (uint8_t) m.getNoteNumber(); e.value = m.getFloatVelocity();
            ++midiSeen;
        }
        else if (m.isNoteOff())
        {
            e.type = MidiEvent::NoteOff; e.note = (uint8_t) m.getNoteNumber();
        }
        else if (m.isPitchWheel())
        {
            e.type = MidiEvent::Bend; e.value = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
        }
        else if (m.isSustainPedalOn() || m.isSustainPedalOff())
        {
            e.type = MidiEvent::Sustain; e.value = m.isSustainPedalOn() ? 1.0f : 0.0f;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            e.type = MidiEvent::AllOff;
        else
            continue;
        if (events.size() < events.capacity())
            events.push_back (e);
    }
    midi.clear();   // nothing goes on down the chain

    // Inputs. A mono track gives one channel.
    const float* inL = mainIn.getNumChannels() > 0 ? mainIn.getReadPointer (0) : silence.data();
    const float* inR = mainIn.getNumChannels() > 1 ? mainIn.getReadPointer (1) : nullptr;
    const float* scL = nullptr;
    const float* scR = nullptr;
    bool sidechain = false;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0)
        {
            scL = sc.getReadPointer (0);
            scR = sc.getNumChannels() > 1 ? sc.getReadPointer (1) : nullptr;
            sidechain = true;
        }
    }

    // The engine reads all of its input before it writes, block by block, so
    // processing in place is safe; the output's right side may be its own.
    float* outL = mainOut.getWritePointer (0);
    float* outR = mainOut.getNumChannels() > 1 ? mainOut.getWritePointer (1) : nullptr;
    if (outR == nullptr)
    {
        // Mono out: render the right side into scratch and fold.
        if (scratch.size() < (size_t) n) return;
        engine.process (inL, inR, scL, scR, outL, scratch.data(), n, settings, events.data(), (int) events.size());
        for (int i = 0; i < n; ++i) outL[i] = 0.5f * (outL[i] + scratch[(size_t) i]);
    }
    else
        engine.process (inL, inR, scL, scR, outL, outR, n, settings, events.data(), (int) events.size());

    // To the panel.
    float cur = uiIn.load();
    const float ip = engine.inputPeak(), op = engine.outputPeak();
    while (ip > cur && ! uiIn.compare_exchange_weak (cur, ip)) {}
    cur = uiOut.load();
    while (op > cur && ! uiOut.compare_exchange_weak (cur, op)) {}
    uiSung.store (engine.sungNote(), std::memory_order_relaxed);
    uiSinging.store (engine.singing(), std::memory_order_relaxed);
    uiGate.store (engine.gateOpen(), std::memory_order_relaxed);
    uiExtMissing.store (engine.externalMissing(), std::memory_order_relaxed);
    uiSidechain.store (sidechain, std::memory_order_relaxed);
    for (int v = 0; v < kHarmonyVoices; ++v)
        uiTargets[(size_t) v].store (engine.harmonyTarget (v), std::memory_order_relaxed);
    for (int w = 0; w < 4; ++w)
    {
        uint32_t a = 0, b = 0;
        for (int bit = 0; bit < 32; ++bit)
        {
            if (engine.soundingNotes().test ((size_t) (w * 32 + bit))) a |= 1u << bit;
            if (engine.chordNotes().test ((size_t) (w * 32 + bit))) b |= 1u << bit;
        }
        uiSounding[(size_t) w].store (a, std::memory_order_relaxed);
        uiChord[(size_t) w].store (b, std::memory_order_relaxed);
    }
    uiCarrierNote.store (engine.carrierNote(), std::memory_order_relaxed);
    const auto& bank = engine.bank();
    uiBands.store (bank.numBands(), std::memory_order_relaxed);
    for (int b = 0; b < Vocoder::kMaxBands; ++b)
    {
        const float v = b < bank.numBands() ? bank.bandLevel (b) : 0.0f;
        float c = uiBandLevels[(size_t) b].load (std::memory_order_relaxed);
        while (v > c && ! uiBandLevels[(size_t) b].compare_exchange_weak (c, v)) {}
    }
    if (midiSeen > 0) uiMidi.fetch_add (midiSeen, std::memory_order_relaxed);
}

DescantProcessor::Readout DescantProcessor::readout()
{
    Readout r;
    r.sung = uiSung.load();
    r.singing = uiSinging.load();
    r.gateOpen = uiGate.load();
    r.externalMissing = uiExtMissing.load();
    r.sidechainConnected = uiSidechain.load();
    for (int v = 0; v < kHarmonyVoices; ++v) r.targets[(size_t) v] = uiTargets[(size_t) v].load();
    for (int w = 0; w < 4; ++w)
    {
        const uint32_t a = uiSounding[(size_t) w].load(), b = uiChord[(size_t) w].load();
        for (int bit = 0; bit < 32; ++bit)
        {
            if (a & (1u << bit)) r.sounding.set ((size_t) (w * 32 + bit));
            if (b & (1u << bit)) r.chord.set ((size_t) (w * 32 + bit));
        }
    }
    r.carrierNote = uiCarrierNote.load();
    r.numBands = uiBands.load();
    // Band levels are peaks since the last read: take them.
    for (int b = 0; b < Vocoder::kMaxBands; ++b)
        r.bands[(size_t) b] = uiBandLevels[(size_t) b].exchange (0.0f);
    r.midiCount = uiMidi.load();
    return r;
}

// ---------------------------------------------------------------------------
// Presets
// ---------------------------------------------------------------------------
int DescantProcessor::getNumPrograms() { return (int) factoryPresets().size(); }

const juce::String DescantProcessor::getProgramName (int index)
{
    const auto& f = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) f.size()) ? juce::String (f[(size_t) index].name) : juce::String();
}

juce::String DescantProcessor::presetBlurb() const
{
    const auto& f = factoryPresets();
    if (userFile == juce::File() && juce::isPositiveAndBelow (factoryIndex, (int) f.size()))
        return f[(size_t) factoryIndex].blurb;
    return {};
}

std::vector<float> DescantProcessor::snapshot() const
{
    std::vector<float> v;
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID() != ids::freeze)
                v.push_back (rp->getValue());
    return v;
}

bool DescantProcessor::presetEdited() const
{
    const auto now = snapshot();
    if (now.size() != loadedValues.size())
        return true;
    for (size_t i = 0; i < now.size(); ++i)
        if (std::abs (now[i] - loadedValues[i]) > 0.002f)
            return true;
    return false;
}

void DescantProcessor::loadFactory (int index)
{
    const auto& list = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size()))
        return;
    const auto& preset = list[(size_t) index];

    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            float v = rp->getDefaultValue();
            for (const auto& [id, plain] : preset.values)
                if (rp->getParameterID() == id)
                    v = rp->convertTo0to1 (plain);
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (v);
            rp->endChangeGesture();
        }
    factoryIndex = index;
    userFile = juce::File();
    currentName = preset.name;
    loadedValues = snapshot();
}

std::unique_ptr<juce::XmlElement> DescantProcessor::stateXml()
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr)
        return nullptr;
    xml->setAttribute ("presetName", currentName);
    xml->setAttribute ("presetFactory", userFile == juce::File() ? factoryIndex : -1);
    xml->setAttribute ("presetFile", userFile.getFullPathName());
    return xml;
}

void DescantProcessor::applyState (const juce::XmlElement& xml)
{
    if (xml.hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (xml));
}

bool DescantProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;
    applyState (*xml);
    userFile = file;
    factoryIndex = -1;
    currentName = file.getFileNameWithoutExtension();
    loadedValues = snapshot();
    return true;
}

bool DescantProcessor::saveUserPreset (const juce::String& rawName, juce::String& error)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
    {
        error = "Give the preset a name.";
        return false;
    }
    const auto folder = userPresetFolder();
    if (! folder.createDirectory())
    {
        error = "Couldn't create " + folder.getFullPathName();
        return false;
    }
    const auto file = folder.getChildFile (name + kPresetExtension);
    currentName = name;
    userFile = file;
    factoryIndex = -1;
    auto xml = stateXml();
    if (xml == nullptr || ! xml->writeTo (file))
    {
        error = "Couldn't write " + file.getFullPathName();
        return false;
    }
    loadedValues = snapshot();
    return true;
}

void DescantProcessor::stepPreset (int delta)
{
    const int numFactory = (int) factoryPresets().size();
    const auto users = userPresets();
    const int total = numFactory + users.size();
    int cur = presetNumber() - 1;
    if (cur < 0) cur = 0;
    const int next = (cur + delta + total) % total;
    if (next < numFactory)
        loadFactory (next);
    else
        loadUserPreset (users[next - numFactory]);
}

int DescantProcessor::presetNumber() const
{
    if (userFile == juce::File())
        return factoryIndex >= 0 ? factoryIndex + 1 : 0;
    const auto users = userPresets();
    const int i = users.indexOf (userFile);
    return i >= 0 ? (int) factoryPresets().size() + i + 1 : 0;
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
void DescantProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = stateXml())
        copyXmlToBinary (*xml, destData);
}

void DescantProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    applyState (*xml);
    currentName = xml->getStringAttribute ("presetName", currentName);
    factoryIndex = xml->getIntAttribute ("presetFactory", -1);
    userFile = juce::File (xml->getStringAttribute ("presetFile"));
    if (userFile != juce::File() && ! userFile.existsAsFile())
        userFile = juce::File();
    loadedValues = snapshot();
}

juce::AudioProcessorEditor* DescantProcessor::createEditor()
{
    return new DescantEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DescantProcessor();
}
