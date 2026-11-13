#pragma once

#include <JuceHeader.h>

//==============================================================================
/** Messy Hat Tricks — the look: Calabi Yau's Cosmos palette on a hardware
    chassis of brushed metal, carbon fibre, bevels and screws. */
namespace cy
{
    //== palette ===============================================================
    const juce::Colour voidBg   { 0xff06040f };
    const juce::Colour panel    { 0xff120b26 };
    const juce::Colour slot     { 0xff0b0d10 };
    const juce::Colour line     { 0xff3a2870 };
    const juce::Colour ink      { 0xffeef1f6 };
    const juce::Colour dim      { 0xff97a0b0 };
    const juce::Colour faint    { 0xff6e7585 };
    const juce::Colour gold     { 0xffe9b24c };
    const juce::Colour lane1    { 0xffa855f7 };
    const juce::Colour lane2    { 0xff22d3ee };
    const juce::Colour velCol   { 0xff4ade80 };
    const juce::Colour randCol  { 0xfff472b6 };
    const juce::Colour deepSpace{ 0xff19134d };
    const juce::Colour nebula   { 0xff43296e };
    const juce::Colour violet   { 0xff5f18b5 };
    const juce::Colour electric { 0xff902bea };
    const juce::Colour orchid   { 0xffc560ef };
    const juce::Colour fuchsia  { 0xffd613d0 };
    const juce::Colour neon     { 0xff11e98a };
    const juce::Colour jade     { 0xff61d693 };
    const juce::Colour azure    { 0xff4d84d8 };
    const juce::Colour teal     { 0xff149ba7 };
    const juce::Colour steel    { 0xff9aa6b5 };
    const juce::Colour cream    { 0xffedd9c2 };

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

    //== embedded artwork ======================================================
    const juce::Image& logotype();       // MESSY HAT TRICKS
    const juce::Image& character();      // the drum-head player
    const juce::Image& stars();          // the three stars
    const juce::Image& malvinas();       // shown in the info panel
    const juce::Image& drumHead();       // the plain drum-head mark
    const juce::Image& calabiYau();      // the Calabi Yau logotype

    //== materials =============================================================
    /** Brushed aluminium: fine vertical grain under a soft top-down sheen. */
    inline void brushed (juce::Graphics& g, juce::Rectangle<int> r)
    {
        g.setGradientFill ({ juce::Colour (0xff43474e), (float) r.getX(), (float) r.getY(),
                             juce::Colour (0xff202429), (float) r.getX(), (float) r.getBottom(), false });
        g.fillRect (r);

        juce::Random rng (r.getX() * 7919 + r.getY() * 104729 + r.getWidth());
        for (int x = r.getX(); x < r.getRight(); x += 2)
        {
            g.setColour (juce::Colours::white.withAlpha (rng.nextFloat() * 0.035f));
            g.drawVerticalLine (x, (float) r.getY(), (float) r.getBottom());
            g.setColour (juce::Colours::black.withAlpha (rng.nextFloat() * 0.04f));
            g.drawVerticalLine (x + 1, (float) r.getY(), (float) r.getBottom());
        }
    }

    /** Carbon fibre: a woven diagonal over a dark ground. */
    inline void carbon (juce::Graphics& g, juce::Rectangle<int> r, float radius = 7.0f)
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r.toFloat(), radius);
        g.reduceClipRegion (clip);

        g.setGradientFill ({ juce::Colour (0xff24272c), (float) r.getX(), (float) r.getY(),
                             juce::Colour (0xff1b1e22), (float) r.getX(), (float) r.getBottom(), false });
        g.fillRect (r);

        const int step = 5;
        for (int d = -r.getHeight(); d < r.getWidth() + r.getHeight(); d += step)
        {
            g.setColour (juce::Colours::white.withAlpha (0.030f));
            g.drawLine ((float) (r.getX() + d), (float) r.getY(),
                        (float) (r.getX() + d + r.getHeight()), (float) r.getBottom(), 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.26f));
            g.drawLine ((float) (r.getX() + d + r.getHeight()), (float) r.getY(),
                        (float) (r.getX() + d), (float) r.getBottom(), 2.0f);
        }
    }

    /** A raised edge: light along the top, shadow along the bottom. */
    inline void bevel (juce::Graphics& g, juce::Rectangle<int> r, float radius = 7.0f)
    {
        auto f = r.toFloat().reduced (0.5f);
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.drawRoundedRectangle (f, radius, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (f.translated (0.0f, 1.0f), radius, 1.0f);
    }

    /** A recessed well for displays, pads and step grids. */
    inline void sunken (juce::Graphics& g, juce::Rectangle<int> r, float radius = 4.0f)
    {
        auto f = r.toFloat();
        g.setColour (slot);
        g.fillRoundedRectangle (f, radius);

        for (int i = 0; i < 4; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.42f - (float) i * 0.09f));
            g.drawRoundedRectangle (f.reduced ((float) i + 0.5f).withTrimmedBottom ((float) i),
                                    radius, 1.0f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawLine (f.getX() + radius, f.getBottom() - 0.5f, f.getRight() - radius, f.getBottom() - 0.5f, 1.0f);
    }

    /** A slotted screw head, for the chassis corners. */
    inline void screw (juce::Graphics& g, juce::Point<float> centre, float diameter = 11.0f)
    {
        const float r = diameter * 0.5f;
        auto box = juce::Rectangle<float> (centre.x - r, centre.y - r, diameter, diameter);

        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.fillEllipse (box.translated (0.0f, 1.0f));

        g.setGradientFill ({ juce::Colour (0xffb9c0c9), box.getX() + diameter * 0.3f, box.getY() + diameter * 0.25f,
                             juce::Colour (0xff3a3f46), box.getRight(), box.getBottom(), true });
        g.fillEllipse (box);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (box, 0.8f);

        juce::Path slotLine;
        slotLine.startNewSubPath (centre.x - r * 0.62f, centre.y);
        slotLine.lineTo (centre.x + r * 0.62f, centre.y);
        g.setColour (juce::Colour (0xff14161a).withAlpha (0.85f));
        g.strokePath (slotLine, juce::PathStrokeType (1.3f),
                      juce::AffineTransform::rotation (0.6f, centre.x, centre.y));
    }
}

//==============================================================================
/** Knobs, buttons and combo boxes as machined hardware. */
class MessyLookAndFeel  : public juce::LookAndFeel_V4
{
public:
    MessyLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, cy::gold);
        setColour (juce::ComboBox::backgroundColourId, cy::slot);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, cy::ink);
        setColour (juce::ComboBox::arrowColourId, cy::faint);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff22262b));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, cy::electric.withAlpha (0.55f));
        setColour (juce::PopupMenu::textColourId, cy::ink);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff3a3f47));
        setColour (juce::TextButton::buttonOnColourId, cy::electric);
        setColour (juce::TextButton::textColourOffId, cy::ink);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, cy::dim);
    }

    //== knob ==================================================================
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (3.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = startAngle + pos * (endAngle - startAngle);
        const auto track = radius - 2.0f;
        const auto knobR = track - 5.5f;
        const auto colour = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // the track, and the travelled part of it with its own glow
        juce::Path back;
        back.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff14171b));
        g.strokePath (back, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        juce::Path fill;
        fill.addCentredArc (centre.x, centre.y, track, track, 0.0f, startAngle,
                            juce::jmax (startAngle + 0.001f, angle), true);
        g.setColour (colour.withAlpha (0.30f));
        g.strokePath (fill, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (fill, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

        // tick marks around the dial
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        for (int i = 0; i <= 20; ++i)
        {
            const float a = startAngle + ((float) i / 20.0f) * (endAngle - startAngle);
            const float c = std::sin (a), s = -std::cos (a);
            g.drawLine (centre.x + c * (knobR + 1.5f), centre.y + s * (knobR + 1.5f),
                        centre.x + c * (knobR + 3.0f), centre.y + s * (knobR + 3.0f), 1.0f);
        }

        // the metal cylinder
        g.setColour (juce::Colour (0xff0f1114));
        g.fillEllipse (centre.x - knobR - 1.0f, centre.y - knobR - 1.0f,
                       (knobR + 1.0f) * 2.0f, (knobR + 1.0f) * 2.0f);

        g.setGradientFill ({ juce::Colour (0xff5b616a), centre.x - knobR * 0.4f, centre.y - knobR * 0.6f,
                             juce::Colour (0xff181b1f), centre.x + knobR, centre.y + knobR, true });
        g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.20f));
        g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

        // the pointer
        const juce::Point<float> tip (centre.x + std::sin (angle) * knobR * 0.74f,
                                      centre.y - std::cos (angle) * knobR * 0.74f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine ({ centre.translated (0.0f, 1.0f), tip.translated (0.0f, 1.0f) }, 2.4f);
        g.setColour (cy::ink);
        g.drawLine ({ centre, tip }, 2.0f);
    }

    //== button ================================================================
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& colour,
                               bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool lit = b.getToggleState();
        const float radius = 4.0f;

        auto top = lit ? colour.brighter (0.32f) : juce::Colour (0xff4a4f57);
        auto bot = lit ? colour.darker (0.35f)   : juce::Colour (0xff2c3036);
        if (down) { top = top.darker (0.25f); bot = bot.darker (0.25f); }
        else if (over) { top = top.brighter (0.10f); bot = bot.brighter (0.10f); }

        if (lit)
        {
            g.setColour (colour.withAlpha (0.38f));
            g.fillRoundedRectangle (r.expanded (2.5f), radius + 2.0f);
        }

        g.setGradientFill ({ top, r.getX(), r.getY(), bot, r.getX(), r.getBottom(), false });
        g.fillRoundedRectangle (r, radius);

        g.setColour (juce::Colours::white.withAlpha (lit ? 0.32f : 0.18f));
        g.drawRoundedRectangle (r.withTrimmedBottom (r.getHeight() * 0.55f), radius, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawRoundedRectangle (r.translated (0.0f, 0.5f), radius, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override
    {
        return juce::Font (juce::FontOptions (10.5f)).boldened();
    }

    //== combo box =============================================================
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int,
                       juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<int> (0, 0, w, h);
        cy::sunken (g, r, 4.0f);

        if (box.hasKeyboardFocus (false))
        {
            g.setColour (cy::gold.withAlpha (0.7f));
            g.drawRoundedRectangle (r.toFloat().reduced (1.0f), 4.0f, 1.2f);
        }

        juce::Path arrow;
        const float cx = (float) w - 12.0f, cy2 = (float) h * 0.5f;
        arrow.startNewSubPath (cx - 4.0f, cy2 - 2.0f);
        arrow.lineTo (cx, cy2 + 2.5f);
        arrow.lineTo (cx + 4.0f, cy2 - 2.0f);
        g.setColour (cy::faint);
        g.strokePath (arrow, juce::PathStrokeType (1.4f));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return juce::Font (juce::FontOptions (11.0f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 24, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }
};
