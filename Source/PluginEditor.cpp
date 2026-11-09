#include "PluginEditor.h"
#include "CalabiHatsData.h"

namespace
{
    constexpr int knobWidth = 64, knobHeight = 68;

    /** Parameter display name, safe when the id is missing. */
    juce::String nameOf (juce::AudioProcessorValueTreeState& state, const juce::String& id,
                         const juce::String& fallback)
    {
        if (auto* p = state.getParameter (id))
            return p->getName (12);

        jassertfalse;
        return fallback;
    }

    juce::Label* makeCaption (juce::OwnedArray<juce::Label>& store, juce::Component& parent,
                              const juce::String& text, juce::Colour colour = cy::faint, float size = 9.0f)
    {
        auto* l = store.add (new juce::Label ({}, text));
        l->setColour (juce::Label::textColourId, colour);
        l->setFont (juce::Font (juce::FontOptions (size)));
        l->setJustificationType (juce::Justification::centredLeft);
        parent.addAndMakeVisible (l);
        return l;
    }
}

//==============================================================================
CalabiHatsEditor::CalabiHatsEditor (CalabiHatsProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    logo = juce::ImageCache::getFromMemory (CalabiHatsData::calabi_hats_logo_png,
                                            CalabiHatsData::calabi_hats_logo_pngSize);

    // ---- voice ---------------------------------------------------------------
    for (int d = 0; d < ch::numDests; ++d)
        addKnob (voiceContent, d);

    // ---- arpeggiator -----------------------------------------------------------
    arpOnButton.setClickingTogglesState (true);
    arpContent.addAndMakeVisible (arpOnButton);
    buttonAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (proc.apvts, "arpOn", arpOnButton));

    addCombo (arpContent, "arpDir", "Direction");
    addCombo (arpContent, "arpDiv", "Division");

    randomNotesButton.onClick = [this] { generateRandomPattern(); };
    arpContent.addAndMakeVisible (randomNotesButton);

    for (auto id : { "arpOctaves", "arpSteps", "arpGate", "arpSwing" })
    {
        auto* k = knobs.add (new KnobBox (proc, id, nameOf (proc.apvts, id, id)));
        arpContent.addAndMakeVisible (k);
    }

    arpContent.addAndMakeVisible (stepGrid);

    makeCaption (labels, arpContent, "EUCLIDEAN GENERATOR", cy::gold, 9.5f)
        ->setJustificationType (juce::Justification::centred);

    for (auto id : { "euPulses", "euSteps", "euRotate", "euAccent" })
    {
        auto* k = knobs.add (new KnobBox (proc, id, nameOf (proc.apvts, id, id).replace ("Euclid ", "")));
        arpContent.addAndMakeVisible (k);
    }

    euTargetBox.addItemList ({ "Steps on / off", "Velocity", "Probability", "Accents (vel + prob)" }, 1);
    euTargetBox.setSelectedId (1, juce::dontSendNotification);
    arpContent.addAndMakeVisible (euTargetBox);

    generateButton.onClick = [this] { generateEuclid(); };
    arpContent.addAndMakeVisible (generateButton);

    euclidReadout.setColour (juce::Label::textColourId, cy::gold);
    euclidReadout.setFont (juce::Font (juce::FontOptions (9.5f)));
    euclidReadout.setJustificationType (juce::Justification::centred);
    arpContent.addAndMakeVisible (euclidReadout);

    // ---- lanes -------------------------------------------------------------------
    static const juce::StringArray toolNames { "Random", "Ramp", "Euclid", "Reverse", "Mirror", "Half", "Clear" };

    for (int l = 0; l < 2; ++l)
    {
        const auto n = juce::String (l + 1);

        auto* chip = chips.add (new SourceChip (l));
        laneContent.addAndMakeVisible (chip);

        auto* onButton = buttons.add (new juce::TextButton ("ON"));
        onButton->setClickingTogglesState (true);
        laneContent.addAndMakeVisible (onButton);
        if (proc.apvts.getParameter ("lane" + n + "On") != nullptr)
            buttonAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (
                proc.apvts, "lane" + n + "On", *onButton));

        addCombo (laneContent, "lane" + n + "Div", "Division");
        addCombo (laneContent, "lane" + n + "Dir", "Direction");
        addCombo (laneContent, "lane" + n + "Range", "Range");

        auto* amount = knobs.add (new KnobBox (proc, "lane" + n + "Amount", "Amount"));
        laneContent.addAndMakeVisible (amount);

        laneBars.add (new LaneBars (proc, l));
        laneContent.addAndMakeVisible (laneBars.getLast());

        for (int t = 0; t < toolNames.size(); ++t)
        {
            auto* b = buttons.add (new juce::TextButton (toolNames[t]));
            b->onClick = [this, l, t] { applyLaneTool (l, t); };
            laneContent.addAndMakeVisible (b);
        }
    }

    // ---- mod sources -----------------------------------------------------------------
    for (int s = 0; s < 4; ++s)
    {
        auto* chip = chips.add (new SourceChip (s));
        sourceContent.addAndMakeVisible (chip);
    }

    for (auto id : { "velDepth", "randDepth" })
    {
        auto* k = knobs.add (new KnobBox (proc, id, juce::String (id) == "velDepth" ? "Vel depth" : "Rand depth"));
        sourceContent.addAndMakeVisible (k);
    }

    makeCaption (labels, sourceContent,
                 "Drag a chip onto any knob to route it. The outer ring shows the depth; "
                 "click the coloured dot under a knob to remove that routing.", cy::faint, 10.0f);

    triggerButton.onClick = [this] { proc.triggerManualHit (1.0f, 0); };
    addAndMakeVisible (triggerButton);

    // ---- sections --------------------------------------------------------------------
    voiceSection  = std::make_unique<Section> ("E-Hat Voice", voiceContent, 150);
    arpSection    = std::make_unique<Section> ("Arpeggiator", arpContent, 268);
    laneSection   = std::make_unique<Section> ("Mod Lanes", laneContent, 292);
    sourceSection = std::make_unique<Section> ("Mod Sources", sourceContent, 86);

    for (auto* s : { voiceSection.get(), arpSection.get(), laneSection.get(), sourceSection.get() })
    {
        s->onFoldChanged = [this] { layoutSections(); };
        holder.addAndMakeVisible (s);
    }

    viewport.setViewedComponent (&holder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    setResizable (true, true);
    setResizeLimits (820, 480, 1800, 1400);
    setSize (960, 760);
    startTimerHz (30);
}

CalabiHatsEditor::~CalabiHatsEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
KnobBox* CalabiHatsEditor::addKnob (juce::Component& parent, int dest)
{
    auto* k = knobs.add (new KnobBox (proc, dest, ch::destParamIDs()[dest], ch::destNames()[dest]));
    parent.addAndMakeVisible (k);
    return k;
}

void CalabiHatsEditor::addCombo (juce::Component& parent, const juce::String& paramID, const juce::String& label)
{
    auto* box = combos.add (new juce::ComboBox());
    auto* parameter = proc.apvts.getParameter (paramID);
    jassert (parameter != nullptr);   // a missing id must never take the editor down

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
        box->addItemList (choice->choices, 1);

    parent.addAndMakeVisible (box);

    if (parameter != nullptr)
        comboAttachments.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (proc.apvts, paramID, *box));
    else
        box->setEnabled (false);

    auto* caption = makeCaption (labels, parent, label.toUpperCase());
    caption->setComponentID ("caption:" + paramID);
    juce::ignoreUnused (caption);
}

//==============================================================================
void CalabiHatsEditor::generateRandomPattern()
{
    auto& r = juce::Random::getSystemRandom();

    for (int i = 0; i < ch::Pattern::numSteps; ++i)
    {
        proc.pattern.stepPitch[(size_t) i] = r.nextInt ({ -12, 13 });
        proc.pattern.stepOn[(size_t) i]    = r.nextFloat() < 0.75f;
        proc.pattern.stepVel[(size_t) i]   = r.nextFloat();
        proc.pattern.stepProb[(size_t) i]  = r.nextFloat();
    }

    stepGrid.repaint();
}

void CalabiHatsEditor::generateEuclid()
{
    const int pulses = (int) proc.apvts.getRawParameterValue ("euPulses")->load();
    const int steps  = (int) proc.apvts.getRawParameterValue ("euSteps")->load();
    const int rot    = (int) proc.apvts.getRawParameterValue ("euRotate")->load();
    const float accent = proc.apvts.getRawParameterValue ("euAccent")->load();
    const int target = euTargetBox.getSelectedId();

    const auto pattern = ch::euclid (pulses, steps, rot);

    if (auto* stepsParam = proc.apvts.getParameter ("arpSteps"))
        stepsParam->setValueNotifyingHost (stepsParam->getNormalisableRange().convertTo0to1 ((float) steps));

    for (int i = 0; i < ch::Pattern::numSteps; ++i)
    {
        const bool hit = pattern[(size_t) (i % juce::jmax (1, steps))];
        const float ghost = juce::jlimit (0.05f, 1.0f, 1.0f - accent);

        if (target == 1)                 proc.pattern.stepOn[(size_t) i]  = hit;
        if (target == 2 || target == 4)  proc.pattern.stepVel[(size_t) i] = hit ? 1.0f : ghost;
        if (target == 3 || target == 4)  proc.pattern.stepProb[(size_t) i] = hit ? 1.0f : ghost;
        if (target == 4)                 proc.pattern.stepOn[(size_t) i]  = true;
    }

    stepGrid.repaint();
}

void CalabiHatsEditor::applyLaneTool (int lane, int tool)
{
    auto& values = proc.pattern.lane[(size_t) lane];
    auto& r = juce::Random::getSystemRandom();
    std::array<float, ch::Pattern::numSteps> copy {};
    for (int i = 0; i < ch::Pattern::numSteps; ++i)
        copy[(size_t) i] = values[(size_t) i].load();

    switch (tool)
    {
        case 0: for (int i = 0; i < 16; ++i) values[(size_t) i] = r.nextFloat(); break;
        case 1: for (int i = 0; i < 16; ++i) values[(size_t) i] = (float) i / 15.0f; break;
        case 2:
        {
            const auto e = ch::euclid (5, 16, 0);
            for (int i = 0; i < 16; ++i) values[(size_t) i] = e[(size_t) i] ? 0.95f : 0.12f;
            break;
        }
        case 3: for (int i = 0; i < 16; ++i) values[(size_t) i] = copy[(size_t) (15 - i)]; break;
        case 4: for (int i = 8; i < 16; ++i) values[(size_t) i] = copy[(size_t) (15 - i)]; break;
        case 5: for (int i = 0; i < 16; ++i) values[(size_t) i] = copy[(size_t) (i / 2)]; break;
        default: for (int i = 0; i < 16; ++i) values[(size_t) i] = 0.0f; break;
    }

    laneBars[lane]->repaint();
}

//==============================================================================
void CalabiHatsEditor::timerCallback()
{
    stepGrid.repaint();

    const int pulses = juce::jmin ((int) proc.apvts.getRawParameterValue ("euPulses")->load(),
                                   (int) proc.apvts.getRawParameterValue ("euSteps")->load());
    euclidReadout.setText ("E(" + juce::String (pulses) + ", "
                            + juce::String ((int) proc.apvts.getRawParameterValue ("euSteps")->load())
                            + ")  ROT " + juce::String ((int) proc.apvts.getRawParameterValue ("euRotate")->load()),
                           juce::dontSendNotification);

    for (auto* k : knobs)
        k->repaint();
}

//==============================================================================
void CalabiHatsEditor::paint (juce::Graphics& g)
{
    g.fillAll (cy::voidBg);

    g.setGradientFill ({ juce::Colour (0xff241048).withAlpha (0.85f), (float) getWidth() * 0.5f, -120.0f,
                         cy::voidBg.withAlpha (0.0f), (float) getWidth() * 0.5f, 360.0f, true });
    g.fillRect (getLocalBounds());

    auto header = getLocalBounds().removeFromTop (54).reduced (16, 8);

    if (logo.isValid())
    {
        auto logoArea = header.removeFromLeft (40).toFloat();
        g.drawImage (logo, logoArea, juce::RectanglePlacement::centred);
        header.removeFromLeft (10);
    }

    g.setColour (cy::ink);
    g.setFont (juce::Font (juce::FontOptions (20.0f)).boldened());
    g.drawText ("CALABI HATS", header.removeFromTop (24), juce::Justification::centredLeft, false);

    g.setColour (cy::faint);
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("HI-HAT ENGINE  -  CALABI YAU", header, juce::Justification::centredLeft, false);

    // output meter
    auto meter = getLocalBounds().removeFromTop (54).removeFromRight (160).reduced (16, 24);
    g.setColour (cy::slot);
    g.fillRect (meter);
    g.setColour (cy::line);
    g.drawRect (meter, 1);
    const float level = juce::jlimit (0.0f, 1.0f, proc.uiLevel.load() * 1.4f);
    g.setColour (cy::lane2);
    g.fillRect (meter.reduced (1).withWidth (juce::roundToInt ((float) (meter.getWidth() - 2) * level)));
}

void CalabiHatsEditor::resized()
{
    triggerButton.setBounds (getWidth() - 260, 18, 80, 24);
    viewport.setBounds (getLocalBounds().withTrimmedTop (54));
    layoutSections();
}

void CalabiHatsEditor::layoutSections()
{
    int total = 12;
    for (auto* s : { voiceSection.get(), arpSection.get(), laneSection.get(), sourceSection.get() })
        if (s != nullptr)
            total += s->preferredHeight() + 12;

    holder.setSize (juce::jmax (700, viewport.getMaximumVisibleWidth()), total);

    auto area = holder.getLocalBounds().reduced (16, 0).withTrimmedTop (12);

    for (auto* s : { voiceSection.get(), arpSection.get(), laneSection.get(), sourceSection.get() })
    {
        if (s == nullptr)
            continue;

        s->setBounds (area.removeFromTop (s->preferredHeight()));
        area.removeFromTop (12);
        s->resized();
    }

    //== voice ===============================================================
    {
        auto bounds = voiceContent.getLocalBounds();
        const int perRow = juce::jmax (6, bounds.getWidth() / knobWidth);
        int index = 0;
        auto row = bounds.removeFromTop (knobHeight);

        for (int d = 0; d < ch::numDests; ++d)
        {
            if (index > 0 && index % perRow == 0)
            {
                bounds.removeFromTop (4);
                row = bounds.removeFromTop (knobHeight);
            }

            knobs[d]->setBounds (row.removeFromLeft (knobWidth));
            ++index;
        }
    }

    //== arp =================================================================
    {
        auto bounds = arpContent.getLocalBounds();
        auto right = bounds.removeFromRight (juce::jmin (220, bounds.getWidth() / 3));
        bounds.removeFromRight (12);

        auto top = bounds.removeFromTop (40);
        arpOnButton.setBounds (top.removeFromLeft (84).reduced (0, 8));
        top.removeFromLeft (8);

        for (int c = 0; c < 2; ++c)
        {
            auto slot = top.removeFromLeft (110);
            labels[c]->setBounds (slot.removeFromTop (12));
            combos[c]->setBounds (slot.reduced (0, 1));
            top.removeFromLeft (8);
        }

        randomNotesButton.setBounds (top.removeFromLeft (150).reduced (0, 8));

        auto knobRow = bounds.removeFromTop (knobHeight);
        for (int i = 0; i < 4; ++i)
            knobs[ch::numDests + i]->setBounds (knobRow.removeFromLeft (knobWidth));

        bounds.removeFromTop (6);
        stepGrid.setBounds (bounds.removeFromTop (150));

        // euclidean panel
        auto euCaption = right.removeFromTop (16);
        labels[2]->setBounds (euCaption);

        auto euKnobs = right.removeFromTop (knobHeight);
        for (int i = 0; i < 4; ++i)
            knobs[ch::numDests + 4 + i]->setBounds (euKnobs.removeFromLeft (juce::jmax (44, right.getWidth() / 4)));

        euclidReadout.setBounds (right.removeFromTop (16));
        right.removeFromTop (6);
        euTargetBox.setBounds (right.removeFromTop (24));
        right.removeFromTop (6);
        generateButton.setBounds (right.removeFromTop (28));
    }

    //== lanes ===============================================================
    {
        auto bounds = laneContent.getLocalBounds();
        const int laneHeight = bounds.getHeight() / 2;

        for (int l = 0; l < 2; ++l)
        {
            auto lane = bounds.removeFromTop (laneHeight);
            auto head = lane.removeFromTop (46);

            chips[l]->setBounds (head.removeFromLeft (64).reduced (0, 12));
            head.removeFromLeft (8);
            buttons[l * 8]->setBounds (head.removeFromLeft (46).reduced (0, 12));
            head.removeFromLeft (8);

            for (int c = 0; c < 3; ++c)
            {
                auto slot = head.removeFromLeft (96);
                labels[3 + l * 3 + c]->setBounds (slot.removeFromTop (12));
                combos[2 + l * 3 + c]->setBounds (slot.reduced (0, 2));
                head.removeFromLeft (6);
            }

            knobs[ch::numDests + 8 + l]->setBounds (head.removeFromLeft (knobWidth).withHeight (knobHeight));

            laneBars[l]->setBounds (lane.removeFromTop (56));
            lane.removeFromTop (4);

            auto tools = lane.removeFromTop (24);
            const int toolWidth = juce::jmin (74, tools.getWidth() / 7);
            for (int t = 0; t < 7; ++t)
            {
                buttons[l * 8 + 1 + t]->setBounds (tools.removeFromLeft (toolWidth).reduced (1, 0));
            }
        }
    }

    //== sources =============================================================
    {
        auto bounds = sourceContent.getLocalBounds();
        auto row = bounds.removeFromTop (knobHeight);

        for (int s = 0; s < 4; ++s)
        {
            chips[2 + s]->setBounds (row.removeFromLeft (70).reduced (0, 22));
            row.removeFromLeft (6);
        }

        row.removeFromLeft (10);
        knobs[ch::numDests + 10]->setBounds (row.removeFromLeft (knobWidth));
        knobs[ch::numDests + 11]->setBounds (row.removeFromLeft (knobWidth));

        labels[labels.size() - 1]->setBounds (bounds.removeFromTop (18));
    }
}
