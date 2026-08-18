#include "CustomLookAndFeel.h"

// Official Google Material Design 3 Dark Theme Palette
const juce::Colour CustomLookAndFeel::m3Surface                  = juce::Colour::fromRGB(17, 19, 24);   // #111318
const juce::Colour CustomLookAndFeel::m3SurfaceContainerLowest   = juce::Colour::fromRGB(12, 14, 19);   // #0C0E13
const juce::Colour CustomLookAndFeel::m3SurfaceContainerLow      = juce::Colour::fromRGB(25, 28, 32);   // #191C20
const juce::Colour CustomLookAndFeel::m3SurfaceContainer         = juce::Colour::fromRGB(29, 32, 36);   // #1D2024
const juce::Colour CustomLookAndFeel::m3SurfaceContainerHigh     = juce::Colour::fromRGB(40, 42, 47);   // #282A2F
const juce::Colour CustomLookAndFeel::m3SurfaceContainerHighest  = juce::Colour::fromRGB(51, 53, 58);   // #33353A
const juce::Colour CustomLookAndFeel::m3Primary                 = juce::Colour::fromRGB(168, 199, 250); // #A8C7FA (M3 Soft Blue)
const juce::Colour CustomLookAndFeel::m3PrimaryContainer        = juce::Colour::fromRGB(8, 66, 160);   // #0842A0
const juce::Colour CustomLookAndFeel::m3Secondary               = juce::Colour::fromRGB(124, 219, 138); // #7CDB8A (M3 Mint)
const juce::Colour CustomLookAndFeel::m3Tertiary                = juce::Colour::fromRGB(244, 143, 177); // #F48FB1 (M3 Coral/Rose)
const juce::Colour CustomLookAndFeel::m3Outline                 = juce::Colour::fromRGB(68, 71, 78);   // #44474E
const juce::Colour CustomLookAndFeel::m3OutlineVariant          = juce::Colour::fromRGB(44, 46, 51);   // #2C2E33
const juce::Colour CustomLookAndFeel::m3OnSurface               = juce::Colour::fromRGB(226, 226, 230); // #E2E2E6
const juce::Colour CustomLookAndFeel::m3OnSurfaceVariant        = juce::Colour::fromRGB(196, 199, 208); // #C4C7D0

CustomLookAndFeel::CustomLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, m3Surface);
    setColour(juce::Slider::textBoxTextColourId, m3OnSurface);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, m3OnSurfaceVariant);
    setColour(juce::ComboBox::backgroundColourId, m3SurfaceContainerHigh);
    setColour(juce::ComboBox::textColourId, m3OnSurface);
    setColour(juce::ComboBox::outlineColourId, m3OutlineVariant);
    setColour(juce::PopupMenu::backgroundColourId, m3SurfaceContainerHigh);
    setColour(juce::PopupMenu::textColourId, m3OnSurface);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, m3SurfaceContainerHighest);
    setColour(juce::PopupMenu::highlightedTextColourId, m3Primary);
}

void CustomLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPosProportional,
                                         float rotaryStartAngle,
                                         float rotaryEndAngle,
                                         juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centre = bounds.getCentre();
    auto currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    float trackThickness = 4.0f;
    float arcRadius = radius - trackThickness * 0.5f;

    // 1. M3 Track Background Arc
    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(m3SurfaceContainerHighest);
    g.strokePath(bgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. M3 Active Track Arc
    if (sliderPosProportional > 0.001f)
    {
        juce::Path activeArc;
        activeArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, currentAngle, true);

        juce::Colour trackCol = m3Primary;
        if (slider.getName().containsIgnoreCase("gain") || slider.getName().containsIgnoreCase("sens"))
            trackCol = m3Secondary;
        else if (slider.getName().containsIgnoreCase("release") || slider.getName().containsIgnoreCase("attack"))
            trackCol = m3Primary;

        g.setColour(trackCol);
        g.strokePath(activeArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3. Dial Face (Flat M3 Surface Container High with subtle outline)
    float knobRadius = radius - trackThickness - 3.5f;
    auto knobBounds = juce::Rectangle<float>(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    g.setColour(m3SurfaceContainerHigh);
    g.fillEllipse(knobBounds);

    g.setColour(m3OutlineVariant);
    g.drawEllipse(knobBounds, 1.0f);

    // 4. M3 Pill Indicator Notch
    juce::Path indicator;
    float indLength = knobRadius * 0.55f;
    float indWidth = 3.0f;
    indicator.addRoundedRectangle(-indWidth * 0.5f, -knobRadius + 2.5f, indWidth, indLength, indWidth * 0.5f);
    indicator.applyTransform(juce::AffineTransform::rotation(currentAngle).translated(centre.x, centre.y));

    g.setColour(m3OnSurface);
    g.fillPath(indicator);
}

void CustomLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    bool isOn = button.getToggleState();

    // M3 Pill Container (Capsule shape)
    float cornerRadius = bounds.getHeight() * 0.5f;

    juce::Colour bgCol = isOn ? m3PrimaryContainer : m3SurfaceContainerHigh;
    if (shouldDrawButtonAsHighlighted)
        bgCol = bgCol.brighter(0.08f);

    g.setColour(bgCol);
    g.fillRoundedRectangle(bounds, cornerRadius);

    g.setColour(isOn ? m3Primary : m3OutlineVariant);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);

    // Pill Text & Status Indicator
    auto textBounds = bounds.reduced(8.0f, 2.0f);
    float dotRadius = 3.0f;
    float dotX = textBounds.getX() + 6.0f;
    float dotY = textBounds.getCentreY();

    g.setColour(isOn ? m3Primary : m3OnSurfaceVariant);
    g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);

    g.setColour(isOn ? m3OnSurface : m3OnSurfaceVariant);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText(),
                     static_cast<int>(dotX + dotRadius + 6.0f),
                     static_cast<int>(bounds.getY()),
                     static_cast<int>(bounds.getWidth() - (dotX + dotRadius + 8.0f)),
                     static_cast<int>(bounds.getHeight()),
                     juce::Justification::centredLeft, 1);
}

void CustomLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                             juce::Button& button,
                                             const juce::Colour& /*backgroundColour*/,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
    float cornerRadius = bounds.getHeight() * 0.5f; // Pill Chip shape

    bool isSelected = button.getToggleState();
    juce::Colour bg = isSelected ? m3SurfaceContainerHighest : m3SurfaceContainerHigh;

    if (shouldDrawButtonAsDown)
        bg = bg.darker(0.1f);
    else if (shouldDrawButtonAsHighlighted)
        bg = bg.brighter(0.08f);

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, cornerRadius);

    g.setColour(isSelected ? m3Primary : m3OutlineVariant);
    g.drawRoundedRectangle(bounds, cornerRadius, isSelected ? 1.5f : 1.0f);

    // Button Text
    g.setColour(isSelected ? m3OnSurface : m3OnSurfaceVariant);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
}

void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                     int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                     juce::ComboBox& /*box*/)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(1.0f);
    float cornerRadius = 8.0f;

    g.setColour(m3SurfaceContainerHigh);
    g.fillRoundedRectangle(bounds, cornerRadius);

    g.setColour(m3OutlineVariant);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);

    // M3 Chevron Arrow
    juce::Path arrow;
    float arrowX = width - 16.0f;
    float arrowY = height * 0.5f;
    arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.0f);
    arrow.lineTo(arrowX, arrowY + 2.0f);
    arrow.lineTo(arrowX + 4.0f, arrowY - 2.0f);

    g.setColour(m3OnSurfaceVariant);
    g.strokePath(arrow, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void CustomLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat();
    g.setColour(m3SurfaceContainerHigh);
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(m3OutlineVariant);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
}

void CustomLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                         bool isSeparator, bool isActive, bool isHighlighted, bool isTicked,
                                         bool /*hasSubMenu*/, const juce::String& text,
                                         const juce::String& /*shortcutKeyText*/,
                                         const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/)
{
    if (isSeparator)
    {
        g.setColour(m3OutlineVariant);
        g.drawLine(area.getX() + 8.0f, area.getCentreY(), area.getRight() - 8.0f, area.getCentreY(), 1.0f);
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour(m3SurfaceContainerHighest);
        g.fillRoundedRectangle(area.toFloat().reduced(4.0f, 2.0f), 6.0f);
    }

    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.setColour(isTicked ? m3Primary : (isActive ? m3OnSurface : m3OnSurfaceVariant.withAlpha(0.5f)));

    auto textRect = area.reduced(12, 0);
    g.drawText(text, textRect, juce::Justification::centredLeft, true);
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
    return juce::Font(11.0f, juce::Font::bold);
}
