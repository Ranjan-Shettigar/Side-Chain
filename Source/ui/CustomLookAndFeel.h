#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Custom LookAndFeel inspired by Google Material Design 3 (Material You) Dark Theme
 */
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

    void drawLinearSlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float minSliderPos,
                          float maxSliderPos,
                          const juce::Slider::SliderStyle style,
                          juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g,
                          juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                           bool hasSubMenu, const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawLabel(juce::Graphics& g, juce::Label& label) override;
    juce::Font getLabelFont(juce::Label& label) override;

    // Material Design 3 Dark Theme Palette Tokens
    static const juce::Colour m3Surface;
    static const juce::Colour m3SurfaceContainerLowest;
    static const juce::Colour m3SurfaceContainerLow;
    static const juce::Colour m3SurfaceContainer;
    static const juce::Colour m3SurfaceContainerHigh;
    static const juce::Colour m3SurfaceContainerHighest;
    static const juce::Colour m3Primary;
    static const juce::Colour m3PrimaryContainer;
    static const juce::Colour m3Secondary;
    static const juce::Colour m3Tertiary;
    static const juce::Colour m3Outline;
    static const juce::Colour m3OutlineVariant;
    static const juce::Colour m3OnSurface;
    static const juce::Colour m3OnSurfaceVariant;
};
