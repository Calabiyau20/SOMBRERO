#pragma once

#include <JuceHeader.h>
#include "Engine.h"

//==============================================================================
class MessyHatTricksProcessor : public juce::AudioProcessor
{
public:
    MessyHatTricksProcessor();
    ~MessyHatTricksProcessor() override = default;

    //== AudioProcessor =======================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Messy Hat Tricks"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //== plugin specific ======================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    /** Oversampling factor the Quality setting currently asks for. */
    int currentFactor() const noexcept;

    //== presets ==============================================================
    static juce::File presetFolder();
    juce::Result savePresetTo (const juce::File& file);
    juce::Result loadPresetFrom (const juce::File& file);
    juce::String presetName() const { return currentPresetName; }

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "state", createLayout() };
    ch::Pattern pattern;

    /** Step the sequencer last played, for the editor's playhead. */
    std::atomic<int> uiStep { -1 };
    /** Peak of the last block, for the editor's meter. */
    std::atomic<float> uiLevel { 0.0f };

    /** Fires one hit straight away — used by the editor's Trigger button. */
    void triggerManualHit (float velocity, int semitones);

    static double beatsPerStep (int divisionIndex);

private:
    void triggerStep (long stepCounter, int sampleOffset);
    void startVoice (const ch::VoiceParams& vp);
    /** Writes the note the sequencer just played to the MIDI output, if it is on. */
    void emitNote (int sampleOffset, int semitones, float velocity);
    void flushNoteOffs (int sampleOffset);
    ch::VoiceParams resolveParams (float velocity, int semitones, const float srcValues[4]);
    float modulated (int dest, const float srcValues[4]) const;
    float laneValueAt (int laneIndex, double ppq) const;
    int   orderedStepIndex (long counter) const;

    float paramValue (const char* id) const noexcept
    {
        const auto it = cached.find (juce::String (id));
        return it != cached.end() ? it->second->load() : 0.0f;
    }

    // cached atomic pointers, filled in the constructor
    std::map<juce::String, std::atomic<float>*> cached;

    static constexpr int maxVoices = 10;
    static constexpr int maxFactor = 4;
    std::array<ch::HatVoice, maxVoices> voices;
    int nextVoice = 0;

    // One oversampler per factor, all built up front: switching Quality mid-flight
    // must never allocate on the audio thread.
    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
    juce::AudioBuffer<float> baseScratch;
    int activeFactor = 1;

    juce::String currentPresetName { "Init" };

    double sampleRate = 44100.0;
    double renderRate = 44100.0;
    double ppqPosition = 0.0;
    double hostBpm = 120.0;
    long   lastStepCounter = std::numeric_limits<long>::min();

    struct Pending { int samplesLeft = 0; long counter = 0; bool active = false; };
    std::array<Pending, 8> pending;

    // MIDI output: the buffer for the block being rendered, plus the note-offs owed.
    juce::MidiBuffer incomingMidi;
    juce::MidiBuffer* outgoingMidi = nullptr;
    struct HeldNote { int samplesLeft = 0; int note = 0; int channel = 1; bool active = false; };
    std::array<HeldNote, 16> heldNotes;

    juce::Random rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessyHatTricksProcessor)
};
