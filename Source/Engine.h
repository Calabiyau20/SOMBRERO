#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace ch
{

//==============================================================================
/** Topology-preserving state variable filter (Zavalishin / Simper form). */
struct SVF
{
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;

    void setup (float freqHz, float q, double sampleRate) noexcept
    {
        const auto nyq = (float) sampleRate * 0.49f;
        freqHz = juce::jlimit (10.0f, nyq, freqHz);
        g  = std::tan (juce::MathConstants<float>::pi * freqHz / (float) sampleRate);
        k  = 1.0f / juce::jmax (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    struct Out { float lp, bp, hp; };

    inline Out process (float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, v0 - k * v1 - v2 };
    }
};

//==============================================================================
/** One resolved snapshot of the voice parameters, taken at trigger time so that
    modulation is applied per hit, exactly like the prototype. */
struct VoiceParams
{
    float attackMs   = 0.6f,  decayMs   = 110.0f;
    float metalFreq  = 320.0f, metalLevel = 0.75f;
    float clickFreq  = 1150.0f, clickLevel = 0.6f;
    float hissFreq   = 7400.0f, hissLevel  = 0.55f;
    float combFreq   = 2600.0f, combFeedback = 0.42f, combMix = 0.34f;
    float fmFreq     = 1850.0f, fmDecayMs = 13.0f;
    float modFreq    = 430.0f, modAmount = 0.34f, modMix = 0.26f;
    float filterFreq = 4230.0f, filterQ = 0.83f;
    float width      = 0.55f, output = 0.85f;
    float amplitude  = 1.0f;   // velocity after Vel Sens
    float tune       = 1.0f;   // pitch ratio from the arp step
};

//==============================================================================
/** A single hi-hat hit: metal / click / hiss layers, FM transient, comb, filter,
    ring modulation and a Haas stereo spread. Renders one sample at a time so the
    sequencer can trigger with sample accuracy. */
class HatVoice
{
public:
    static constexpr int numMetalPartials = 6;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        combBuffer.assign ((size_t) juce::roundToInt (sampleRate * 0.06) + 4, 0.0f);
        widthBuffer.assign ((size_t) juce::roundToInt (sampleRate * 0.02) + 4, 0.0f);
        reset();
    }

    void reset()
    {
        playing = false;
        env = 0.0f;
        std::fill (combBuffer.begin(), combBuffer.end(), 0.0f);
        std::fill (widthBuffer.begin(), widthBuffer.end(), 0.0f);
        combWrite = widthWrite = 0;
        metalFilter.reset();
        clickFilter.reset();
        hissFilter.reset();
        mainFilter.reset();
        dampFilter.reset();
    }

    bool isPlaying() const noexcept { return playing; }

    void trigger (const VoiceParams& params)
    {
        p = params;

        const auto sr = (float) sampleRate;
        attackInc = 1.0f / juce::jmax (1.0f, p.attackMs * 0.001f * sr);
        decayCoef = std::exp (-1.0f / juce::jmax (1.0f, p.decayMs * 0.001f * sr * 0.33f));
        fmCoef    = std::exp (-1.0f / juce::jmax (1.0f, p.fmDecayMs * 0.001f * sr * 0.33f));

        const float clickMs = juce::jmin (p.decayMs * 0.22f, 35.0f);
        clickCoef = std::exp (-1.0f / juce::jmax (1.0f, clickMs * 0.001f * sr * 0.33f));

        env = 0.0f;
        inAttack = true;
        fmEnv = 1.0f;
        clickEnv = 1.0f;

        const float metalBase = juce::jlimit (20.0f, 2000.0f, p.metalFreq * p.tune / 5.43f);
        static constexpr float ratios[numMetalPartials] = { 2.0f, 3.0f, 4.16f, 5.43f, 6.79f, 8.21f };
        for (int i = 0; i < numMetalPartials; ++i)
        {
            metalInc[i]   = juce::jlimit (20.0f, sr * 0.45f, metalBase * ratios[i]) / sr;
            metalPhase[i] = rng.nextFloat();
        }

        metalFilter.setup (p.metalFreq * p.tune, 1.1f, sampleRate);
        clickFilter.setup (p.clickFreq * p.tune, 1.6f, sampleRate);
        hissFilter .setup (p.hissFreq  * p.tune, 0.8f, sampleRate);
        mainFilter .setup (p.filterFreq, p.filterQ, sampleRate);
        dampFilter .setup (11000.0f, 0.707f, sampleRate);

        fmCarrierFreq = juce::jlimit (20.0f, sr * 0.45f, p.fmFreq * p.tune);
        fmPhase = 0.0f;
        fmModPhase = 0.0f;
        ringPhase = 0.0f;

        combDelaySamples = juce::jlimit (2.0f, (float) combBuffer.size() - 2.0f,
                                         (float) sampleRate / juce::jmax (40.0f, p.combFreq));
        widthDelaySamples = juce::jlimit (0.0f, (float) widthBuffer.size() - 2.0f,
                                          p.width * 0.0055f * (float) sampleRate);
        playing = true;
    }

    inline void renderSample (float& outL, float& outR) noexcept
    {
        if (! playing)
            return;

        const auto sr = (float) sampleRate;

        // ---- amplitude envelope -------------------------------------------
        if (inAttack)
        {
            env += attackInc;
            if (env >= 1.0f) { env = 1.0f; inAttack = false; }
        }
        else
        {
            env *= decayCoef;
            if (env < 1.0e-5f) { playing = false; env = 0.0f; return; }
        }

        // ---- metal: six squares through a bandpass -------------------------
        float metal = 0.0f;
        for (int i = 0; i < numMetalPartials; ++i)
        {
            metalPhase[i] += metalInc[i];
            if (metalPhase[i] >= 1.0f) metalPhase[i] -= 1.0f;
            metal += (metalPhase[i] < 0.5f ? 1.0f : -1.0f);
        }
        metal = metalFilter.process (metal * (1.0f / numMetalPartials)).bp * p.metalLevel * 1.6f;

        // ---- click ---------------------------------------------------------
        clickEnv *= clickCoef;
        const float noise = rng.nextFloat() * 2.0f - 1.0f;
        const float click = clickFilter.process (noise).bp * p.clickLevel * clickEnv * 1.8f;

        // ---- hiss ----------------------------------------------------------
        const float noise2 = rng.nextFloat() * 2.0f - 1.0f;
        const float hiss = hissFilter.process (noise2).hp * p.hissLevel * 1.1f;

        // ---- FM transient ---------------------------------------------------
        fmEnv *= fmCoef;
        fmModPhase += (fmCarrierFreq * 1.71f) / sr;
        if (fmModPhase >= 1.0f) fmModPhase -= 1.0f;
        const float fmMod = std::sin (fmModPhase * juce::MathConstants<float>::twoPi);
        float fmInc = (fmCarrierFreq + fmMod * fmCarrierFreq * 3.2f * fmEnv) / sr;
        fmInc = juce::jlimit (-0.49f, 0.49f, fmInc);
        fmPhase += fmInc;
        if (fmPhase >= 1.0f) fmPhase -= 1.0f;
        if (fmPhase < 0.0f)  fmPhase += 1.0f;
        const float fm = std::sin (fmPhase * juce::MathConstants<float>::twoPi) * fmEnv * 0.55f;

        float sig = (metal + click + hiss + fm) * 0.35f;

        // ---- comb ------------------------------------------------------------
        const float delayed = readDelay (combBuffer, combWrite, combDelaySamples);
        const float damped = dampFilter.process (delayed).lp;
        combBuffer[(size_t) combWrite] = sig + damped * p.combFeedback;
        if (++combWrite >= (int) combBuffer.size()) combWrite = 0;
        sig = sig * (1.0f - p.combMix * 0.65f) + delayed * p.combMix;

        // ---- main filter ------------------------------------------------------
        sig = mainFilter.process (sig).hp;

        // ---- ring modulation ---------------------------------------------------
        ringPhase += p.modFreq / sr;
        if (ringPhase >= 1.0f) ringPhase -= 1.0f;
        const float ring = sig * std::sin (ringPhase * juce::MathConstants<float>::twoPi) * p.modAmount;
        sig = sig * (1.0f - p.modMix) + ring * p.modMix;

        // ---- stereo width (Haas) -------------------------------------------------
        const float right = readDelay (widthBuffer, widthWrite, widthDelaySamples);
        widthBuffer[(size_t) widthWrite] = sig;
        if (++widthWrite >= (int) widthBuffer.size()) widthWrite = 0;

        const float gain = env * p.amplitude * p.output;
        outL += sig * gain;
        outR += (p.width > 0.001f ? right : sig) * gain;
    }

private:
    static inline float readDelay (const std::vector<float>& buf, int writePos, float delaySamples) noexcept
    {
        const int size = (int) buf.size();
        float readPos = (float) writePos - delaySamples;
        while (readPos < 0.0f) readPos += (float) size;
        const int i0 = (int) readPos;
        const int i1 = (i0 + 1) % size;
        const float f = readPos - (float) i0;
        return buf[(size_t) i0] * (1.0f - f) + buf[(size_t) i1] * f;
    }

    double sampleRate = 44100.0;
    VoiceParams p;

    bool playing = false, inAttack = false;
    float env = 0.0f, attackInc = 0.01f, decayCoef = 0.999f;
    float fmEnv = 0.0f, fmCoef = 0.999f, clickEnv = 0.0f, clickCoef = 0.99f;

    float metalPhase[numMetalPartials] {};
    float metalInc[numMetalPartials] {};
    float fmPhase = 0.0f, fmModPhase = 0.0f, fmCarrierFreq = 1800.0f, ringPhase = 0.0f;

    SVF metalFilter, clickFilter, hissFilter, mainFilter, dampFilter;

    std::vector<float> combBuffer, widthBuffer;
    int combWrite = 0, widthWrite = 0;
    float combDelaySamples = 32.0f, widthDelaySamples = 0.0f;

    juce::Random rng { juce::Random::getSystemRandom().nextInt64() };
};

//==============================================================================
enum class ModSource { lane1 = 0, lane2, velocity, random, numSources };

enum Dest
{
    dAttack = 0, dDecay,
    dMetalFreq, dMetalLevel, dClickFreq, dClickLevel, dHissFreq, dHissLevel,
    dCombFreq, dCombFb, dCombMix,
    dFmFreq, dFmDecay,
    dModFreq, dModAmount, dModMix,
    dFilterFreq, dFilterQ,
    dVelSens, dWidth, dOutput,
    numDests
};

/** Parameter id for every modulation destination, in Dest order. */
inline const juce::StringArray& destParamIDs()
{
    static const juce::StringArray ids {
        "attack", "decay",
        "metalFreq", "metalLevel", "clickFreq", "clickLevel", "hissFreq", "hissLevel",
        "combFreq", "combFb", "combMix",
        "fmFreq", "fmDecay",
        "modFreq", "modAmount", "modMix",
        "filterFreq", "filterQ",
        "velSens", "width", "output"
    };
    return ids;
}

inline const juce::StringArray& destNames()
{
    static const juce::StringArray names {
        "Attack", "Decay",
        "Metal Freq", "Metal Lvl", "Click Freq", "Click Lvl", "Hiss Freq", "Hiss Lvl",
        "Freq", "Feedback", "Mix",
        "Freq", "Decay",
        "Freq", "Amount", "Mix",
        "Filter", "Q",
        "Vel Sens.", "Width", "Output"
    };
    return names;
}

//==============================================================================
/** 16-step arp pattern plus two modulation lanes. Edited from the message
    thread, read from the audio thread, so every cell is atomic. */
struct Pattern
{
    static constexpr int numSteps = 16;
    static constexpr int numLanes = 2;

    std::array<std::atomic<bool>,  numSteps> stepOn;
    std::array<std::atomic<int>,   numSteps> stepPitch;
    std::array<std::atomic<float>, numSteps> stepVel;
    std::array<std::atomic<float>, numSteps> stepProb;
    std::array<std::array<std::atomic<float>, numSteps>, numLanes> lane;

    /** One bitmask of ModSource per destination. */
    std::array<std::atomic<juce::uint32>, numDests> routes;

    Pattern() { setDefaults(); }

    void setDefaults()
    {
        for (int i = 0; i < numSteps; ++i)
        {
            stepOn[(size_t) i]    = (i != 6 && i != 13);
            stepPitch[(size_t) i] = 0;
            stepVel[(size_t) i]   = (i % 4 == 0 ? 1.0f : (i % 2 == 0 ? 0.72f : 0.5f));
            stepProb[(size_t) i]  = (i % 2 == 1 ? 0.78f : 1.0f);
            lane[0][(size_t) i]   = 0.5f + 0.45f * std::sin ((float) i / 16.0f * juce::MathConstants<float>::twoPi);
            lane[1][(size_t) i]   = (float) (i % 4) / 3.0f * 0.8f + 0.1f;
        }

        for (auto& r : routes)
            r = 0u;
    }

    bool isRouted (int dest, ModSource src) const noexcept
    {
        return (routes[(size_t) dest].load() & (1u << (int) src)) != 0u;
    }

    void setRoute (int dest, ModSource src, bool shouldBeOn)
    {
        auto mask = routes[(size_t) dest].load();
        const auto bit = 1u << (int) src;
        mask = shouldBeOn ? (mask | bit) : (mask & ~bit);
        routes[(size_t) dest] = mask;
    }

    void toggleRoute (int dest, ModSource src) { setRoute (dest, src, ! isRouted (dest, src)); }

    //== state ================================================================
    void writeTo (juce::ValueTree& tree) const
    {
        juce::String on, pitch, vel, prob, l1, l2, rt;
        for (int i = 0; i < numSteps; ++i)
        {
            on    += (stepOn[(size_t) i].load() ? "1" : "0");
            pitch += juce::String (stepPitch[(size_t) i].load()) + " ";
            vel   += juce::String (stepVel[(size_t) i].load(), 4) + " ";
            prob  += juce::String (stepProb[(size_t) i].load(), 4) + " ";
            l1    += juce::String (lane[0][(size_t) i].load(), 4) + " ";
            l2    += juce::String (lane[1][(size_t) i].load(), 4) + " ";
        }
        for (int d = 0; d < numDests; ++d)
            rt += juce::String ((int) routes[(size_t) d].load()) + " ";

        tree.setProperty ("stepOn", on, nullptr);
        tree.setProperty ("stepPitch", pitch.trim(), nullptr);
        tree.setProperty ("stepVel", vel.trim(), nullptr);
        tree.setProperty ("stepProb", prob.trim(), nullptr);
        tree.setProperty ("lane1", l1.trim(), nullptr);
        tree.setProperty ("lane2", l2.trim(), nullptr);
        tree.setProperty ("routes", rt.trim(), nullptr);
    }

    void readFrom (const juce::ValueTree& tree)
    {
        if (! tree.isValid())
            return;

        const auto on = tree.getProperty ("stepOn").toString();
        for (int i = 0; i < juce::jmin (numSteps, on.length()); ++i)
            stepOn[(size_t) i] = (on[i] == '1');

        auto readFloats = [&tree] (const char* prop, auto& target, int laneIndex = -1)
        {
            juce::StringArray tokens;
            tokens.addTokens (tree.getProperty (prop).toString(), " ", "");
            tokens.removeEmptyStrings();
            for (int i = 0; i < juce::jmin (numSteps, tokens.size()); ++i)
            {
                juce::ignoreUnused (laneIndex);
                target[(size_t) i] = (float) tokens[i].getDoubleValue();
            }
        };

        juce::StringArray pitchTokens;
        pitchTokens.addTokens (tree.getProperty ("stepPitch").toString(), " ", "");
        pitchTokens.removeEmptyStrings();
        for (int i = 0; i < juce::jmin (numSteps, pitchTokens.size()); ++i)
            stepPitch[(size_t) i] = pitchTokens[i].getIntValue();

        readFloats ("stepVel", stepVel);
        readFloats ("stepProb", stepProb);
        readFloats ("lane1", lane[0]);
        readFloats ("lane2", lane[1]);

        juce::StringArray routeTokens;
        routeTokens.addTokens (tree.getProperty ("routes").toString(), " ", "");
        routeTokens.removeEmptyStrings();
        for (int d = 0; d < juce::jmin ((int) numDests, routeTokens.size()); ++d)
            routes[(size_t) d] = (juce::uint32) routeTokens[d].getIntValue();
    }
};

//==============================================================================
/** Euclidean pattern: hit on step i when ((i * pulses) % steps) < pulses. */
inline std::array<bool, Pattern::numSteps> euclid (int pulses, int steps, int rotation)
{
    std::array<bool, Pattern::numSteps> out {};
    steps  = juce::jlimit (2, Pattern::numSteps, steps);
    pulses = juce::jlimit (0, steps, pulses);

    for (int i = 0; i < steps; ++i)
    {
        const int j = ((i - rotation) % steps + steps) % steps;
        out[(size_t) i] = (((j * pulses) % steps) < pulses);
    }
    return out;
}

} // namespace ch
