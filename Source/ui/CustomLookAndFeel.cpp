#include "CustomLookAndFeel.h"

const juce::Colour CustomLookAndFeel::bgDark        = juce::Colour::fromRGB(13, 17, 23);
const juce::Colour CustomLookAndFeel::bgCard        = juce::Colour::fromRGB(22, 27, 34);
const juce::Colour CustomLookAndFeel::bgCardInner   = juce::Colour::fromRGB(16, 20, 26);
const juce::Colour CustomLookAndFeel::borderSubtle  = juce::Colour::fromRGB(48, 54, 61);
const juce::Colour CustomLookAndFeel::accentCyan    = juce::Colour::fromRGB(0, 229, 255);
const juce::Colour CustomLookAndFeel::accentMagenta = juce::Colour::fromRGB(255, 64, 129);
const juce::Colour CustomLookAndFeel::accentAmber   = juce::Colour::fromRGB(255, 179, 0);
const juce::Colour CustomLookAndFeel::textPrimary   = juce::Colour::fromRGB(240, 246, 252);
const juce::Colour CustomLookAndFeel::textSecondary = juce::Colour::fromRGB(139, 148, 158);

CustomLookAndFeel::CustomLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, bgDark);
    setColour(juce::Slider::textBoxTextColourId, textPrimary);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, textSecondary);
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPosProportional,
                                         float rotaryStartAngle,
                                         float rotaryEndAngle,
                                         juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    auto currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Track arc
    float trackThickness = 4.5f;
    float arcRadius = radius - trackThickness * 0.5f;

    // Background track path
    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour::fromRGB(35, 42, 52));
    g.strokePath(bgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active track path
    if (sliderPosProportional > 0.001f)
    {
        juce::Path activeArc;
        activeArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, currentAngle, true);
        
        juce::Colour trackCol = accentCyan;
        if (slider.getName().containsIgnoreCase("low") || slider.getName().containsIgnoreCase("high"))
            trackCol = accentAmber;
        else if (slider.getName().containsIgnoreCase("gain"))
            trackCol = juce::Colour::fromRGB(76, 209, 55);

        // Subtle glow
        g.setColour(trackCol.withAlpha(0.35f));
        g.strokePath(activeArc, juce::PathStrokeType(trackThickness + 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(trackCol);
        g.strokePath(activeArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Knob dial body
    float knobRadius = radius - trackThickness - 4.0f;
    auto knobBounds = juce::Rectangle<float>(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    // Outer shadow / border
    g.setColour(juce::Colour::fromRGB(10, 12, 16));
    g.fillEllipse(knobBounds.expanded(1.0f));

    // Dial face gradient
    juce::ColourGradient dialGrad(juce::Colour::fromRGB(38, 45, 56), centre.x, centre.y - knobRadius,
                                 juce::Colour::fromRGB(20, 24, 30), centre.x, centre.y + knobRadius, false);
    g.setGradientFill(dialGrad);
    g.fillEllipse(knobBounds);

    // Subtle inner rim
    g.setColour(borderSubtle.withAlpha(0.6f));
    g.drawEllipse(knobBounds, 1.2f);

    // Indicator needle notch
    juce::Path needle;
    float needleLength = knobRadius * 0.65f;
    float needleWidth = 3.0f;
    needle.addRoundedRectangle(-needleWidth * 0.5f, -knobRadius + 3.0f, needleWidth, needleLength, 1.5f);
    needle.applyTransform(juce::AffineTransform::rotation(currentAngle).translated(centre.x, centre.y));

    g.setColour(textPrimary);
    g.fillPath(needle);
}

void CustomLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    bool isOn = button.getToggleState();

    juce::Colour baseBg = isOn ? juce::Colour::fromRGB(32, 45, 58) : bgCard;
    if (shouldDrawButtonAsHighlighted)
        baseBg = baseBg.brighter(0.1f);

    g.setColour(baseBg);
    g.fillRoundedRectangle(bounds, 6.0f);

    juce::Colour borderCol = isOn ? accentCyan : borderSubtle;
    if (button.getName().containsIgnoreCase("listen") && isOn)
        borderCol = accentAmber;
    else if (button.getName().containsIgnoreCase("bypass") && isOn)
        borderCol = accentMagenta;

    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, 6.0f, isOn ? 1.5f : 1.0f);

    // Text & LED indicator
    auto textBounds = bounds.reduced(6.0f, 2.0f);
    float ledRadius = 3.5f;
    float ledX = textBounds.getX() + 6.0f;
    float ledY = textBounds.getCentreY();

    g.setColour(isOn ? borderCol : juce::Colour::fromRGB(60, 68, 77));
    g.fillEllipse(ledX - ledRadius, ledY - ledRadius, ledRadius * 2.0f, ledRadius * 2.0f);

    if (isOn)
    {
        g.setColour(borderCol.withAlpha(0.4f));
        g.fillEllipse(ledX - ledRadius - 2.0f, ledY - ledRadius - 2.0f, (ledRadius + 2.0f) * 2.0f, (ledRadius + 2.0f) * 2.0f);
    }

    g.setColour(isOn ? textPrimary : textSecondary);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText(),
                     static_cast<int>(ledX + ledRadius + 6.0f),
                     static_cast<int>(bounds.getY()),
                     static_cast<int>(bounds.getWidth() - (ledX + ledRadius + 8.0f)),
                     static_cast<int>(bounds.getHeight()),
                     juce::Justification::centredLeft, 1);
}

void CustomLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited())
    {
        auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font(getLabelFont(label));

        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(font);

        auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
                         juce::jmax(1, (int)(textArea.getHeight() / (float)font.getHeight())),
                         label.getMinimumHorizontalScale());
    }
}

juce::Font CustomLookAndFeel::getLabelFont(juce::Label& /*label*/)
{
    return juce::Font(12.0f, juce::Font::bold);
}
