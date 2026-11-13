#include "PluginProcessor.h"
#include "PluginEditor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using NRange = juce::NormalisableRange<float>;

namespace
{
    NRange skewed (float lo, float hi, float centre)
    {
        NRange r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }

    const juce::StringArray divisionNames { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
    const juce::StringArray arpDirections { "Forward", "Backward", "Random", "Odd steps", "Even steps" };
    const juce::StringArray laneDirections { "Forward", "Reverse", "Ping-Pong", "Random" };
    const juce::StringArray laneRanges { "Unipolar", "Bipolar" };
    const juce::StringArray midiChannels { "1", "2", "3", "4", "5", "6", "7", "8",
                                           "9", "10", "11", "12", "13", "14", "15", "16" };

    /** How a parameter prints itself in the host and under its knob. */
    enum class Fmt { hertz, millis, ratio, percent, integer, gain };

    juce::String formatValue (float v, Fmt fmt)
    {
        switch (fmt)
        {
            case Fmt::hertz:
                return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                                    : juce::String (juce::roundToInt (v)) + " Hz";
            case Fmt::millis:
                return v < 10.0f ? juce::String (v, 1) + " ms"
                                 : juce::String (juce::roundToInt (v)) + " ms";
            case Fmt::percent:
                return juce::String (juce::roundToInt (v * 100.0f)) + " %";
            case Fmt::integer:
                return juce::String (juce::roundToInt (v));
            case Fmt::gain:
                return juce::String (v, 2);
            default:
                return juce::String (v, 2);
        }
    }
}

//==============================================================================
double MessyHatTricksProcessor::beatsPerStep (int divisionIndex)
{
    switch (divisionIndex)
    {
        case 0:  return 1.0;        // 1/4
        case 1:  return 0.5;        // 1/8
        case 2:  return 0.25;       // 1/16
        case 3:  return 0.125;      // 1/32
        case 4:  return 1.0 / 3.0;  // 1/8T
        case 5:  return 1.0 / 6.0;  // 1/16T
        default: return 0.25;
    }
}

//==============================================================================
APVTS::ParameterLayout MessyHatTricksProcessor::createLayout()
{
    APVTS::ParameterLayout layout;
    auto add = [&layout] (const char* id, const char* name, NRange range, float def,
                          Fmt fmt = Fmt::ratio)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([fmt] (float v, int) { return formatValue (v, fmt); })));
    };

    // ---- voice -------------------------------------------------------------
    add ("attack",     "Attack",     skewed (0.1f, 60.0f, 4.0f),        0.6f,   Fmt::millis);
    add ("decay",      "Decay",      skewed (8.0f, 1400.0f, 150.0f),    110.0f, Fmt::millis);
    add ("metalFreq",  "Metal Freq", skewed (40.0f, 4000.0f, 400.0f),   320.0f, Fmt::hertz);
    add ("metalLevel", "Metal Level",NRange (0.0f, 1.0f),               0.75f,  Fmt::percent);
    add ("clickFreq",  "Click Freq", skewed (100.0f, 9000.0f, 1200.0f), 1150.0f,Fmt::hertz);
    add ("clickLevel", "Click Level",NRange (0.0f, 1.0f),               0.6f,   Fmt::percent);
    add ("hissFreq",   "Hiss Freq",  skewed (500.0f, 16000.0f, 5000.0f),7400.0f,Fmt::hertz);
    add ("hissLevel",  "Hiss Level", NRange (0.0f, 1.0f),               0.55f,  Fmt::percent);
    add ("combFreq",   "Comb Freq",  skewed (120.0f, 9000.0f, 1500.0f), 2600.0f,Fmt::hertz);
    add ("combFb",     "Comb Feedback", NRange (0.0f, 0.97f),           0.42f,  Fmt::percent);
    add ("combMix",    "Comb Mix",   NRange (0.0f, 1.0f),               0.34f,  Fmt::percent);
    add ("fmFreq",     "FM Freq",    skewed (150.0f, 7000.0f, 1500.0f), 1850.0f,Fmt::hertz);
    add ("fmDecay",    "FM Decay",   skewed (1.0f, 150.0f, 20.0f),      13.0f,  Fmt::millis);
    add ("modFreq",    "Mod Freq",   skewed (0.5f, 3200.0f, 300.0f),    430.0f, Fmt::hertz);
    add ("modAmount",  "Mod Amount", NRange (0.0f, 1.0f),               0.34f,  Fmt::percent);
    add ("modMix",     "Mod Mix",    NRange (0.0f, 1.0f),               0.26f,  Fmt::percent);
    add ("filterFreq", "Cutoff",     skewed (120.0f, 14000.0f, 2000.0f),4230.0f,Fmt::hertz);
    add ("filterQ",    "Resonance",  skewed (0.3f, 9.0f, 1.5f),         0.83f,  Fmt::gain);
    add ("velSens",    "Vel Sens",   NRange (0.0f, 1.0f),               0.72f,  Fmt::percent);
    add ("width",      "Width",      NRange (0.0f, 1.0f),               0.55f,  Fmt::percent);
    add ("output",     "Output",     NRange (0.0f, 1.4f),               0.85f,  Fmt::gain);

    // ---- arpeggiator --------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "arpOn", 1 }, "Arp On", true));
    layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "arpDir", 1 }, "Direction", arpDirections, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "arpDiv", 1 }, "Division", divisionNames, 2));
    add ("arpOctaves", "Octaves", NRange (1.0f, 4.0f, 1.0f),  1.0f,  Fmt::integer);
    add ("arpSteps",   "Steps",   NRange (1.0f, 16.0f, 1.0f), 16.0f, Fmt::integer);
    add ("arpGate",    "Gate",    NRange (0.1f, 2.0f),        0.9f,  Fmt::gain);
    add ("arpSwing",   "Swing",   NRange (0.0f, 0.7f),        0.12f, Fmt::percent);

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "quality", 1 }, "Quality",
        juce::StringArray { "Draft (1x)", "High (2x)", "Ultra (4x)" }, 1));

    // ---- MIDI output ----------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "midiOut", 1 }, "MIDI Out", false));
    layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "midiChannel", 1 }, "MIDI Channel", midiChannels, 0));
    layout.add (std::make_unique<juce::AudioParameterInt>   (juce::ParameterID { "midiRoot", 1 }, "MIDI Root Note", 0, 127, 42));

    // ---- mod lanes -----------------------------------------------------------
    for (int l = 1; l <= 2; ++l)
    {
        const auto n = juce::String (l);
        layout.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "lane" + n + "On", 1 },    "Lane " + n + " On", true));
        layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "lane" + n + "Div", 1 },   "Lane " + n + " Division", divisionNames, l == 1 ? 2 : 1));
        layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "lane" + n + "Dir", 1 },   "Lane " + n + " Direction", laneDirections, l == 1 ? 0 : 2));
        layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "lane" + n + "Range", 1 }, "Lane " + n + " Range", laneRanges, l == 1 ? 0 : 1));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "lane" + n + "Amount", 1 }, "Lane " + n + " Amount",
            NRange (0.0f, 1.0f), l == 1 ? 0.45f : 0.35f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction ([] (float v, int) { return formatValue (v, Fmt::percent); })));
    }

    add ("velDepth",  "Vel Depth",  NRange (0.0f, 1.0f), 0.4f, Fmt::percent);
    add ("randDepth", "Rand Depth", NRange (0.0f, 1.0f), 0.3f, Fmt::percent);

    // ---- euclidean generator ---------------------------------------------------
    add ("euPulses", "Euclid Pulses", NRange (1.0f, 16.0f, 1.0f), 5.0f, Fmt::integer);
    add ("euSteps",  "Euclid Steps",  NRange (2.0f, 16.0f, 1.0f), 16.0f, Fmt::integer);
    add ("euRotate", "Euclid Rotate", NRange (0.0f, 15.0f, 1.0f), 0.0f, Fmt::integer);
    add ("euAccent", "Euclid Accent", NRange (0.0f, 1.0f),        0.6f, Fmt::percent);

    return layout;
}

//==============================================================================
MessyHatTricksProcessor::MessyHatTricksProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto* p : getParameters())
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            cached[withID->paramID] = apvts.getRawParameterValue (withID->paramID);
}

int MessyHatTricksProcessor::currentFactor() const noexcept
{
    switch ((int) paramValue ("quality"))
    {
        case 0:  return 1;
        case 2:  return 4;
        default: return 2;
    }
}

void MessyHatTricksProcessor::prepareToPlay (double newSampleRate, int samplesPerBlock)
{
    sampleRate = newSampleRate;

    // Voices are sized for the highest rate they may ever run at, so switching
    // Quality later only changes a number, never the allocation.
    for (auto& v : voices)
        v.prepare (sampleRate * maxFactor);

    using OS = juce::dsp::Oversampling<float>;
    os2 = std::make_unique<OS> (2, 1, OS::filterHalfBandPolyphaseIIR, true, true);
    os4 = std::make_unique<OS> (2, 2, OS::filterHalfBandPolyphaseIIR, true, true);
    os2->initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
    os4->initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
    os2->reset();
    os4->reset();

    baseScratch.setSize (2, juce::jmax (1, samplesPerBlock), false, true, true);

    activeFactor = 0;   // forces the render rate to be pushed to the voices

    lastStepCounter = std::numeric_limits<long>::min();
    ppqPosition = 0.0;
    for (auto& p : pending)
        p.active = false;

    for (auto& h : heldNotes)
        h.active = false;

    incomingMidi.ensureSize (2048);   // so the audio thread never allocates
}

bool MessyHatTricksProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

//==============================================================================
float MessyHatTricksProcessor::modulated (int dest, const float srcValues[4]) const
{
    const auto& id = ch::destParamIDs()[dest];
    auto* param = apvts.getParameter (id);
    if (param == nullptr)
        return 0.0f;

    float norm = param->getValue();

    const auto mask = pattern.routes[(size_t) dest].load();
    const float depths[4] = { paramValue ("lane1Amount"), paramValue ("lane2Amount"),
                              paramValue ("velDepth"),    paramValue ("randDepth") };

    for (int s = 0; s < 4; ++s)
        if ((mask & (1u << s)) != 0u)
            norm += depths[s] * srcValues[s];

    return param->getNormalisableRange().convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm));
}

ch::VoiceParams MessyHatTricksProcessor::resolveParams (float velocity, int semitones, const float srcValues[4])
{
    ch::VoiceParams vp;
    vp.attackMs     = modulated (ch::dAttack, srcValues);
    vp.decayMs      = modulated (ch::dDecay, srcValues) * paramValue ("arpGate");
    vp.metalFreq    = modulated (ch::dMetalFreq, srcValues);
    vp.metalLevel   = modulated (ch::dMetalLevel, srcValues);
    vp.clickFreq    = modulated (ch::dClickFreq, srcValues);
    vp.clickLevel   = modulated (ch::dClickLevel, srcValues);
    vp.hissFreq     = modulated (ch::dHissFreq, srcValues);
    vp.hissLevel    = modulated (ch::dHissLevel, srcValues);
    vp.combFreq     = modulated (ch::dCombFreq, srcValues);
    vp.combFeedback = modulated (ch::dCombFb, srcValues);
    vp.combMix      = modulated (ch::dCombMix, srcValues);
    vp.fmFreq       = modulated (ch::dFmFreq, srcValues);
    vp.fmDecayMs    = modulated (ch::dFmDecay, srcValues);
    vp.modFreq      = modulated (ch::dModFreq, srcValues);
    vp.modAmount    = modulated (ch::dModAmount, srcValues);
    vp.modMix       = modulated (ch::dModMix, srcValues);
    vp.filterFreq   = modulated (ch::dFilterFreq, srcValues);
    vp.filterQ      = modulated (ch::dFilterQ, srcValues);
    vp.width        = modulated (ch::dWidth, srcValues);
    vp.output       = modulated (ch::dOutput, srcValues);

    const float velSens = modulated (ch::dVelSens, srcValues);
    vp.amplitude = (1.0f - velSens) + velSens * juce::jlimit (0.0f, 1.0f, velocity);
    vp.tune = std::pow (2.0f, (float) semitones / 12.0f);
    return vp;
}

void MessyHatTricksProcessor::startVoice (const ch::VoiceParams& vp)
{
    int index = -1;
    for (int i = 0; i < maxVoices; ++i)
        if (! voices[(size_t) i].isPlaying()) { index = i; break; }

    if (index < 0)
    {
        index = nextVoice;
        nextVoice = (nextVoice + 1) % maxVoices;
    }

    voices[(size_t) index].trigger (vp);
}

void MessyHatTricksProcessor::triggerManualHit (float velocity, int semitones)
{
    const float src[4] = { laneValueAt (0, ppqPosition), laneValueAt (1, ppqPosition),
                           velocity, rng.nextFloat() * 2.0f - 1.0f };
    startVoice (resolveParams (velocity, semitones, src));
}

//==============================================================================
float MessyHatTricksProcessor::laneValueAt (int laneIndex, double ppq) const
{
    const auto n = juce::String (laneIndex + 1);
    if (paramValue (("lane" + n + "On").toRawUTF8()) < 0.5f)
        return 0.0f;

    const int len = ch::Pattern::numSteps;
    const double stepBeats = beatsPerStep ((int) paramValue (("lane" + n + "Div").toRawUTF8()));
    long counter = (long) std::floor (juce::jmax (0.0, ppq) / juce::jmax (1.0e-6, stepBeats));

    int index;
    switch ((int) paramValue (("lane" + n + "Dir").toRawUTF8()))
    {
        case 1:  index = (len - 1) - (int) (counter % len); break;                       // Reverse
        case 2:  { const long m = counter % (len * 2 - 2);                               // Ping-Pong
                   index = (int) (m < len ? m : (len * 2 - 2 - m)); break; }
        case 3:  index = juce::Random::getSystemRandom().nextInt (len); break;           // Random
        default: index = (int) (counter % len); break;                                   // Forward
    }

    index = juce::jlimit (0, len - 1, index);
    const float raw = pattern.lane[(size_t) laneIndex][(size_t) index].load();
    const bool bipolar = paramValue (("lane" + n + "Range").toRawUTF8()) > 0.5f;
    return bipolar ? raw * 2.0f - 1.0f : raw;
}

int MessyHatTricksProcessor::orderedStepIndex (long counter) const
{
    const int len = juce::jlimit (1, ch::Pattern::numSteps, (int) paramValue ("arpSteps"));
    const int dir = (int) paramValue ("arpDir");

    std::vector<int> order;
    order.reserve ((size_t) len);

    if (dir == 3)        for (int i = 0; i < len; i += 2) order.push_back (i);   // Odd steps (1, 3, 5...)
    else if (dir == 4)   for (int i = 1; i < len; i += 2) order.push_back (i);   // Even steps (2, 4, 6...)
    else                 for (int i = 0; i < len; ++i)    order.push_back (i);

    if (order.empty())
        for (int i = 0; i < len; ++i) order.push_back (i);

    if (dir == 1)
        std::reverse (order.begin(), order.end());

    if (dir == 2)
        return order[(size_t) juce::Random::getSystemRandom().nextInt ((int) order.size())];

    return order[(size_t) (counter % (long) order.size())];
}

void MessyHatTricksProcessor::emitNote (int sampleOffset, int semitones, float velocity)
{
    if (outgoingMidi == nullptr || paramValue ("midiOut") < 0.5f)
        return;

    const int channel = juce::jlimit (1, 16, (int) paramValue ("midiChannel") + 1);
    const int root = juce::jlimit (0, 127, (int) paramValue ("midiRoot"));
    const int note = juce::jlimit (0, 127, root + semitones);

    outgoingMidi->addEvent (juce::MidiMessage::noteOn (channel, note,
                                                       juce::jlimit (0.01f, 1.0f, velocity)),
                            sampleOffset);

    // Hold it for the step's gate, so the receiving device sees a real note length.
    const double stepBeats = beatsPerStep ((int) paramValue ("arpDiv"));
    const double gate = juce::jlimit (0.05f, 2.0f, paramValue ("arpGate"));
    const int length = juce::jmax (16, (int) (stepBeats * gate * 60.0 / hostBpm * renderRate));

    for (auto& h : heldNotes)
    {
        if (h.active)
            continue;

        h = { length, note, channel, true };
        return;
    }

    // No slot free: release the oldest right away so nothing hangs.
    auto& oldest = heldNotes[0];
    outgoingMidi->addEvent (juce::MidiMessage::noteOff (oldest.channel, oldest.note), sampleOffset);
    oldest = { length, note, channel, true };
}

void MessyHatTricksProcessor::flushNoteOffs (int sampleOffset)
{
    for (auto& h : heldNotes)
    {
        if (! h.active)
            continue;

        if (--h.samplesLeft > 0)
            continue;

        if (outgoingMidi != nullptr)
            outgoingMidi->addEvent (juce::MidiMessage::noteOff (h.channel, h.note), sampleOffset);

        h.active = false;
    }
}

void MessyHatTricksProcessor::triggerStep (long counter, int sampleOffset)
{
    if (paramValue ("arpOn") < 0.5f)
        return;

    const int index = orderedStepIndex (counter);
    uiStep = index;

    if (! pattern.stepOn[(size_t) index].load())
        return;

    if (rng.nextFloat() > pattern.stepProb[(size_t) index].load())
        return;

    const int len = juce::jlimit (1, ch::Pattern::numSteps, (int) paramValue ("arpSteps"));
    const int octaves = juce::jlimit (1, 4, (int) paramValue ("arpOctaves"));
    const int octaveAdd = octaves > 1 ? 12 * (int) ((counter / len) % octaves) : 0;

    const float velocity = pattern.stepVel[(size_t) index].load();
    const int semitones = pattern.stepPitch[(size_t) index].load() + octaveAdd;
    const float src[4] = { laneValueAt (0, ppqPosition), laneValueAt (1, ppqPosition),
                           velocity, rng.nextFloat() * 2.0f - 1.0f };

    startVoice (resolveParams (velocity, semitones, src));
    emitNote (sampleOffset, semitones, velocity);
}

//==============================================================================
void MessyHatTricksProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    bool hasHostTransport = false, transportPlaying = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hasHostTransport = true;
            transportPlaying = pos->getIsPlaying();

            if (auto bpm = pos->getBpm())
                hostBpm = *bpm;

            if (auto ppq = pos->getPpqPosition())
                if (transportPlaying)
                    ppqPosition = *ppq;
        }
    }

    // In a host the sequencer follows the transport; standalone free-runs.
    const bool sequencerRunning = transportPlaying || ! hasHostTransport;


    // ---- quality: pick the factor and tell the voices the rate they run at ----
    // A block larger than the one we prepared for would overrun the scratch, so
    // that rare case renders dry rather than reallocating here.
    int factor = currentFactor();
    const bool scratchFits = numSamples <= baseScratch.getNumSamples()
                             && baseScratch.getNumChannels() >= 2;
    if (! scratchFits || os2 == nullptr || os4 == nullptr)
        factor = 1;

    if (factor != activeFactor)
    {
        activeFactor = factor;
        renderRate = sampleRate * factor;
        for (auto& v : voices)
            v.setRate (renderRate);
    }

    const int osSamples = numSamples * factor;
    const double ppqPerSample = hostBpm / 60.0 / renderRate;
    const double stepBeats = beatsPerStep ((int) paramValue ("arpDiv"));
    const double stepSamples = stepBeats * 60.0 / hostBpm * renderRate;
    const float swing = paramValue ("arpSwing");

    // Incoming MIDI is taken aside so the same buffer can carry what we generate.
    incomingMidi.clear();
    incomingMidi.addEvents (midi, 0, numSamples, 0);
    midi.clear();
    outgoingMidi = &midi;

    // Stopping the transport must not leave a note hanging in the receiving device.
    if (! sequencerRunning)
    {
        for (auto& h : heldNotes)
        {
            if (! h.active)
                continue;

            midi.addEvent (juce::MidiMessage::noteOff (h.channel, h.note), 0);
            h.active = false;
        }
    }

    auto midiIterator = incomingMidi.begin();

    // ---- where the voices render ---------------------------------------------
    // Above 1x the voices run into the oversampled block and the decimation
    // filter on the way down removes anything that folded above Nyquist.
    auto* overSampler = factor == 4 ? os4.get() : (factor == 2 ? os2.get() : nullptr);

    // At 1x without a usable scratch the voices render straight into the output.
    const bool viaScratch = scratchFits;
    juce::dsp::AudioBlock<float> baseBlock = viaScratch
        ? juce::dsp::AudioBlock<float> (baseScratch).getSubBlock (0, (size_t) numSamples)
        : juce::dsp::AudioBlock<float> (buffer).getSubBlock (0, (size_t) numSamples);
    baseBlock.clear();

    juce::dsp::AudioBlock<float> renderBlock = baseBlock;
    if (overSampler != nullptr)
    {
        renderBlock = overSampler->processSamplesUp (baseBlock);
        renderBlock.clear();
    }

    float* left  = renderBlock.getChannelPointer (0);
    float* right = renderBlock.getNumChannels() > 1 ? renderBlock.getChannelPointer (1) : nullptr;

    for (int i = 0; i < osSamples; ++i)
    {
        // ---- MIDI notes trigger a hit directly -----------------------------
        while (midiIterator != incomingMidi.end()
                && (*midiIterator).samplePosition * factor <= i)
        {
            const auto message = (*midiIterator).getMessage();
            if (message.isNoteOn())
                triggerManualHit (message.getFloatVelocity(), message.getNoteNumber() - 60);
            ++midiIterator;
        }

        const int hostSample = i / factor;

        // ---- sequencer ------------------------------------------------------
        const long counter = (long) std::floor (juce::jmax (0.0, ppqPosition) / juce::jmax (1.0e-6, stepBeats));
        if (sequencerRunning && counter != lastStepCounter)
        {
            lastStepCounter = counter;
            const int delaySamples = (counter % 2 != 0)
                                       ? (int) (swing * 0.5 * stepSamples) : 0;

            if (delaySamples <= 0)
            {
                triggerStep (counter, hostSample);
            }
            else
            {
                for (auto& p : pending)
                    if (! p.active) { p = { delaySamples, counter, true }; break; }
            }
        }

        for (auto& p : pending)
        {
            if (! p.active)
                continue;

            if (--p.samplesLeft <= 0)
            {
                p.active = false;
                triggerStep (p.counter, hostSample);
            }
        }

        flushNoteOffs (hostSample);

        // ---- voices -----------------------------------------------------------
        float l = 0.0f, r = 0.0f;
        for (auto& v : voices)
            v.renderSample (l, r);

        left[i] = l;
        if (right != nullptr)
            right[i] = r;

        ppqPosition += ppqPerSample;
    }

    if (overSampler != nullptr)
        overSampler->processSamplesDown (baseBlock);

    if (viaScratch)
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.copyFrom (ch, 0, baseScratch, juce::jmin (ch, 1), 0, numSamples);
    else if (numChannels > 1 && right == nullptr)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    outgoingMidi = nullptr;
    uiLevel = buffer.getMagnitude (0, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* MessyHatTricksProcessor::createEditor()
{
    return new MessyHatTricksEditor (*this);
}

//==============================================================================
juce::File MessyHatTricksProcessor::presetFolder()
{
    auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                      .getChildFile ("Calabi Yau")
                      .getChildFile ("Messy Hat Tricks")
                      .getChildFile ("Presets");
    folder.createDirectory();
    return folder;
}

juce::Result MessyHatTricksProcessor::savePresetTo (const juce::File& file)
{
    auto state = apvts.copyState();
    auto patternTree = state.getOrCreateChildWithName ("pattern", nullptr);
    pattern.writeTo (patternTree);
    state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);

    auto xml = state.createXml();
    if (xml == nullptr)
        return juce::Result::fail ("Could not build the preset.");

    if (! file.getParentDirectory().createDirectory())
        return juce::Result::fail ("Could not create " + file.getParentDirectory().getFullPathName());

    if (! xml->writeTo (file, {}))
        return juce::Result::fail ("Could not write " + file.getFullPathName());

    currentPresetName = file.getFileNameWithoutExtension();
    return juce::Result::ok();
}

juce::Result MessyHatTricksProcessor::loadPresetFrom (const juce::File& file)
{
    if (! file.existsAsFile())
        return juce::Result::fail ("That preset is not there any more.");

    auto xml = juce::parseXML (file);
    if (xml == nullptr)
        return juce::Result::fail ("That file is not a Messy Hat Tricks preset.");

    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != apvts.state.getType())
        return juce::Result::fail ("That file is not a Messy Hat Tricks preset.");

    pattern.readFrom (state.getChildWithName ("pattern"));
    apvts.replaceState (state);
    currentPresetName = file.getFileNameWithoutExtension();
    return juce::Result::ok();
}

//==============================================================================
void MessyHatTricksProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto patternTree = state.getOrCreateChildWithName ("pattern", nullptr);
    pattern.writeTo (patternTree);
    state.setProperty ("presetName", currentPresetName, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MessyHatTricksProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid())
            return;

        pattern.readFrom (state.getChildWithName ("pattern"));
        currentPresetName = state.getProperty ("presetName", "Init").toString();
        apvts.replaceState (state);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MessyHatTricksProcessor();
}
