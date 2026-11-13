#pragma once

#include <JuceHeader.h>
#include "Look.h"
#include "PluginProcessor.h"

//==============================================================================
/** A knob that accepts modulation sources dropped onto it. */
class KnobBox  : public juce::Component,
                 public juce::DragAndDropTarget
{
public:
    KnobBox (MessyHatTricksProcessor& p, int destination, const juce::String& paramID,
             const juce::String& displayName, juce::Colour tint)
        : proc (p), dest (destination), name (displayName), colour (tint)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        slider.setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (slider);

        param = p.apvts.getParameter (paramID);
        jassert (param != nullptr);

        if (param != nullptr)
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                p.apvts, paramID, slider);
        else
            slider.setEnabled (false);

        setName (displayName);
    }

    /** For parameters that are not modulation destinations. */
    KnobBox (MessyHatTricksProcessor& p, const juce::String& paramID,
             const juce::String& displayName, juce::Colour tint)
        : KnobBox (p, -1, paramID, displayName, tint) {}

    static constexpr int width = 62, height = 72;

    void resized() override
    {
        slider.setBounds (getLocalBounds().removeFromTop (getHeight() - 26).reduced (2));
    }

    void paint (juce::Graphics& g) override
    {
        if (highlight)
        {
            g.setColour (cy::gold.withAlpha (0.20f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
            g.setColour (cy::gold);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.0f);
        }

        auto area = getLocalBounds().removeFromBottom (26);

        g.setColour (cy::dim);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (name, area.removeFromTop (12), juce::Justification::centred, false);

        g.setColour (cy::ink);
        g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());
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

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return dest >= 0 && d.description.isInt();
    }

    void itemDragEnter (const SourceDetails&) override { highlight = true;  repaint(); }
    void itemDragExit  (const SourceDetails&) override { highlight = false; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        const int source = (int) d.description;
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

            const auto c = cy::sourceColour (s);
            const juce::Rectangle<float> box ((float) (dots.getX() + visible * 9),
                                              (float) dots.getY(), 7.0f, 7.0f);
            g.setColour (c.withAlpha (0.35f));
            g.fillEllipse (box.expanded (2.0f));
            g.setColour (c);
            g.fillEllipse (box);
            ++visible;
        }
    }

    MessyHatTricksProcessor& proc;
    int dest;
    juce::String name;
    juce::Colour colour;
    juce::Slider slider;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool highlight = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KnobBox)
};

//==============================================================================
/** One stage of the voice: its own tint, its name, and a line saying what it
    does in the synthesis. */
class ModuleBox  : public juce::Component
{
public:
    ModuleBox (const juce::String& titleText, const juce::String& blurbText, juce::Colour tint)
        : title (titleText), blurb (blurbText), colour (tint) {}

    static constexpr int headerHeight = 34;

    juce::Rectangle<int> contentArea() const
    {
        return getLocalBounds().withTrimmedTop (headerHeight).reduced (8, 0).withTrimmedBottom (6);
    }

    void paint (juce::Graphics& g) override
    {
        auto f = getLocalBounds().toFloat().reduced (0.5f);

        g.setGradientFill ({ colour.withAlpha (0.17f), f.getX(), f.getY(),
                             juce::Colours::black.withAlpha (0.42f), f.getX(), f.getBottom(), false });
        g.fillRoundedRectangle (f, 6.0f);

        g.setColour (juce::Colours::white.withAlpha (0.11f));
        g.drawRoundedRectangle (f.withTrimmedBottom (f.getHeight() * 0.6f), 6.0f, 1.0f);
        g.setColour (colour.withAlpha (0.45f));
        g.drawRoundedRectangle (f, 6.0f, 1.0f);

        cy::screw (g, { f.getRight() - 9.0f, f.getY() + 9.0f }, 8.0f);

        auto header = getLocalBounds().withHeight (headerHeight).reduced (10, 6);

        g.setColour (colour);
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        g.drawText (title, header.removeFromTop (14), juce::Justification::centredLeft, false);

        g.setColour (cy::dim);
        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        g.drawText (blurb, header, juce::Justification::centredLeft, false);
    }

private:
    juce::String title, blurb;
    juce::Colour colour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModuleBox)
};

//==============================================================================
/** The high-pass filter as an XY pad: cutoff across, resonance up. */
class FilterPad  : public juce::Component,
                   public juce::DragAndDropTarget,
                   private juce::Timer
{
public:
    explicit FilterPad (MessyHatTricksProcessor& p) : proc (p)
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
        auto bounds = getLocalBounds();
        cy::sunken (g, bounds, 5.0f);

        auto r = bounds.toFloat().reduced (2.0f);
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r, 4.0f);
        g.reduceClipRegion (clip);

        g.setFont (juce::Font (juce::FontOptions (9.0f)));
        for (float f : { 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f })
        {
            const float x = r.getX() + freqToX (f) * r.getWidth();
            g.setColour (cy::steel.withAlpha (0.16f));
            g.drawLine (x, r.getY(), x, r.getBottom(), 1.0f);
            g.setColour (cy::faint);
            g.drawText (f >= 1000.0f ? juce::String (f / 1000.0f, 0) + "k" : juce::String ((int) f),
                        juce::Rectangle<float> (x + 4.0f, r.getBottom() - 15.0f, 34.0f, 12.0f).toNearestInt(),
                        juce::Justification::left, false);
        }
        for (int i = 1; i < 4; ++i)
        {
            const float y = r.getY() + r.getHeight() * (float) i / 4.0f;
            g.setColour (cy::steel.withAlpha (0.12f));
            g.drawLine (r.getX(), y, r.getRight(), y, 1.0f);
        }

        // the response, from the same values the DSP runs on
        const float fc = cutoffHz(), q = resonance();
        juce::Path curve;
        for (float px = 0.0f; px <= r.getWidth(); px += 2.0f)
        {
            const float f = xToFreq (px / r.getWidth());
            const float ratio = f / fc;
            const float mag = ratio * ratio
                / std::sqrt (std::pow (1.0f - ratio * ratio, 2.0f) + std::pow (ratio / q, 2.0f));
            const float db = 20.0f * std::log10 (juce::jmax (1.0e-4f, mag));
            const float y = juce::jlimit (r.getY() - 40.0f, r.getBottom() + 40.0f,
                                          r.getCentreY() - db * (r.getHeight() / 48.0f));
            if (px == 0.0f) curve.startNewSubPath (r.getX() + px, y);
            else            curve.lineTo (r.getX() + px, y);
        }

        auto fill = curve;
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        g.setGradientFill ({ cy::gold.withAlpha (0.22f), r.getX(), r.getY(),
                             cy::gold.withAlpha (0.02f), r.getX(), r.getBottom(), false });
        g.fillPath (fill);

        g.setColour (cy::gold.withAlpha (0.35f));
        g.strokePath (curve, juce::PathStrokeType (5.0f));
        g.setColour (cy::gold);
        g.strokePath (curve, juce::PathStrokeType (2.2f));

        // the handle and its crosshair
        const auto h = handlePos();
        g.setColour (cy::neon.withAlpha (0.40f));
        g.drawLine (h.x, r.getY(), h.x, r.getBottom(), 1.0f);
        g.drawLine (r.getX(), h.y, r.getRight(), h.y, 1.0f);

        g.setColour (cy::neon.withAlpha (0.22f));
        g.fillEllipse (h.x - 15.0f, h.y - 15.0f, 30.0f, 30.0f);
        g.setColour (cy::neon);
        g.fillEllipse (h.x - 6.5f, h.y - 6.5f, 13.0f, 13.0f);
        g.setColour (cy::slot);
        g.fillEllipse (h.x - 2.8f, h.y - 2.8f, 5.6f, 5.6f);

        g.setColour (cy::ink);
        g.setFont (juce::Font (juce::FontOptions (11.5f)).boldened());
        g.drawText (cutoff->getCurrentValueAsText() + "   Q " + reso->getCurrentValueAsText(),
                    bounds.reduced (10, 8), juce::Justification::topRight, false);

        if (hasKeyboardFocus (false))
        {
            g.setColour (cy::gold);
            g.drawRoundedRectangle (r, 4.0f, 1.5f);
        }

        if (highlight)
        {
            g.setColour (cy::gold.withAlpha (0.18f));
            g.fillRoundedRectangle (r, 4.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { grabKeyboardFocus(); dragTo (e); }
    void mouseDrag (const juce::MouseEvent& e) override { dragTo (e); }

    bool keyPressed (const juce::KeyPress& k) override
    {
        const float step = k.getModifiers().isShiftDown() ? 0.002f : 0.02f;
        if (k.isKeyCode (juce::KeyPress::leftKey))  { nudge (-step, 0.0f); return true; }
        if (k.isKeyCode (juce::KeyPress::rightKey)) { nudge ( step, 0.0f); return true; }
        if (k.isKeyCode (juce::KeyPress::upKey))    { nudge (0.0f,  step); return true; }
        if (k.isKeyCode (juce::KeyPress::downKey))  { nudge (0.0f, -step); return true; }
        return false;
    }

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
        if (std::abs (now - lastDrawn) > 0.0005f) { lastDrawn = now; repaint(); }
    }

    static float freqToX (float f) { return std::log (f / fMin) / std::log (fMax / fMin); }
    static float xToFreq (float x) { return fMin * std::pow (fMax / fMin, juce::jlimit (0.0f, 1.0f, x)); }

    float cutoffHz()  const { return cutoff->getNormalisableRange().convertFrom0to1 (cutoff->getValue()); }
    float resonance() const { return reso->getNormalisableRange().convertFrom0to1 (reso->getValue()); }

    juce::Point<float> handlePos() const
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        return { r.getX() + freqToX (juce::jlimit (fMin, fMax, cutoffHz())) * r.getWidth(),
                 r.getBottom() - reso->getValue() * r.getHeight() };
    }

    void dragTo (const juce::MouseEvent& e)
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float nx = juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / juce::jmax (1.0f, r.getWidth()));
        const float ny = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - r.getY()) / juce::jmax (1.0f, r.getHeight()));

        cutoff->setValueNotifyingHost (cutoff->getNormalisableRange().convertTo0to1 (xToFreq (nx)));
        reso->setValueNotifyingHost (ny);
        repaint();
    }

    void nudge (float dx, float dy)
    {
        if (dx != 0.0f) cutoff->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, cutoff->getValue() + dx));
        if (dy != 0.0f) reso->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, reso->getValue() + dy));
        repaint();
    }

    MessyHatTricksProcessor& proc;
    juce::RangedAudioParameter* cutoff = nullptr;
    juce::RangedAudioParameter* reso = nullptr;
    float lastDrawn = -1.0f;
    bool highlight = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterPad)
};

//==============================================================================
/** The Euclidean pattern drawn as the cycle it is, with chords joining the
    pulses so the figure's interval structure is visible at a glance. */
class EuclidRing  : public juce::Component,
                    private juce::Timer
{
public:
    explicit EuclidRing (MessyHatTricksProcessor& p) : proc (p) { startTimerHz (24); }

    void paint (juce::Graphics& g) override
    {
        cy::sunken (g, getLocalBounds(), 5.0f);

        const int steps  = juce::jlimit (2, 16, (int) value ("euSteps"));
        const int pulses = juce::jlimit (0, steps, (int) value ("euPulses"));
        const int rot    = (int) value ("euRotate");
        const auto pattern = ch::euclid (pulses, steps, rot);

        auto area = getLocalBounds().toFloat().reduced (8.0f);
        const auto centre = area.getCentre();
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 10.0f;

        auto at = [&] (int i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / (float) steps
                            - juce::MathConstants<float>::halfPi;
            return juce::Point<float> (centre.x + std::cos (a) * radius,
                                       centre.y + std::sin (a) * radius);
        };

        g.setColour (cy::gold.withAlpha (0.18f));
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);
        g.setColour (cy::gold.withAlpha (0.10f));
        g.drawEllipse (centre.x - radius * 0.62f, centre.y - radius * 0.62f,
                       radius * 1.24f, radius * 1.24f, 1.0f);

        // the chords: joining the pulses draws the figure's own shape
        juce::Path chords;
        bool started = false;
        for (int i = 0; i < steps; ++i)
        {
            if (! pattern[(size_t) i])
                continue;

            const auto pt = at (i);
            if (! started) { chords.startNewSubPath (pt); started = true; }
            else           chords.lineTo (pt);
        }
        if (started)
        {
            chords.closeSubPath();
            g.setColour (cy::gold.withAlpha (0.22f));
            g.strokePath (chords, juce::PathStrokeType (4.5f));
            g.setColour (cy::gold.withAlpha (0.60f));
            g.strokePath (chords, juce::PathStrokeType (1.6f));
        }

        const int playing = proc.uiStep.load();
        const bool inSync = steps == juce::jlimit (1, 16, (int) value ("arpSteps"));

        for (int i = 0; i < steps; ++i)
        {
            const auto pt = at (i);

            if (pattern[(size_t) i])
            {
                g.setColour (cy::gold.withAlpha (0.22f));
                g.fillEllipse (pt.x - 11.0f, pt.y - 11.0f, 22.0f, 22.0f);
                g.setColour (cy::gold);
                g.fillEllipse (pt.x - 5.5f, pt.y - 5.5f, 11.0f, 11.0f);
            }
            else
            {
                g.setColour (cy::steel.withAlpha (0.45f));
                g.fillEllipse (pt.x - 2.6f, pt.y - 2.6f, 5.2f, 5.2f);
            }

            if (inSync && i == playing)
            {
                g.setColour (cy::neon);
                g.drawEllipse (pt.x - 10.5f, pt.y - 10.5f, 21.0f, 21.0f, 2.0f);
            }
        }

        // where the cycle begins
        g.setColour (cy::steel.withAlpha (0.8f));
        g.drawLine (centre.x, centre.y - radius - 9.0f, centre.x, centre.y - radius - 3.0f, 1.5f);

        g.setColour (cy::gold);
        g.setFont (juce::Font (juce::FontOptions (16.0f)).boldened());
        g.drawText ("E(" + juce::String (pulses) + ", " + juce::String (steps) + ")",
                    getLocalBounds().withTrimmedTop (getHeight() / 2 - 20).withHeight (22),
                    juce::Justification::centred, false);

        g.setColour (cy::faint);
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("ROT " + juce::String (rot),
                    getLocalBounds().withTrimmedTop (getHeight() / 2 + 4).withHeight (16),
                    juce::Justification::centred, false);
    }

private:
    void timerCallback() override
    {
        const auto now = value ("euPulses") * 1.0e6f + value ("euSteps") * 1.0e4f
                        + value ("euRotate") * 100.0f + (float) proc.uiStep.load() * 0.01f;
        if (std::abs (now - lastDrawn) > 1.0e-3f) { lastDrawn = now; repaint(); }
    }

    float value (const char* id) const
    {
        if (auto* p = proc.apvts.getRawParameterValue (id))
            return p->load();
        return 0.0f;
    }

    MessyHatTricksProcessor& proc;
    float lastDrawn = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EuclidRing)
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
        auto r = getLocalBounds().toFloat().reduced (0.5f);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (c.withAlpha (0.18f));
        g.fillRoundedRectangle (r.reduced (1.0f), 3.0f);
        g.setColour (c);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        g.setFont (juce::Font (juce::FontOptions (9.5f)).boldened());
        g.drawText (cy::sourceNames()[index], getLocalBounds(), juce::Justification::centred, false);
    }

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! container->isDragAndDropActive())
                container->startDragging (index, this);
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

private:
    int index;
};

//==============================================================================
/** The arpeggiator's On / Pitch / Vel / Prob rows. */
class StepGrid  : public juce::Component
{
public:
    explicit StepGrid (MessyHatTricksProcessor& p) : proc (p) {}

    static constexpr int labelWidth = 40;

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
                        juce::Rectangle<int> { 0, bounds.getY(), labelWidth - 5, bounds.getHeight() },
                        juce::Justification::centredRight, false);

            for (int i = 0; i < 16; ++i)
            {
                auto cell = cellBounds (row, i).toFloat();
                const float alpha = i >= len ? 0.3f : 1.0f;

                g.setColour (juce::Colours::black.withAlpha (0.5f * alpha));
                g.fillRoundedRectangle (cell, 1.5f);

                switch (row)
                {
                    case 0:
                        if (proc.pattern.stepOn[(size_t) i].load())
                        {
                            g.setGradientFill ({ cy::velCol.brighter (0.2f).withMultipliedAlpha (alpha),
                                                 cell.getX(), cell.getY(),
                                                 cy::velCol.darker (0.45f).withMultipliedAlpha (alpha),
                                                 cell.getX(), cell.getBottom(), false });
                            g.fillRoundedRectangle (cell.reduced (0.5f), 1.5f);
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
                        g.setColour (cy::steel.withAlpha (0.28f));
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
                        g.setGradientFill ({ cy::lane1.withMultipliedAlpha (alpha), cell.getX(), cell.getBottom() - h,
                                             cy::lane1.darker (0.7f).withMultipliedAlpha (alpha), cell.getX(), cell.getBottom(), false });
                        g.fillRect (cell.getX(), cell.getBottom() - h, cell.getWidth(), h);
                        break;
                    }

                    default:
                    {
                        const float v = proc.pattern.stepProb[(size_t) i].load();
                        const float h = juce::jmax (2.0f, v * cell.getHeight());
                        g.setGradientFill ({ cy::lane2.brighter (0.25f).withMultipliedAlpha (alpha), cell.getX(), cell.getBottom() - h,
                                             cy::teal.darker (0.4f).withMultipliedAlpha (alpha), cell.getX(), cell.getBottom(), false });
                        g.fillRect (cell.getX(), cell.getBottom() - h, cell.getWidth(), h);
                        if (v < 0.995f)
                        {
                            g.setColour (juce::Colours::white.withAlpha (0.9f * alpha));
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
                    g.drawRoundedRectangle (cell.reduced (0.5f), 1.5f, 1.0f);
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { painted.clear(); apply (e, true); }
    void mouseDrag (const juce::MouseEvent& e) override { apply (e, false); }

private:
    static juce::String rowName (int row)
    {
        switch (row) { case 0: return "ON"; case 1: return "PITCH"; case 2: return "VEL"; default: return "PROB"; }
    }

    static int rowHeight (int row)
    {
        switch (row) { case 0: return 17; case 1: return 44; case 2: return 44; default: return 27; }
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
            if (rowBounds (row).getY() - 1 <= y && y <= rowBounds (row).getBottom() + 1)
                return row;
        return -1;
    }

    MessyHatTricksProcessor& proc;
    juce::Array<int> painted;
};

//==============================================================================
/** The sixteen draggable bars of one modulation lane. */
class LaneBars  : public juce::Component
{
public:
    LaneBars (MessyHatTricksProcessor& p, int laneIndex) : proc (p), lane (laneIndex) {}

    void paint (juce::Graphics& g) override
    {
        cy::sunken (g, getLocalBounds(), 4.0f);

        const auto colour = lane == 0 ? cy::lane1 : cy::lane2;
        for (int i = 0; i < 16; ++i)
        {
            auto cell = cellBounds (i).toFloat();
            const float v = proc.pattern.lane[(size_t) lane][(size_t) i].load();
            const float h = juce::jmax (2.0f, v * cell.getHeight());

            g.setGradientFill ({ colour, cell.getX(), cell.getBottom() - h,
                                 colour.darker (0.75f), cell.getX(), cell.getBottom(), false });
            g.fillRoundedRectangle (cell.getX(), cell.getBottom() - h, cell.getWidth(), h, 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { apply (e); }
    void mouseDrag (const juce::MouseEvent& e) override { apply (e); }

private:
    juce::Rectangle<int> cellBounds (int index) const
    {
        auto r = getLocalBounds().reduced (3);
        const float w = (float) r.getWidth() / 16.0f;
        return juce::Rectangle<float> (r.getX() + w * (float) index, (float) r.getY(),
                                       w - 2.0f, (float) r.getHeight()).toNearestInt();
    }

    void apply (const juce::MouseEvent& e)
    {
        auto r = getLocalBounds().reduced (3);
        const int index = juce::jlimit (0, 15, (int) ((float) (e.x - r.getX()) / (float) r.getWidth() * 16.0f));
        proc.pattern.lane[(size_t) lane][(size_t) index] =
            juce::jlimit (0.0f, 1.0f, 1.0f - (float) (e.y - r.getY()) / (float) r.getHeight());
        repaint();
    }

    MessyHatTricksProcessor& proc;
    int lane;
};

//==============================================================================
/** A chassis panel whose header folds its content away. */
class Section  : public juce::Component
{
public:
    Section (const juce::String& titleText, juce::Component& contentToHold, int contentHeight)
        : title (titleText), content (contentToHold), height (contentHeight)
    {
        addAndMakeVisible (content);
    }

    static constexpr int headerHeight = 28;

    int preferredHeight() const { return headerHeight + (folded ? 6 : height + 21); }
    void setContentHeight (int h) { height = h; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        cy::carbon (g, r, 7.0f);
        cy::bevel (g, r, 7.0f);

        auto header = r.withHeight (headerHeight);
        g.setGradientFill ({ juce::Colours::white.withAlpha (0.07f), (float) header.getX(), (float) header.getY(),
                             juce::Colours::black.withAlpha (0.25f), (float) header.getX(), (float) header.getBottom(), false });
        g.fillRect (header);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine ((float) header.getX(), (float) header.getBottom() - 0.5f,
                    (float) header.getRight(), (float) header.getBottom() - 0.5f, 1.0f);

        auto strip = header.reduced (14, 0);

        if (const auto& mark = cy::drumHead(); mark.isValid())
        {
            auto markArea = strip.removeFromLeft (17).toFloat().withSizeKeepingCentre (17.0f, 17.0f);
            g.drawImage (mark, markArea, juce::RectanglePlacement::centred);
            strip.removeFromLeft (9);
        }

        g.setColour (cy::gold);
        g.setFont (juce::Font (juce::FontOptions (10.0f)).boldened());
        g.drawText (title.toUpperCase(), strip.removeFromLeft (strip.getWidth() - 20),
                    juce::Justification::centredLeft, false);

        g.setColour (cy::faint);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawText (folded ? "+" : juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x93")),
                    strip, juce::Justification::centredRight, false);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        area.removeFromTop (headerHeight);
        content.setVisible (! folded);
        if (! folded)
            content.setBounds (area.reduced (14, 0).withTrimmedTop (11).withTrimmedBottom (10));
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
/** What the character shows when you click it. */
class InfoPanel  : public juce::Component
{
public:
    InfoPanel()
    {
        close.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95")));
        close.onClick = [this] { setVisible (false); };
        addAndMakeVisible (close);
        setAlwaysOnTop (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff06080b).withAlpha (0.84f));

        auto card = cardBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (card.translated (0.0f, 8.0f).expanded (4.0f), 11.0f);

        g.setGradientFill ({ juce::Colour (0xfff7f4ee), card.getX(), card.getY(),
                             juce::Colour (0xffe8e3d9), card.getX(), card.getBottom(), false });
        g.fillRoundedRectangle (card, 9.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (card.reduced (0.5f), 9.0f, 1.0f);

        auto inner = cardBounds().reduced (24, 20);

        if (const auto& mv = cy::malvinas(); mv.isValid())
        {
            auto art = inner.removeFromTop (178).toFloat();
            g.drawImage (mv, art.withSizeKeepingCentre (176.0f, 165.0f),
                         juce::RectanglePlacement::centred);
            inner.removeFromTop (8);
        }

        g.setColour (juce::Colour (0xff1d2026));
        g.setFont (juce::Font (juce::FontOptions (15.0f)).boldened());
        g.drawText ("MESSY HAT TRICKS", inner.removeFromTop (20), juce::Justification::centred, false);

        g.setColour (juce::Colour (0xff4b5058));
        g.setFont (juce::Font (juce::FontOptions (11.5f)));
        for (auto line : { "Hi-hat engine  ·  version 1.0",
                           "Built by Facundo Melchor — Calabi Yau · Aarhus",
                           "Character by Nico Nigsar",
                           "Free software under the AGPLv3" })
        {
            g.drawText (line, inner.removeFromTop (17), juce::Justification::centred, false);
        }

        inner.removeFromTop (8);
        g.setColour (juce::Colour (0xff8a8275));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText ("CALABI YAU INSTRUMENTS", inner.removeFromTop (14),
                    juce::Justification::centred, false);
    }

    void resized() override
    {
        auto card = cardBounds();
        close.setBounds (card.getRight() - 32, card.getY() + 8, 24, 24);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! cardBounds().contains (e.getPosition()))
            setVisible (false);
    }

private:
    juce::Rectangle<int> cardBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre (470, 360);
    }

    struct CloseButton : juce::TextButton
    {
        void paintButton (juce::Graphics& g, bool, bool down) override
        {
            g.setColour (juce::Colour (down ? 0xff2b2b2b : 0xff6a6358));
            g.setFont (juce::Font (juce::FontOptions (16.0f)));
            g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);
        }
    };

    CloseButton close;
};
