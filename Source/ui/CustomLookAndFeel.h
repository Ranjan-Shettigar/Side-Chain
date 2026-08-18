#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();
    ~CustomLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    juce::Font getLabelFont(juce::Label& label) override;

    // Palette
    static const juce::Colour bgDark;
    static const juce::Colour bgCard;
    static const juce::Colour bgCardInner;
    static const juce::Colour borderSubtle;
    static const juce::Colour accentCyan;
    static const juce::Colour accentMagenta;
    static const juce::Colour accentAmber;
    static const juce::Colour textPrimary;
    static const juce::Colour textSecondary;
};
