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

    /** The Calabi Hats isologo — the drum robot in its bowler hat.
        Loaded once from the embedded PNG; defined in PluginEditor.cpp. */
    const juce::Image& brandMark();

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
        jassert (param != nullptr);   // a missing id must never take the editor down

        if (param != nullptr)
        {
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                p.apvts, paramID, slider);
        }
        else
        {
            slider.setEnabled (false);
        }

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
/** One stage of the voice: its own tint, its name, and a line saying what it
    does in the synthesis. Knobs (or the filter pad) are placed inside it. */
class ModuleBox  : public juce::Component
{
public:
    ModuleBox (const juce::String& titleText, const juce::String& blurbText, juce::Colour tint)
        : title (titleText), blurb (blurbText), colour (tint) {}

    static constexpr int headerHeight = 38;

    juce::Rectangle<int> contentArea() const
    {
        return getLocalBounds().withTrimmedTop (headerHeight).reduced (10, 0).withTrimmedBottom (8);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        g.setColour (colour.withAlpha (0.10f));
        g.fillPath (cy::chamfer (r, 10.0f));

        g.setColour (colour.withAlpha (0.55f));
        g.strokePath (cy::chamfer (r.reduced (0.5f), 10.0f), juce::PathStrokeType (1.2f));

        auto header = getLocalBounds().withHeight (headerHeight).reduced (12, 5);

        g.setColour (colour);
        g.setFont (juce::Font (juce::FontOptions (11.5f)).boldened());
        g.drawText (title, header.removeFromTop (14), juce::Justification::centredLeft, false);

        g.setColour (cy::dim);
        g.setFont (juce::Font (juce::FontOptions (9.5f)));
        g.drawText (blurb, header, juce::Justification::centredLeft, false);
    }

private:
    juce::String title, blurb;
    juce::Colour colour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleBox)
};

//==============================================================================
/** The high-pass filter as an XY pad: cutoff across, resonance up.
    Drag the handle, or use the arrow keys. Shift gives a fine drag. */
class FilterPad  : public juce::Component,
                   public juce::DragAndDropTarget,
                   private juce::Timer
{
public:
    explicit FilterPad (CalabiHatsProcessor& p) : proc (p)
    {
        cutoff = proc.apvts.getParameter ("filterFreq");
        reso   = proc.apvts.getParameter ("filterQ");
        setWantsKeyboardFocus (true);
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        startTimerHz (24);
    }

    static constexpr float fMin = 120.0f, fMax = 14000.0f;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        g.setColour (cy::slot);
        g.fillRect (r);

        // frequency grid
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        for (float f : { 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f })
        {
            const float x = r.getX() + freqToX (f) * r.getWidth();
            g.setColour (cy::line.withAlpha (0.55f));
            g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
            g.setColour (cy::faint);
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x + 3.0f, r.getBottom() - 14.0f, 34.0f, 12.0f)
                            .toNearestInt(),
                        juce::Justification::left, false);
        }
        for (int i = 1; i < 4; ++i)
        {
            const float y = r.getY() + r.getHeight() * (float) i / 4.0f;
            g.setColour (cy::line.withAlpha (0.4f));
            g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
        }

        // the high-pass response, drawn from the same values the DSP uses
        const float fc = cutoffHz(), q = resonance();
        juce::Path curve;
        for (float px = 0.0f; px <= r.getWidth(); px += 2.0f)
        {
            const float f = xToFreq (px / r.getWidth());
            const float ratio = f / fc;
            const float mag = ratio * ratio
                / std::sqrt (std::pow (1.0f - ratio * ratio, 2.0f) + std::pow (ratio / q, 2.0f));
            const float db = 20.0f * std::log10 (juce::jmax (1.0e-4f, mag));
            const float y = juce::jlimit (r.getY() - 20.0f, r.getBottom() + 20.0f,
                                          r.getCentreY() - db * (r.getHeight() / 48.0f));
            if (px == 0.0f) curve.startNewSubPath (r.getX() + px, y);
            else            curve.lineTo (r.getX() + px, y);
        }

        auto fill = curve;
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        g.setColour (cy::gold.withAlpha (0.12f));
        g.fillPath (fill);

        g.setColour (cy::gold);
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        // the handle
        const auto h = handlePos();
        g.setColour (cy::neon.withAlpha (0.25f));
        g.fillEllipse (h.x - 16.0f, h.y - 16.0f, 32.0f, 32.0f);
        g.setColour (cy::neon);
        g.fillEllipse (h.x - 7.0f, h.y - 7.0f, 14.0f, 14.0f);
        g.setColour (cy::voidBg);
        g.fillEllipse (h.x - 3.0f, h.y - 3.0f, 6.0f, 6.0f);

        g.setColour (cy::neon.withAlpha (0.45f));
        g.drawLine (h.x, r.getY(), h.x, r.getBottom(), 1.0f);
        g.drawLine (r.getX(), h.y, r.getRight(), h.y, 1.0f);

        // readout
        g.setColour (cy::ink);
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        g.drawText (cutoff->getCurrentValueAsText() + "   Q " + reso->getCurrentValueAsText(),
                    getLocalBounds().reduced (8, 6), juce::Justification::topRight, false);

        g.setColour (cy::line);
        g.drawRect (getLocalBounds(), 1);

        if (hasKeyboardFocus (false))
        {
            g.setColour (cy::gold);
            g.drawRect (getLocalBounds(), 2);
        }

        if (highlight)
        {
            g.setColour (cy::gold.withAlpha (0.2f));
            g.fillRect (getLocalBounds());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { grabKeyboardFocus(); dragTo (e); }
    void mouseDrag (const juce::MouseEvent& e) override { dragTo (e); }

    bool keyPressed (const juce::KeyPress& k) override
    {
        const float stepX = k.getModifiers().isShiftDown() ? 0.002f : 0.02f;
        if (k.isKeyCode (juce::KeyPress::leftKey))  { nudge (-stepX, 0.0f); return true; }
        if (k.isKeyCode (juce::KeyPress::rightKey)) { nudge ( stepX, 0.0f); return true; }
        if (k.isKeyCode (juce::KeyPress::upKey))    { nudge (0.0f,  stepX); return true; }
        if (k.isKeyCode (juce::KeyPress::downKey))  { nudge (0.0f, -stepX); return true; }
        return false;
    }

    //== drag and drop: the pad is a modulation destination too =================
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.isInt(); }
    void itemDragEnter (const SourceDetails&) override { highlight = true;  repaint(); }
    void itemDragExit  (const SourceDetails&) override { highlight = false; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        const int source = (int) d.description;
        if (juce::isPositiveAndBelow (source, 4))
            proc.pattern.setRoute (ch::dFilterFreq, (ch::ModSource) source, true);

        highlight = false;
        repaint();
    }

private:
    void timerCallback() override
    {
        const auto now = cutoff->getValue() * 1000.0f + reso->getValue();
        if (std::abs (now - lastDrawn) > 0.001f) { lastDrawn = now; repaint(); }
    }

    static float freqToX (float f)
    {
        return std::log (f / fMin) / std::log (fMax / fMin);
    }
    static float xToFreq (float x)
    {
        return fMin * std::pow (fMax / fMin, juce::jlimit (0.0f, 1.0f, x));
    }

    float cutoffHz()   const { return cutoff->getNormalisableRange().convertFrom0to1 (cutoff->getValue()); }
    float resonance()  const { return reso->getNormalisableRange().convertFrom0to1 (reso->getValue()); }

    juce::Point<float> handlePos() const
    {
        auto r = getLocalBounds().toFloat();
        return { r.getX() + freqToX (juce::jlimit (fMin, fMax, cutoffHz())) * r.getWidth(),
                 r.getBottom() - reso->getValue() * r.getHeight() };
    }

    void dragTo (const juce::MouseEvent& e)
    {
        auto r = getLocalBounds().toFloat();
        const float nx = juce::jlimit (0.0f, 1.0f, (float) e.x / juce::jmax (1.0f, r.getWidth()));
        const float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (float) e.y / juce::jmax (1.0f, r.getHeight()));

        const float hz = xToFreq (nx);
        cutoff->setValueNotifyingHost (cutoff->getNormalisableRange().convertTo0to1 (hz));
        reso->setValueNotifyingHost (ny);
        repaint();
    }

    void nudge (float dx, float dy)
    {
        if (dx != 0.0f)
            cutoff->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, cutoff->getValue() + dx));
        if (dy != 0.0f)
            reso->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, reso->getValue() + dy));
        repaint();
    }

    CalabiHatsProcessor& proc;
    juce::RangedAudioParameter* cutoff = nullptr;
    juce::RangedAudioParameter* reso = nullptr;
    float lastDrawn = -1.0f;
    bool highlight = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterPad)
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
            g.drawText (rowName (row),
                        juce::Rectangle<int> { 0, bounds.getY(), labelWidth - 4, bounds.getHeight() },
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

        // the isologo stands in for a bullet at the head of every section
        if (const auto& mark = cy::brandMark(); mark.isValid())
        {
            auto markArea = header.removeFromLeft (18).toFloat()
                                  .withSizeKeepingCentre (18.0f, 18.0f);
            g.drawImage (mark, markArea, juce::RectanglePlacement::centred);
            header.removeFromLeft (8);
        }

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
    void buildVoiceModules();

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
    juce::OwnedArray<ModuleBox> modules;
    std::unique_ptr<FilterPad> filterPad;
    /** Which knobs sit in each module, in the order they were added. */
    std::vector<std::vector<KnobBox*>> moduleKnobs;
    std::vector<KnobBox*> arpKnobs, euclidKnobs, depthKnobs;
    std::array<KnobBox*, 2> laneAmount { nullptr, nullptr };
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachments;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachments;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::OwnedArray<SourceChip> chips;
    juce::OwnedArray<LaneBars> laneBars;

    juce::TextButton arpOnButton { "ARP ON" }, triggerButton { "Trigger" };
    juce::TextButton midiOutButton { "MIDI OUT" };
    juce::ComboBox* midiChannelBox = nullptr;
    juce::Label* midiChannelLabel = nullptr;
    KnobBox* midiRootKnob = nullptr;
    juce::TextButton randomNotesButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\x84")) + " Random notes" };
    juce::TextButton generateButton { "Generate" };
    juce::ComboBox euTargetBox;

    StepGrid stepGrid { proc };
    juce::Label euclidReadout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CalabiHatsEditor)
};
