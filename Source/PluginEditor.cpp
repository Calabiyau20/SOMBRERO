#include "PluginEditor.h"
#include "MessyHatTricksData.h"

//==============================================================================
namespace cy
{
    static juce::Image loadOnce (const char* data, int size)
    {
        return juce::ImageCache::getFromMemory (data, size);
    }

    const juce::Image& logotype()
    {
        static const juce::Image i = loadOnce (MessyData::messylogotype_png, MessyData::messylogotype_pngSize);
        return i;
    }
    const juce::Image& character()
    {
        static const juce::Image i = loadOnce (MessyData::messycharacter_png, MessyData::messycharacter_pngSize);
        return i;
    }
    const juce::Image& stars()
    {
        static const juce::Image i = loadOnce (MessyData::threestars_png, MessyData::threestars_pngSize);
        return i;
    }
    const juce::Image& malvinas()
    {
        static const juce::Image i = loadOnce (MessyData::malvinas_png, MessyData::malvinas_pngSize);
        return i;
    }
    const juce::Image& drumHead()
    {
        static const juce::Image i = loadOnce (MessyData::calabi_hats_logo_png, MessyData::calabi_hats_logo_pngSize);
        return i;
    }
    const juce::Image& calabiYau()
    {
        static const juce::Image i = loadOnce (MessyData::calabiyaulogotype_png, MessyData::calabiyaulogotype_pngSize);
        return i;
    }
}

namespace
{
    constexpr int headerBand   = 92;
    constexpr int footerBand   = 34;
    constexpr int windowWidth  = 1180;
    constexpr int windowHeight = 1000;
    constexpr int gap          = 11;
    constexpr int filterWidth  = 556;

    juce::Label* makeCaption (juce::OwnedArray<juce::Label>& store, juce::Component& parent,
                              const juce::String& text, juce::Colour colour = cy::faint,
                              float size = 8.0f, juce::Justification just = juce::Justification::centredLeft)
    {
        auto* l = store.add (new juce::Label ({}, text));
        l->setColour (juce::Label::textColourId, colour);
        l->setFont (juce::Font (juce::FontOptions (size)).boldened());
        l->setJustificationType (just);
        parent.addAndMakeVisible (l);
        return l;
    }

    juce::String nameOf (juce::AudioProcessorValueTreeState& state, const juce::String& id,
                         const juce::String& fallback)
    {
        if (auto* p = state.getParameter (id))
            return p->getName (12);

        jassertfalse;
        return fallback;
    }
}

//==============================================================================
MessyHatTricksEditor::MessyHatTricksEditor (MessyHatTricksProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    buildVoiceModules();
    buildArp();
    buildLanes();

    // ---- header -------------------------------------------------------------
    saveButton.onClick = [this] { savePreset(); };
    loadButton.onClick = [this] { loadPreset(); };
    triggerButton.onClick = [this] { proc.triggerManualHit (1.0f, 0); };
    for (auto* b : { &saveButton, &loadButton, &triggerButton })
        addAndMakeVisible (b);

    // ---- sections -----------------------------------------------------------
    voiceSection  = std::make_unique<Section> ("E-Hat Voice", voiceContent, 250);
    filterSection = std::make_unique<Section> ("9 · High-pass filter", filterContent, 236);
    laneSection   = std::make_unique<Section> ("Mod lanes & sources", laneContent, 236);
    arpSection    = std::make_unique<Section> ("Arpeggiator", arpContent, 330);

    for (auto* s : { voiceSection.get(), filterSection.get(), laneSection.get(), arpSection.get() })
    {
        s->onFoldChanged = [this] { layoutSections(); };
        holder.addAndMakeVisible (s);
    }

    viewport.setViewedComponent (&holder, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    addChildComponent (info);

    setResizable (true, true);
    setResizeLimits (980, 560, 2000, 1700);
    setSize (windowWidth, windowHeight);
    startTimerHz (24);
}

MessyHatTricksEditor::~MessyHatTricksEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
juce::ComboBox* MessyHatTricksEditor::addCombo (juce::Component& parent, const juce::String& paramID,
                                                const juce::String& caption)
{
    auto* box = combos.add (new juce::ComboBox());
    auto* parameter = proc.apvts.getParameter (paramID);
    jassert (parameter != nullptr);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
        box->addItemList (choice->choices, 1);

    parent.addAndMakeVisible (box);

    if (parameter != nullptr)
        comboAttachments.add (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (
            proc.apvts, paramID, *box));
    else
        box->setEnabled (false);

    if (caption.isNotEmpty())
        makeCaption (captions, parent, caption.toUpperCase());

    return box;
}

juce::TextButton* MessyHatTricksEditor::addButton (juce::Component& parent, const juce::String& text)
{
    auto* b = buttons.add (new juce::TextButton (text));
    parent.addAndMakeVisible (b);
    return b;
}

//==============================================================================
void MessyHatTricksEditor::buildVoiceModules()
{
    struct Spec
    {
        const char* title;
        const char* blurb;
        juce::Colour colour;
        std::vector<int> dests;
    };

    // In the order the signal actually travels through the voice.
    const std::vector<Spec> specs
    {
        { "1 · METAL",    "The body — six 808 squares",   cy::lane1,  { ch::dMetalFreq, ch::dMetalLevel } },
        { "2 · CLICK",    "The attack — filtered noise",  cy::randCol,{ ch::dClickFreq, ch::dClickLevel } },
        { "3 · HISS",     "The air — high-passed noise",  cy::lane2,  { ch::dHissFreq,  ch::dHissLevel } },
        { "4 · FM HIT",   "The metallic transient",       cy::velCol, { ch::dFmFreq,    ch::dFmDecay } },
        { "5 · COMB",     "Tuned metallic resonance",     cy::azure,  { ch::dCombFreq,  ch::dCombFb, ch::dCombMix } },
        { "6 · RING MOD", "Inharmonic colour",            cy::orchid, { ch::dModFreq,   ch::dModAmount, ch::dModMix } },
        { "7 · ENVELOPE", "How the hit opens and dies",   cy::gold,   { ch::dAttack,    ch::dDecay } },
        { "8 · OUTPUT",   "Dynamics, stereo and level",   cy::steel,  { ch::dVelSens,   ch::dWidth, ch::dOutput } }
    };

    for (const auto& spec : specs)
    {
        auto* box = modules.add (new ModuleBox (spec.title, spec.blurb, spec.colour));
        voiceContent.addAndMakeVisible (box);

        std::vector<KnobBox*> inside;
        for (int dest : spec.dests)
        {
            auto* k = knobs.add (new KnobBox (proc, dest, ch::destParamIDs()[dest],
                                              ch::destNames()[dest], spec.colour));
            box->addAndMakeVisible (k);
            inside.push_back (k);
        }
        moduleKnobs.push_back (inside);
    }

    filterPad = std::make_unique<FilterPad> (proc);
    filterContent.addAndMakeVisible (filterPad.get());
}

void MessyHatTricksEditor::buildArp()
{
    arpOnButton.setClickingTogglesState (true);
    arpOnButton.setColour (juce::TextButton::buttonOnColourId, cy::electric);
    arpContent.addAndMakeVisible (arpOnButton);
    if (proc.apvts.getParameter ("arpOn") != nullptr)
        buttonAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (
            proc.apvts, "arpOn", arpOnButton));

    arpDirBox = addCombo (arpContent, "arpDir", "Direction");
    dirCaption = captions.getLast();
    arpDivBox = addCombo (arpContent, "arpDiv", "Division");
    divCaption = captions.getLast();

    randomNotesButton.onClick = [this] { generateRandomPattern(); };
    arpContent.addAndMakeVisible (randomNotesButton);

    midiOutButton.setClickingTogglesState (true);
    midiOutButton.setColour (juce::TextButton::buttonOnColourId, cy::neon);
    arpContent.addAndMakeVisible (midiOutButton);
    if (proc.apvts.getParameter ("midiOut") != nullptr)
        buttonAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (
            proc.apvts, "midiOut", midiOutButton));

    midiChannelBox = addCombo (arpContent, "midiChannel", "Ch");
    chCaption = captions.getLast();

    for (auto id : { "arpOctaves", "arpSteps", "arpGate", "arpSwing" })
        arpKnobs.push_back (knobs.add (new KnobBox (proc, id, nameOf (proc.apvts, id, id), cy::gold)));

    arpKnobs.push_back (knobs.add (new KnobBox (proc, "midiRoot", "MIDI note", cy::neon)));

    for (auto* k : arpKnobs)
        arpContent.addAndMakeVisible (k);

    arpContent.addAndMakeVisible (stepGrid);
    hintCaption = makeCaption (captions, arpContent,
        "Tap a cell in ON to switch a step on or off; "
        "drag vertically over Pitch, Vel and Prob to draw them.", cy::faint, 9.5f);

    euCaption = makeCaption (captions, arpContent, "EUCLIDEAN GENERATOR", cy::gold, 9.0f,
                             juce::Justification::centred);

    euclidRing = std::make_unique<EuclidRing> (proc);
    arpContent.addAndMakeVisible (euclidRing.get());

    for (auto id : { "euPulses", "euSteps", "euRotate", "euAccent" })
    {
        auto* k = knobs.add (new KnobBox (proc, id, nameOf (proc.apvts, id, id).replace ("Euclid ", ""), cy::gold));
        arpContent.addAndMakeVisible (k);
        euclidKnobs.push_back (k);
    }

    euTargetBox.addItemList ({ "Steps on / off", "Velocity", "Probability", "Accents (vel + prob)" }, 1);
    euTargetBox.setSelectedId (1, juce::dontSendNotification);
    arpContent.addAndMakeVisible (euTargetBox);

    generateButton.onClick = [this] { generateEuclid(); };
    generateButton.setColour (juce::TextButton::buttonColourId, cy::gold);
    generateButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff241603));
    arpContent.addAndMakeVisible (generateButton);
}

void MessyHatTricksEditor::buildLanes()
{
    static const juce::StringArray toolNames { "Random", "Ramp", "Euclid", "Reverse", "Mirror", "Half", "Clear" };

    for (int l = 0; l < 2; ++l)
    {
        const auto n = juce::String (l + 1);
        auto& ui = lanes[(size_t) l];

        ui.chip = chips.add (new SourceChip (l));
        laneContent.addAndMakeVisible (ui.chip);

        ui.onButton = addButton (laneContent, "On");
        ui.onButton->setClickingTogglesState (true);
        ui.onButton->setColour (juce::TextButton::buttonOnColourId, l == 0 ? cy::lane1 : cy::lane2);
        if (proc.apvts.getParameter ("lane" + n + "On") != nullptr)
            buttonAttachments.add (new juce::AudioProcessorValueTreeState::ButtonAttachment (
                proc.apvts, "lane" + n + "On", *ui.onButton));

        ui.div   = addCombo (laneContent, "lane" + n + "Div", {});
        ui.dir   = addCombo (laneContent, "lane" + n + "Dir", {});
        ui.range = addCombo (laneContent, "lane" + n + "Range", {});

        ui.amount = knobs.add (new KnobBox (proc, "lane" + n + "Amount", "Amount",
                                            l == 0 ? cy::lane1 : cy::lane2));
        laneContent.addAndMakeVisible (ui.amount);

        ui.bars = new LaneBars (proc, l);
        laneContent.addAndMakeVisible (ui.bars);
        barsOwner.add (ui.bars);

        for (int t = 0; t < toolNames.size(); ++t)
        {
            auto* b = addButton (laneContent, toolNames[t]);
            b->onClick = [this, l, t] { applyLaneTool (l, t); };
            ui.tools.push_back (b);
        }
    }

    dragCaption = makeCaption (captions, laneContent,
        "DRAG ONTO ANY KNOB  " + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")));

    for (int s = 0; s < 4; ++s)
        laneContent.addAndMakeVisible (chips.add (new SourceChip (s)));

    for (auto id : { "velDepth", "randDepth" })
    {
        auto* k = knobs.add (new KnobBox (proc, id,
                                          juce::String (id) == "velDepth" ? "Vel" : "Rand",
                                          juce::String (id) == "velDepth" ? cy::velCol : cy::randCol));
        laneContent.addAndMakeVisible (k);
        depthKnobs.push_back (k);
    }
}

//==============================================================================
void MessyHatTricksEditor::savePreset()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Save this preset", MessyHatTricksProcessor::presetFolder(), "*.messyhat");

    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File())
                return;

            if (! file.hasFileExtension ("messyhat"))
                file = file.withFileExtension ("messyhat");

            const auto result = proc.savePresetTo (file);
            if (result.failed())
                juce::NativeMessageBox::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon, "Could not save", result.getErrorMessage());

            repaint();
        });
}

void MessyHatTricksEditor::loadPreset()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Open a preset", MessyHatTricksProcessor::presetFolder(), "*.messyhat");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file == juce::File())
                return;

            const auto result = proc.loadPresetFrom (file);
            if (result.failed())
                juce::NativeMessageBox::showMessageBoxAsync (
                    juce::MessageBoxIconType::WarningIcon, "Could not open", result.getErrorMessage());

            repaint();
        });
}

//==============================================================================
void MessyHatTricksEditor::generateRandomPattern()
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

void MessyHatTricksEditor::generateEuclid()
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

void MessyHatTricksEditor::applyLaneTool (int lane, int tool)
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

    if (auto* bars = lanes[(size_t) lane].bars)
        bars->repaint();
}

//==============================================================================
void MessyHatTricksEditor::timerCallback()
{
    stepGrid.repaint();

    for (auto* k : knobs)
        k->repaint();

    // the preset name and the meter live in the header
    repaint (0, 0, getWidth(), headerBand);
}

//==============================================================================
void MessyHatTricksEditor::mouseDown (const juce::MouseEvent& e)
{
    if (faceBounds.contains (e.getPosition()))
    {
        info.setVisible (true);
        info.toFront (false);
    }
}

//==============================================================================
void MessyHatTricksEditor::paint (juce::Graphics& g)
{
    auto all = getLocalBounds();

    cy::brushed (g, all);

    // the drum head, very faint, watermarking the chassis
    if (const auto& mark = cy::drumHead(); mark.isValid())
    {
        const float size = (float) juce::jmin (getWidth(), getHeight()) * 0.55f;
        g.setOpacity (0.04f);
        g.drawImage (mark, juce::Rectangle<float> ((float) getWidth() - size * 0.72f,
                                                   (float) getHeight() - size * 0.80f, size, size),
                     juce::RectanglePlacement::centred);
        g.setOpacity (1.0f);
    }

    // ---- header band ---------------------------------------------------------
    auto band = all.withHeight (headerBand);
    cy::brushed (g, band);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawLine (0.0f, (float) headerBand - 0.5f, (float) getWidth(), (float) headerBand - 0.5f, 1.0f);

    cy::screw (g, { 13.0f, 13.0f });
    cy::screw (g, { (float) getWidth() - 13.0f, 13.0f });

    auto header = band.reduced (18, 9);

    if (const auto& face = cy::character(); face.isValid())
    {
        faceBounds = header.removeFromLeft (74);
        g.drawImage (face, faceBounds.toFloat(), juce::RectanglePlacement::centred);
        header.removeFromLeft (10);
    }

    if (const auto& lg = cy::logotype(); lg.isValid())
    {
        auto area = header.removeFromLeft (96).toFloat();
        g.drawImage (lg, area, juce::RectanglePlacement::centred);
        header.removeFromLeft (14);

        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawLine ((float) header.getX() - 7.0f, (float) header.getY() + 6.0f,
                    (float) header.getX() - 7.0f, (float) header.getBottom() - 6.0f, 1.0f);
        header.removeFromLeft (6);
    }

    // the preset display sits beside the Save and Load buttons
    auto lcd = header.removeFromLeft (390).withSizeKeepingCentre (390, 28);
    lcd.removeFromLeft (190);
    cy::sunken (g, lcd, 4.0f);
    g.setColour (cy::gold.withAlpha (0.16f));
    g.drawRoundedRectangle (lcd.toFloat().reduced (0.5f), 4.0f, 1.0f);
    g.setColour (cy::gold);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (proc.presetName(), lcd.reduced (10, 0), juce::Justification::centredLeft, false);

    // ---- meter and the label mark --------------------------------------------
    auto right = band.reduced (18, 0);

    if (const auto& cyl = cy::calabiYau(); cyl.isValid())
    {
        auto area = right.removeFromRight (124).toFloat().withSizeKeepingCentre (124.0f, 46.0f);
        g.drawImage (cyl, area, juce::RectanglePlacement::centred);
        right.removeFromRight (16);
    }

    auto meter = right.removeFromRight (120).withSizeKeepingCentre (120, 9);
    cy::sunken (g, meter, 2.0f);
    const float level = juce::jlimit (0.0f, 1.0f, proc.uiLevel.load() * 1.4f);
    if (level > 0.001f)
    {
        auto lit = meter.reduced (1).withWidth (juce::roundToInt ((float) (meter.getWidth() - 2) * level));
        g.setGradientFill ({ cy::lane2, (float) lit.getX(), 0.0f, cy::neon, (float) lit.getRight(), 0.0f, false });
        g.fillRect (lit);
    }

    // ---- footer: the three stars between two hairlines -----------------------
    auto footer = all.removeFromBottom (footerBand);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (footer);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawLine ((float) footer.getX(), (float) footer.getY() + 0.5f,
                (float) footer.getRight(), (float) footer.getY() + 0.5f, 1.0f);

    if (const auto& st = cy::stars(); st.isValid())
    {
        const float h = 19.0f;
        const float w = h * (float) st.getWidth() / (float) juce::jmax (1, st.getHeight());
        auto art = juce::Rectangle<float> (w, h).withCentre (footer.getCentre().toFloat());

        g.setOpacity (0.62f);
        g.drawImage (st, art, juce::RectanglePlacement::centred);
        g.setOpacity (1.0f);

        const float y = footer.getCentreY();
        for (int side = 0; side < 2; ++side)
        {
            const float x0 = side == 0 ? art.getX() - 20.0f : art.getRight() + 20.0f;
            const float x1 = side == 0 ? art.getX() - 200.0f : art.getRight() + 200.0f;
            g.setGradientFill ({ cy::gold.withAlpha (0.32f), x0, y,
                                 cy::gold.withAlpha (0.0f), x1, y, false });
            g.drawLine (x0, y, x1, y, 1.0f);
        }
    }
}

void MessyHatTricksEditor::resized()
{
    auto band = getLocalBounds().withHeight (headerBand).reduced (18, 9);
    band.removeFromLeft (74 + 10 + 96 + 14 + 6);
    auto buttonsRow = band.removeFromLeft (190).withSizeKeepingCentre (190, 28);
    saveButton.setBounds (buttonsRow.removeFromLeft (86));
    buttonsRow.removeFromLeft (8);
    loadButton.setBounds (buttonsRow.removeFromLeft (86));

    auto rightRow = getLocalBounds().withHeight (headerBand).reduced (18, 0);
    rightRow.removeFromRight (124 + 16 + 120 + 16);
    triggerButton.setBounds (rightRow.removeFromRight (92).withSizeKeepingCentre (92, 28));

    viewport.setBounds (getLocalBounds().withTrimmedTop (headerBand).withTrimmedBottom (footerBand));
    info.setBounds (getLocalBounds());
    layoutSections();
}

//==============================================================================
void MessyHatTricksEditor::layoutSections()
{
    const int contentWidth = juce::jmax (700, viewport.getMaximumVisibleWidth()) - 28;

    // the voice grid is four modules wide; its height follows the rows it needs
    const int columns = juce::jmax (2, juce::jmin (4, contentWidth / 240));
    const int rows = (modules.size() + columns - 1) / columns;
    const int moduleHeight = ModuleBox::headerHeight + KnobBox::height + 6;
    voiceSection->setContentHeight (rows * moduleHeight + (rows - 1) * gap);

    int total = 12;
    for (auto* s : { voiceSection.get(), filterSection.get(), laneSection.get(), arpSection.get() })
        total += s->preferredHeight() + gap;

    // the filter and the lanes share a row, so only the taller one counts
    total -= juce::jmin (filterSection->preferredHeight(), laneSection->preferredHeight()) + gap;

    holder.setSize (juce::jmax (700, viewport.getMaximumVisibleWidth()), total);

    auto area = holder.getLocalBounds().reduced (14, 0).withTrimmedTop (12);

    voiceSection->setBounds (area.removeFromTop (voiceSection->preferredHeight()));
    area.removeFromTop (gap);

    {
        const int rowHeight = juce::jmax (filterSection->preferredHeight(), laneSection->preferredHeight());
        auto row = area.removeFromTop (rowHeight);
        const int left = juce::jmin (filterWidth, row.getWidth() / 2);
        filterSection->setBounds (row.removeFromLeft (left));
        row.removeFromLeft (gap);
        laneSection->setBounds (row);
        area.removeFromTop (gap);
    }

    arpSection->setBounds (area.removeFromTop (arpSection->preferredHeight()));

    for (auto* s : { voiceSection.get(), filterSection.get(), laneSection.get(), arpSection.get() })
        s->resized();

    //== voice ==================================================================
    {
        auto bounds = voiceContent.getLocalBounds();
        const int cellW = (bounds.getWidth() - (columns - 1) * gap) / columns;

        for (int m = 0; m < modules.size(); ++m)
        {
            const int col = m % columns, row = m / columns;
            juce::Rectangle<int> cell (bounds.getX() + col * (cellW + gap),
                                       bounds.getY() + row * (moduleHeight + gap),
                                       cellW, moduleHeight);
            modules[m]->setBounds (cell);

            const auto& inside = moduleKnobs[(size_t) m];
            auto inner = modules[m]->contentArea();
            const int span = (int) inside.size() * KnobBox::width;
            inner = inner.withSizeKeepingCentre (span, inner.getHeight());
            for (auto* k : inside)
                k->setBounds (inner.removeFromLeft (KnobBox::width));
        }
    }

    //== filter =================================================================
    filterPad->setBounds (filterContent.getLocalBounds());

    //== lanes ==================================================================
    {
        auto bounds = laneContent.getLocalBounds();
        const int blockHeight = 26 + 5 + 44 + 5 + 20;

        for (int l = 0; l < 2; ++l)
        {
            auto& ui = lanes[(size_t) l];
            auto block = bounds.removeFromTop (blockHeight);
            bounds.removeFromTop (9);

            auto head = block.removeFromTop (26);
            ui.chip->setBounds (head.removeFromLeft (62).withSizeKeepingCentre (62, 22));
            head.removeFromLeft (6);
            ui.onButton->setBounds (head.removeFromLeft (44).withSizeKeepingCentre (44, 21));
            head.removeFromLeft (6);
            ui.div->setBounds (head.removeFromLeft (66).withSizeKeepingCentre (66, 21));
            head.removeFromLeft (6);
            ui.dir->setBounds (head.removeFromLeft (88).withSizeKeepingCentre (88, 21));
            head.removeFromLeft (6);
            ui.range->setBounds (head.removeFromLeft (84).withSizeKeepingCentre (84, 21));

            block.removeFromTop (5);
            auto body = block;
            auto knobSlot = body.removeFromRight (KnobBox::width);
            ui.amount->setBounds (knobSlot.withHeight (KnobBox::height).withY (head.getY()));
            body.removeFromRight (8);

            ui.bars->setBounds (body.removeFromTop (44));
            body.removeFromTop (5);

            auto tools = body.removeFromTop (20);
            const int toolWidth = juce::jmin (74, tools.getWidth() / (int) ui.tools.size());
            for (auto* b : ui.tools)
            {
                b->setBounds (tools.removeFromLeft (toolWidth).reduced (1, 0));
            }
        }

        // the sources row, under a divider
        auto srcs = bounds.removeFromTop (juce::jmax (26, bounds.getHeight()));
        auto row = srcs.withSizeKeepingCentre (srcs.getWidth(), 30);
        dragCaption->setBounds (row.removeFromLeft (150));
        row.removeFromLeft (4);
        for (int s = 0; s < 4; ++s)
        {
            chips[2 + s]->setBounds (row.removeFromLeft (62).withSizeKeepingCentre (62, 22));
            row.removeFromLeft (6);
        }
        auto depth = row.removeFromRight (KnobBox::width * 2);
        for (auto* k : depthKnobs)
            k->setBounds (depth.removeFromLeft (KnobBox::width).withHeight (KnobBox::height)
                               .withY (srcs.getY() - 4));
    }

    //== arpeggiator ============================================================
    {
        auto bounds = arpContent.getLocalBounds();
        auto right = bounds.removeFromRight (juce::jmin (286, bounds.getWidth() / 3));
        bounds.removeFromRight (12);

        auto top = bounds.removeFromTop (38);
        arpOnButton.setBounds (top.removeFromLeft (80).withSizeKeepingCentre (80, 26));
        top.removeFromLeft (8);

        auto placeCombo = [&] (juce::ComboBox* box, juce::Label* caption, int width)
        {
            auto slot = top.removeFromLeft (width);
            caption->setBounds (slot.removeFromTop (11));
            box->setBounds (slot.withSizeKeepingCentre (width, 24));
            top.removeFromLeft (8);
        };

        placeCombo (arpDirBox, dirCaption, 104);
        placeCombo (arpDivBox, divCaption, 96);

        randomNotesButton.setBounds (top.removeFromLeft (134).withSizeKeepingCentre (134, 26));
        top.removeFromLeft (8);
        midiOutButton.setBounds (top.removeFromLeft (88).withSizeKeepingCentre (88, 26));
        top.removeFromLeft (6);
        placeCombo (midiChannelBox, chCaption, 54);

        auto knobRow = bounds.removeFromTop (KnobBox::height);
        for (auto* k : arpKnobs)
            k->setBounds (knobRow.removeFromLeft (KnobBox::width));

        bounds.removeFromTop (4);
        stepGrid.setBounds (bounds.removeFromTop (147));
        hintCaption->setBounds (bounds.removeFromTop (16));

        // the generator column
        euCaption->setBounds (right.removeFromTop (14));
        right.removeFromTop (6);
        euclidRing->setBounds (right.removeFromTop (juce::jmin (196, right.getWidth())));
        right.removeFromTop (8);

        auto euKnobs = right.removeFromTop (KnobBox::height);
        const int euWidth = euKnobs.getWidth() / 4;
        for (auto* k : euclidKnobs)
            k->setBounds (euKnobs.removeFromLeft (euWidth));

        right.removeFromTop (7);
        euTargetBox.setBounds (right.removeFromTop (26));
        right.removeFromTop (7);
        generateButton.setBounds (right.removeFromTop (30));
    }
}
