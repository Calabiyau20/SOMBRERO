#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Look.h"
#include "Widgets.h"

//==============================================================================
class MessyHatTricksEditor  : public juce::AudioProcessorEditor,
                              public juce::DragAndDropContainer,
                              private juce::Timer
{
public:
    explicit MessyHatTricksEditor (MessyHatTricksProcessor&);
    ~MessyHatTricksEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void layoutSections();
    void buildVoiceModules();
    void buildArp();
    void buildLanes();
    void generateRandomPattern();
    void generateEuclid();
    void applyLaneTool (int lane, int tool);
    void savePreset();
    void loadPreset();

    juce::ComboBox* addCombo (juce::Component& parent, const juce::String& paramID,
                              const juce::String& caption);
    juce::TextButton* addButton (juce::Component& parent, const juce::String& text);

    MessyHatTricksProcessor& proc;
    MessyLookAndFeel lnf;

    //== chassis ==============================================================
    juce::Viewport viewport;
    juce::Component holder;
    juce::Component voiceContent, filterContent, laneContent, arpContent;
    std::unique_ptr<Section> voiceSection, filterSection, laneSection, arpSection;

    //== header ===============================================================
    juce::TextButton saveButton { "Save" }, loadButton { "Load" }, triggerButton { "Trigger" };
    juce::Rectangle<int> faceBounds;        // the character: click it for the info panel
    InfoPanel info;
    std::unique_ptr<juce::FileChooser> chooser;

    //== voice ================================================================
    juce::OwnedArray<KnobBox> knobs;
    juce::OwnedArray<ModuleBox> modules;
    std::vector<std::vector<KnobBox*>> moduleKnobs;
    std::unique_ptr<FilterPad> filterPad;

    //== arp ==================================================================
    juce::TextButton arpOnButton { "Arp on" }, midiOutButton { "MIDI out" };
    juce::TextButton randomNotesButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\x84")) + " Random notes" };
    juce::TextButton generateButton { "Generate" };
    juce::ComboBox euTargetBox;
    juce::ComboBox* arpDirBox = nullptr;
    juce::ComboBox* arpDivBox = nullptr;
    juce::ComboBox* midiChannelBox = nullptr;
    std::vector<KnobBox*> arpKnobs, euclidKnobs, depthKnobs;
    StepGrid stepGrid { proc };
    std::unique_ptr<EuclidRing> euclidRing;

    //== lanes ================================================================
    struct LaneUI
    {
        SourceChip* chip = nullptr;
        juce::TextButton* onButton = nullptr;
        juce::ComboBox* div = nullptr;
        juce::ComboBox* dir = nullptr;
        juce::ComboBox* range = nullptr;
        KnobBox* amount = nullptr;
        LaneBars* bars = nullptr;
        std::vector<juce::TextButton*> tools;
    };
    std::array<LaneUI, 2> lanes;
    juce::OwnedArray<SourceChip> chips;
    juce::OwnedArray<LaneBars> barsOwner;

    //== plumbing =============================================================
    juce::OwnedArray<juce::Label> captions;
    // named, so the layout never depends on the order they were built in
    juce::Label* dirCaption = nullptr;
    juce::Label* divCaption = nullptr;
    juce::Label* chCaption = nullptr;
    juce::Label* hintCaption = nullptr;
    juce::Label* euCaption = nullptr;
    juce::Label* dragCaption = nullptr;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MessyHatTricksEditor)
};
