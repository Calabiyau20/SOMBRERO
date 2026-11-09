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
}

//==============================================================================
double CalabiHatsProcessor::beatsPerStep (int divisionIndex)
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
APVTS::ParameterLayout CalabiHatsProcessor::createLayout()
{
    APVTS::ParameterLayout layout;
    auto add = [&layout] (const char* id, const char* name, NRange range, float def, const char* unit = "")
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, def,
            juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    // ---- voice -------------------------------------------------------------
    add ("attack",     "Attack",     skewed (0.1f, 60.0f, 4.0f),      0.6f,  "ms");
    add ("decay",      "Decay",      skewed (8.0f, 1400.0f, 150.0f),  110.0f,"ms");
    add ("metalFreq",  "Metal Freq", skewed (40.0f, 4000.0f, 400.0f), 320.0f,"Hz");
    add ("metalLevel", "Metal Level",NRange (0.0f, 1.0f),              0.75f);
    add ("clickFreq",  "Click Freq", skewed (100.0f, 9000.0f, 1200.0f),1150.0f,"Hz");
    add ("clickLevel", "Click Level",NRange (0.0f, 1.0f),              0.6f);
    add ("hissFreq",   "Hiss Freq",  skewed (500.0f, 16000.0f, 5000.0f),7400.0f,"Hz");
    add ("hissLevel",  "Hiss Level", NRange (0.0f, 1.0f),              0.55f);
    add ("combFreq",   "Comb Freq",  skewed (120.0f, 9000.0f, 1500.0f),2600.0f,"Hz");
    add ("combFb",     "Comb Feedback", NRange (0.0f, 0.92f),          0.42f);
    add ("combMix",    "Comb Mix",   NRange (0.0f, 1.0f),              0.34f);
    add ("fmFreq",     "FM Freq",    skewed (150.0f, 7000.0f, 1500.0f),1850.0f,"Hz");
    add ("fmDecay",    "FM Decay",   skewed (1.0f, 150.0f, 20.0f),    13.0f, "ms");
    add ("modFreq",    "Mod Freq",   skewed (0.5f, 3200.0f, 300.0f),  430.0f,"Hz");
    add ("modAmount",  "Mod Amount", NRange (0.0f, 1.0f),              0.34f);
    add ("modMix",     "Mod Mix",    NRange (0.0f, 1.0f),              0.26f);
    add ("filterFreq", "Filter",     skewed (120.0f, 14000.0f, 2000.0f),4230.0f,"Hz");
    add ("filterQ",    "Filter Q",   skewed (0.3f, 9.0f, 1.5f),       0.83f);
    add ("velSens",    "Vel Sens",   NRange (0.0f, 1.0f),              0.72f);
    add ("width",      "Width",      NRange (0.0f, 1.0f),              0.55f);
    add ("output",     "Output",     NRange (0.0f, 1.4f),              0.85f);

    // ---- arpeggiator --------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "arpOn", 1 }, "Arp On", true));
    layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "arpDir", 1 }, "Direction", arpDirections, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "arpDiv", 1 }, "Division", divisionNames, 2));
    add ("arpOctaves", "Octaves", NRange (1.0f, 4.0f, 1.0f),  1.0f);
    add ("arpSteps",   "Steps",   NRange (1.0f, 16.0f, 1.0f), 16.0f);
    add ("arpGate",    "Gate",    NRange (0.1f, 2.0f),        0.9f);
    add ("arpSwing",   "Swing",   NRange (0.0f, 0.7f),        0.12f);

    // ---- mod lanes -----------------------------------------------------------
    for (int l = 1; l <= 2; ++l)
    {
        const auto n = juce::String (l);
        layout.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "lane" + n + "On", 1 },    "Lane " + n + " On", true));
        layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "lane" + n + "Div", 1 },   "Lane " + n + " Division", divisionNames, l == 1 ? 2 : 1));
        layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "lane" + n + "Dir", 1 },   "Lane " + n + " Direction", laneDirections, l == 1 ? 0 : 2));
        layout.add (std::make_unique<juce::AudioParameterChoice>(juce::ParameterID { "lane" + n + "Range", 1 }, "Lane " + n + " Range", laneRanges, l == 1 ? 0 : 1));
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "lane" + n + "Amount", 1 },"Lane " + n + " Amount",
                                                                 NRange (0.0f, 1.0f), l == 1 ? 0.45f : 0.35f));
    }

    add ("velDepth",  "Vel Depth",  NRange (0.0f, 1.0f), 0.4f);
    add ("randDepth", "Rand Depth", NRange (0.0f, 1.0f), 0.3f);

    // ---- euclidean generator ---------------------------------------------------
    add ("euPulses", "Euclid Pulses", NRange (1.0f, 16.0f, 1.0f), 5.0f);
    add ("euSteps",  "Euclid Steps",  NRange (2.0f, 16.0f, 1.0f), 16.0f);
    add ("euRotate", "Euclid Rotate", NRange (0.0f, 15.0f, 1.0f), 0.0f);
    add ("euAccent", "Euclid Accent", NRange (0.0f, 1.0f),        0.6f);

    return layout;
}

//==============================================================================
CalabiHatsProcessor::CalabiHatsProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto* p : getParameters())
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            cached[withID->paramID] = apvts.getRawParameterValue (withID->paramID);
}

void CalabiHatsProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    for (auto& v : voices)
        v.prepare (sampleRate);

    lastStepCounter = std::numeric_limits<long>::min();
    ppqPosition = 0.0;
    for (auto& p : pending)
        p.active = false;
}

bool CalabiHatsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

//==============================================================================
float CalabiHatsProcessor::modulated (int dest, const float srcValues[4]) const
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

ch::VoiceParams CalabiHatsProcessor::resolveParams (float velocity, int semitones, const float srcValues[4])
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

void CalabiHatsProcessor::startVoice (const ch::VoiceParams& vp)
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

void CalabiHatsProcessor::triggerManualHit (float velocity, int semitones)
{
    const float src[4] = { laneValueAt (0, ppqPosition), laneValueAt (1, ppqPosition),
                           velocity, rng.nextFloat() * 2.0f - 1.0f };
    startVoice (resolveParams (velocity, semitones, src));
}

//==============================================================================
float CalabiHatsProcessor::laneValueAt (int laneIndex, double ppq) const
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

int CalabiHatsProcessor::orderedStepIndex (long counter) const
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

void CalabiHatsProcessor::triggerStep (long counter)
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
    const float src[4] = { laneValueAt (0, ppqPosition), laneValueAt (1, ppqPosition),
                           velocity, rng.nextFloat() * 2.0f - 1.0f };

    startVoice (resolveParams (velocity, pattern.stepPitch[(size_t) index].load() + octaveAdd, src));
}

//==============================================================================
void CalabiHatsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    const double ppqPerSample = hostBpm / 60.0 / sampleRate;
    const double stepBeats = beatsPerStep ((int) paramValue ("arpDiv"));
    const double stepSamples = stepBeats * 60.0 / hostBpm * sampleRate;
    const float swing = paramValue ("arpSwing");

    auto midiIterator = midi.begin();

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // ---- MIDI notes trigger a hit directly -----------------------------
        while (midiIterator != midi.end() && (*midiIterator).samplePosition <= i)
        {
            const auto message = (*midiIterator).getMessage();
            if (message.isNoteOn())
                triggerManualHit (message.getFloatVelocity(), message.getNoteNumber() - 60);
            ++midiIterator;
        }

        // ---- sequencer ------------------------------------------------------
        const long counter = (long) std::floor (juce::jmax (0.0, ppqPosition) / juce::jmax (1.0e-6, stepBeats));
        if (sequencerRunning && counter != lastStepCounter)
        {
            lastStepCounter = counter;
            const int delaySamples = (counter % 2 != 0)
                                       ? (int) (swing * 0.5 * stepSamples) : 0;

            if (delaySamples <= 0)
            {
                triggerStep (counter);
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
                triggerStep (p.counter);
            }
        }

        // ---- voices -----------------------------------------------------------
        float l = 0.0f, r = 0.0f;
        for (auto& v : voices)
            v.renderSample (l, r);

        left[i] = l;
        if (right != nullptr)
            right[i] = r;

        ppqPosition += ppqPerSample;
    }

    uiLevel = buffer.getMagnitude (0, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* CalabiHatsProcessor::createEditor()
{
    return new CalabiHatsEditor (*this);
}

void CalabiHatsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto patternTree = state.getOrCreateChildWithName ("pattern", nullptr);
    pattern.writeTo (patternTree);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CalabiHatsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid())
            return;

        pattern.readFrom (state.getChildWithName ("pattern"));
        apvts.replaceState (state);
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CalabiHatsProcessor();
}
