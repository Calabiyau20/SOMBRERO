#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class CalabiHatsProcessor;

//==============================================================================
/** Calabi Yau cosmos palette. */
namespace cy
{
    const juce::Colour voidBg   { 0xff06040f };
    const juce::Colour panel    { 0xff120b26 };
    const juce::Colour panel2   { 0xff1a1035 };
    const juce::Colour slot     { 0xff0c0720 };
    const juce::Colour line     { 0xff3a2870 };
    const juce::Colour ink      { 0xffece7ff };
    const juce::Colour dim      { 0xff9b8ed2 };
    const juce::Colour faint    { 0xff6b5da8 };
    const juce::Colour gold     { 0xffe9b24c };
    const juce::Colour lane1    { 0xffa855f7 };
    const juce::Colour lane2    { 0xff22d3ee };
    const juce::Colour velCol   { 0xff4ade80 };
    const juce::Colour randCol  { 0xfff472b6 };
    const juce::Colour hot      { 0xffff4d7d };

    inline juce::Colour sourceColour (int index)
    {
        switch (index)
        {
            case 0:  return lane1;
            case 1:  return lane2;
            case 2:  return velCol;
            default: return randCol;
        }
    }

    inline const juce::StringArray& sourceNames()
    {
        static const juce::StringArray n { "LANE 1", "LANE 2", "VEL", "RAND" };
        return n;
    }

    /** Chamfered frame, the shape used across the Calabi Yau plugins. */
    inline juce::Path chamfer (juce::Rectangle<float> r, float cut)
    {
        juce::Path p;
        p.startNewSubPath (r.getX() + cut, r.getY());
        p.lineTo (r.getRight(), r.getY());
        p.lineTo (r.getRight(), r.getBottom() - cut);
        p.lineTo (r.getRight() - cut, r.getBottom());
        p.lineTo (r.getX(), r.getBottom());
        p.lineTo (r.getX(), r.getY() + cut);
        p.closeSubPath();
        return p;
    }
}

//==============================================================================
class CalabiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CalabiLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, cy::gold);
        setColour (juce::ComboBox::backgroundColourId, cy::slot);
        setColour (juce::ComboBox::outlineColourId, cy::line);
        setColour (juce::ComboBox::textColourId, cy::ink);
        setColour (juce::ComboBox::arrowColourId, cy::faint);
        setColour (juce::PopupMenu::backgroundColourId, cy::panel2);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, cy::line);
        setColour (juce::PopupMenu::textColourId, cy::ink);
        setColour (juce::TextButton::buttonColourId, cy::slot);
        setColour (juce::TextButton::buttonOnColourId, cy::lane1.withAlpha (0.35f));
        setColour (juce::TextButton::textColourOffId, cy::ink);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, cy::dim);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = startAngle + pos * (endAngle - startAngle);
        const auto track = radius - 2.5f;

        juce::Path back;
        back.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2a1d55));
        g.strokePath (back, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        juce::Path fill;
        fill.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle, angle, true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId));
        g.strokePath (fill, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        const auto knobR = track - 5.0f;
        g.setColour (juce::Colour (0xff0d0820));
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour (juce::Colour (0xff45327f));
        g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

        juce::Point<float> tip (centre.x + std::sin (angle) * knobR * 0.85f,
                                centre.y - std::cos (angle) * knobR * 0.85f);
        g.setColour (cy::ink);
        g.drawLine ({ centre, tip }, 2.0f);
    }
};

//==============================================================================
/** A knob that accepts modulation sources dropped onto it. */
class KnobBox  : public juce::Component,
                 public juce::DragAndDropTarget
{
public:
    KnobBox (CalabiHatsProcessor& p, int destination, const juce::String& paramID,
             const juce::String& displayName)
        : proc (p), dest (destination), name (displayName)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (slider);

        param = p.apvts.getParameter (paramID);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            p.apvts, paramID, slider);

        setName (displayName);
    }

    /** Overload for parameters that are not modulation destinations. */
    KnobBox (CalabiHatsProcessor& p, const juce::String& paramID, const juce::String& displayName)
        : KnobBox (p, -1, paramID, displayName) {}

    void resized() override
    {
        slider.setBounds (getLocalBounds().removeFromTop (getHeight() - 26).reduced (2));
    }

    void paint (juce::Graphics& g) override
    {
        if (highlight)
        {
            g.setColour (cy::gold.withAlpha (0.18f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
            g.setColour (cy::gold);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
        }

        auto area = getLocalBounds().removeFromBottom (26);
        g.setColour (cy::dim);
        g.setFont (juce::Font (juce::FontOptions (10.5f)));
        g.drawText (name, area.removeFromTop (12), juce::Justification::centred, false);

        g.setColour (cy::ink);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (param != nullptr ? param->getCurrentValueAsText() : juce::String(),
                    area.removeFromTop (12), juce::Justification::centred, false);

        paintRouteDots (g);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (dest < 0)
            return;

        const auto mask = proc.pattern.routes[(size_t) dest].load();
        if (mask == 0u)
            return;

        int last = -1;
        for (int s = 0; s < 4; ++s)
            if ((mask & (1u << s)) != 0u)
                last = s;

        if (last < 0)
            return;

        const float depth = depthFor (last);
        const float pos = param != nullptr ? param->getValue() : 0.0f;
        const float start = juce::MathConstants<float>::pi * 1.25f;
        const float end   = juce::MathConstants<float>::pi * 2.75f;
        const float a0 = start + pos * (end - start);
        const float a1 = start + juce::jlimit (0.0f, 1.0f, pos + depth) * (end - start);

        auto knobArea = getLocalBounds().removeFromTop (getHeight() - 26).reduced (2).toFloat();
        const auto centre = knobArea.getCentre();
        const auto r = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f;

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, r, r, 0.0f, a0, juce::jmax (a0 + 0.001f, a1), true);
        g.setColour (cy::sourceColour (last));
        g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (dest < 0)
            return;

        const auto dots = dotBounds();
        if (! dots.contains (e.getPosition()))
            return;

        int visible = 0;
        for (int s = 0; s < 4; ++s)
        {
            if (! proc.pattern.isRouted (dest, (ch::ModSource) s))
                continue;

            auto box = juce::Rectangle<int> (dots.getX() + visible * 9, dots.getY(), 8, 8);
            if (box.expanded (2).contains (e.getPosition()))
            {
                proc.pattern.setRoute (dest, (ch::ModSource) s, false);
                repaint();
                return;
            }
            ++visible;
        }
    }

    //== drag and drop ========================================================
    bool isInterestedInDragSource (const SourceDetails& details) override
    {
        return dest >= 0 && details.description.isInt();
    }

    void itemDragEnter (const SourceDetails&) override { highlight = true;  repaint(); }
    void itemDragExit  (const SourceDetails&) override { highlight = false; repaint(); }

    void itemDropped (const SourceDetails& details) override
    {
        const int source = (int) details.description;
        if (juce::isPositiveAndBelow (source, 4))
            proc.pattern.setRoute (dest, (ch::ModSource) source, true);

        highlight = false;
        repaint();
    }

private:
    float depthFor (int source) const
    {
        static const char* ids[] = { "lane1Amount", "lane2Amount", "velDepth", "randDepth" };
        if (auto* p = proc.apvts.getRawParameterValue (ids[source]))
            return p->load();
        return 0.0f;
    }

    juce::Rectangle<int> dotBounds() const
    {
        return { getWidth() / 2 - 18, getHeight() - 9, 36, 8 };
    }

    void paintRouteDots (juce::Graphics& g)
    {
        if (dest < 0)
            return;

        auto dots = dotBounds();
        int visible = 0;
        for (int s = 0; s < 4; ++s)
        {
            if (! proc.pattern.isRouted (dest, (ch::ModSource) s))
                continue;

            g.setColour (cy::sourceColour (s));
            g.fillEllipse ((float) (dots.getX() + visible * 9), (float) dots.getY(), 7.0f, 7.0f);
            ++visible;
        }
    }

    CalabiHatsProcessor& proc;
    int dest;
    juce::String name;
    juce::Slider slider;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool highlight = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobBox)
};

//==============================================================================
/** A draggable modulation source chip. */
class SourceChip  : public juce::Component
{
public:
    explicit SourceChip (int sourceIndex) : index (sourceIndex) {}

    void paint (juce::Graphics& g) override
    {
        const auto c = cy::sourceColour (index);
        g.setColour (c.withAlpha (0.12f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
        g.setColour (c);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f, 1.0f);
        g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());
        g.drawText (cy::sourceNames()[index], getLocalBounds(), juce::Justification::centred, false);
    }

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! container->isDragAndDropActive())
                container->startDragging (index, this);
    }

    void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }

private:
    int index;
};

//==============================================================================
/** The arpeggiator's On / Pitch / Vel / Prob rows. */
class StepGrid  : public juce::Component
{
public:
    explicit StepGrid (CalabiHatsProcessor& p) : proc (p) {}

    static constexpr int labelWidth = 36;

    void paint (juce::Graphics& g) override
    {
        const int len = juce::jlimit (1, 16, (int) proc.apvts.getRawParameterValue ("arpSteps")->load());
        const int playing = proc.uiStep.load();

        for (int row = 0; row < 4; ++row)
        {
            auto bounds = rowBounds (row);

            g.setColour (cy::faint);
            g.setFont (juce::Font (juce::FontOptions (8.5f)));
            g.drawText (rowName (row), { 0, bounds.getY(), labelWidth - 4, bounds.getHeight() },
                        juce::Justification::centredRight, false);

            for (int i = 0; i < 16; ++i)
            {
                auto cell = cellBounds (row, i).toFloat();
                const bool beyond = i >= len;
                const float alpha = beyond ? 0.3f : 1.0f;

                g.setColour (cy::slot.withMultipliedAlpha (alpha));
                g.fillRect (cell);
                g.setColour (cy::line.withAlpha (0.55f * alpha));
                g.drawRect (cell, 1.0f);

                switch (row)
                {
                    case 0:
                        if (proc.pattern.stepOn[(size_t) i].load())
                        {
                            g.setColour (cy::velCol.withMultipliedAlpha (alpha));
                            g.fillRect (cell.reduced (1.0f));
                        }
                        break;

                    case 1:
                    {
                        const int pitch = juce::jlimit (-12, 12, proc.pattern.stepPitch[(size_t) i].load());
                        const float mid = cell.getCentreY();
                        const float h = std::abs ((float) pitch) / 12.0f * (cell.getHeight() * 0.45f);
                        g.setColour (cy::lane1.withMultipliedAlpha (alpha));
                        if (pitch >= 0) g.fillRect (cell.getX(), mid - h, cell.getWidth(), h);
                        else            g.fillRect (cell.getX(), mid, cell.getWidth(), h);
                        g.setColour (cy::dim.withAlpha (0.35f));
                        g.fillRect (cell.getX(), mid, cell.getWidth(), 1.0f);
                        if (pitch != 0)
                        {
                            g.setColour (juce::Colours::white.withAlpha (alpha));
                            g.setFont (juce::Font (juce::FontOptions (8.0f)));
                            g.drawText (juce::String (pitch > 0 ? "+" : "") + juce::String (pitch),
                                        cell.toNearestInt().removeFromTop (10),
                                        juce::Justification::centred, false);
                        }
                        break;
                    }

                    case 2:
                    {
                        const float v = proc.pattern.stepVel[(size_t) i].load();
                        const float h = juce::jmax (2.0f, v * cell.getHeight());
                        g.setColour (cy::lane1.withMultipliedAlpha (alpha));
                        g.fillRect (cell.getX(), cell.getBottom() - h, cell.getWidth(), h);
                        break;
                    }

                    default:
                    {
                        const float v = proc.pattern.stepProb[(size_t) i].load();
                        const float h = juce::jmax (2.0f, v * cell.getHeight());
                        g.setColour (juce::Colour (0xff7dd3fc).withMultipliedAlpha (alpha));
                        g.fillRect (cell.getX(), cell.getBottom() - h, cell.getWidth(), h);
                        if (v < 0.995f)
                        {
                            g.setColour (juce::Colours::white.withAlpha (0.85f * alpha));
                            g.setFont (juce::Font (juce::FontOptions (8.0f)));
                            g.drawText (juce::String (juce::roundToInt (v * 100.0f)),
                                        cell.toNearestInt(), juce::Justification::centredTop, false);
                        }
                        break;
                    }
                }

                if (i == playing)
                {
                    g.setColour (cy::gold);
                    g.drawRect (cell, 1.0f);
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        painted.clear();
        apply (e, true);
    }

    void mouseDrag (const juce::MouseEvent& e) override { apply (e, false); }

private:
    static juce::String rowName (int row)
    {
        switch (row) { case 0: return "ON"; case 1: return "PITCH"; case 2: return "VEL"; default: return "PROB"; }
    }

    static int rowHeight (int row)
    {
        switch (row) { case 0: return 18; case 1: return 42; case 2: return 42; default: return 28; }
    }

    juce::Rectangle<int> rowBounds (int row) const
    {
        int y = 0;
        for (int r = 0; r < row; ++r)
            y += rowHeight (r) + 3;
        return { labelWidth, y, getWidth() - labelWidth, rowHeight (row) };
    }

    juce::Rectangle<int> cellBounds (int row, int index) const
    {
        auto r = rowBounds (row);
        const float w = (float) r.getWidth() / 16.0f;
        return juce::Rectangle<float> (r.getX() + w * (float) index, (float) r.getY(),
                                       w - 2.0f, (float) r.getHeight()).toNearestInt();
    }

    void apply (const juce::MouseEvent& e, bool isFirst)
    {
        const int row = rowAt (e.y);
        if (row < 0)
            return;

        auto r = rowBounds (row);
        const int index = juce::jlimit (0, 15, (int) ((float) (e.x - r.getX()) / (float) r.getWidth() * 16.0f));
        const float rel = juce::jlimit (0.0f, 1.0f, 1.0f - (float) (e.y - r.getY()) / (float) r.getHeight());

        switch (row)
        {
            case 0:
                if (! painted.contains (index))
                {
                    painted.add (index);
                    proc.pattern.stepOn[(size_t) index] = isFirst ? ! proc.pattern.stepOn[(size_t) index].load() : true;
                }
                break;
            case 1:  proc.pattern.stepPitch[(size_t) index] = juce::roundToInt ((rel * 2.0f - 1.0f) * 12.0f); break;
            case 2:  proc.pattern.stepVel[(size_t) index] = rel; break;
            default: proc.pattern.stepProb[(size_t) index] = rel; break;
        }

        repaint();
    }

    int rowAt (int y) const
    {
        for (int row = 0; row < 4; ++row)
            if (rowBounds (row).expanded (0, 1).getY() <= y && y <= rowBounds (row).getBottom() + 1)
                return row;
        return -1;
    }

    CalabiHatsProcessor& proc;
    juce::Array<int> painted;
};

//==============================================================================
/** The 16 draggable bars of one modulation lane. */
class LaneBars  : public juce::Component
{
public:
    LaneBars (CalabiHatsProcessor& p, int laneIndex) : proc (p), lane (laneIndex) {}

    void paint (juce::Graphics& g) override
    {
        const auto colour = lane == 0 ? cy::lane1 : cy::lane2;
        for (int i = 0; i < 16; ++i)
        {
            auto cell = cellBounds (i).toFloat();
            g.setColour (cy::slot);
            g.fillRect (cell);
            g.setColour (cy::line.withAlpha (0.55f));
            g.drawRect (cell, 1.0f);

            const float v = proc.pattern.lane[(size_t) lane][(size_t) i].load();
            const float h = juce::jmax (2.0f, v * cell.getHeight());
            g.setGradientFill ({ colour, cell.getX(), cell.getBottom() - h,
                                 juce::Colour (0xff2a1d55), cell.getX(), cell.getBottom(), false });
            g.fillRect (cell.getX(), cell.getBottom() - h, cell.getWidth(), h);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { apply (e); }
    void mouseDrag (const juce::MouseEvent& e) override { apply (e); }

private:
    juce::Rectangle<int> cellBounds (int index) const
    {
        const float w = (float) getWidth() / 16.0f;
        return juce::Rectangle<float> (w * (float) index, 0.0f, w - 2.0f, (float) getHeight()).toNearestInt();
    }

    void apply (const juce::MouseEvent& e)
    {
        const int index = juce::jlimit (0, 15, (int) ((float) e.x / (float) getWidth() * 16.0f));
        proc.pattern.lane[(size_t) lane][(size_t) index] =
            juce::jlimit (0.0f, 1.0f, 1.0f - (float) e.y / (float) getHeight());
        repaint();
    }

    CalabiHatsProcessor& proc;
    int lane;
};

//==============================================================================
/** A panel whose header folds its content away. */
class Section  : public juce::Component
{
public:
    Section (const juce::String& titleText, juce::Component& contentToHold, int contentHeight)
        : title (titleText), content (contentToHold), height (contentHeight)
    {
        addAndMakeVisible (content);
    }

    static constexpr int headerHeight = 26;

    int preferredHeight() const { return headerHeight + (folded ? 6 : height + 12); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setGradientFill ({ cy::panel2, bounds.getX(), bounds.getY(),
                             cy::panel, bounds.getX(), bounds.getBottom(), false });
        g.fillPath (cy::chamfer (bounds, 12.0f));
        g.setColour (cy::line);
        g.strokePath (cy::chamfer (bounds.reduced (0.5f), 12.0f), juce::PathStrokeType (1.0f));

        auto header = getLocalBounds().removeFromTop (headerHeight).reduced (12, 0);
        g.setColour (cy::gold);
        g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());
        g.drawText (title.toUpperCase(), header.removeFromLeft (header.getWidth() - 20),
                    juce::Justification::centredLeft, false);

        g.setColour (cy::faint);
        g.drawText (folded ? "+" : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")),
                    header, juce::Justification::centredRight, false);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (headerHeight);
        content.setVisible (! folded);
        if (! folded)
            content.setBounds (area.reduced (12, 0).withTrimmedBottom (10));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.y > headerHeight)
            return;

        folded = ! folded;
        if (onFoldChanged != nullptr)
            onFoldChanged();
        repaint();
    }

    std::function<void()> onFoldChanged;

private:
    juce::String title;
    juce::Component& content;
    int height;
    bool folded = false;
};

//==============================================================================
class CalabiHatsEditor  : public juce::AudioProcessorEditor,
                          public juce::DragAndDropContainer,
                          private juce::Timer
{
public:
    explicit CalabiHatsEditor (CalabiHatsProcessor&);
    ~CalabiHatsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutSections();
    void generateRandomPattern();
    void generateEuclid();
    void applyLaneTool (int lane, int tool);

    KnobBox* addKnob (juce::Component& parent, int dest);
    void addCombo (juce::Component& parent, const juce::String& paramID, const juce::String& label);

    CalabiHatsProcessor& proc;
    CalabiLookAndFeel lnf;
    juce::Image logo;

    juce::Viewport viewport;
    juce::Component holder;
    juce::Component voiceContent, arpContent, laneContent, sourceContent;
    std::unique_ptr<Section> voiceSection, arpSection, laneSection, sourceSection;

    juce::OwnedArray<KnobBox> knobs;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachments;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::OwnedArray<SourceChip> chips;
    juce::OwnedArray<LaneBars> laneBars;

    juce::TextButton arpOnButton { "ARP ON" }, triggerButton { "Trigger" };
    juce::TextButton randomNotesButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\x84")) + " Random notes" };
    juce::TextButton generateButton { "Generate" };
    juce::ComboBox euTargetBox;

    StepGrid stepGrid { proc };
    juce::Label euclidReadout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CalabiHatsEditor)
};
