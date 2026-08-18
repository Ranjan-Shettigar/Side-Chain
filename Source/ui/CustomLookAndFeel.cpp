#include "CustomLookAndFeel.h"

// Material Design 3 Tonal Dark Palette
const juce::Colour CustomLookAndFeel::m3Surface                 = juce::Colour::fromRGB(17, 19, 24);   // #111318
const juce::Colour CustomLookAndFeel::m3SurfaceContainerLowest   = juce::Colour::fromRGB(12, 14, 19);   // #0C0E13
const juce::Colour CustomLookAndFeel::m3SurfaceContainerLow      = juce::Colour::fromRGB(25, 28, 33);   // #191C21
const juce::Colour CustomLookAndFeel::m3SurfaceContainer         = juce::Colour::fromRGB(29, 32, 36);   // #1D2024
const juce::Colour CustomLookAndFeel::m3SurfaceContainerHigh     = juce::Colour::fromRGB(40, 42, 47);   // #282A2F
const juce::Colour CustomLookAndFeel::m3SurfaceContainerHighest  = juce::Colour::fromRGB(51, 53, 58);   // #33353A

const juce::Colour CustomLookAndFeel::m3Primary                  = juce::Colour::fromRGB(168, 199, 250); // #A8C7FA Soft Blue
const juce::Colour CustomLookAndFeel::m3PrimaryContainer         = juce::Colour::fromRGB(8, 66, 160);   // #0842A0
const juce::Colour CustomLookAndFeel::m3Secondary                = juce::Colour::fromRGB(124, 219, 138); // #7CDB8A Mint Green
const juce::Colour CustomLookAndFeel::m3Tertiary                 = juce::Colour::fromRGB(244, 143, 177); // #F48FB1 Coral Pink

const juce::Colour CustomLookAndFeel::m3Outline                  = juce::Colour::fromRGB(68, 71, 78);   // #44474E
const juce::Colour CustomLookAndFeel::m3OutlineVariant           = juce::Colour::fromRGB(44, 46, 51);   // #2C2E33
const juce::Colour CustomLookAndFeel::m3OnSurface                = juce::Colour::fromRGB(226, 226, 233); // #E2E2E9
const juce::Colour CustomLookAndFeel::m3OnSurfaceVariant         = juce::Colour::fromRGB(196, 198, 207); // #C4C6CF

CustomLookAndFeel::CustomLookAndFeel()
{
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, m3SurfaceContainerHigh);
    setColour(juce::Slider::textBoxTextColourId, m3OnSurface);
    setColour(juce::Slider::textBoxHighlightColourId, m3PrimaryContainer);

    setColour(juce::ComboBox::backgroundColourId, m3SurfaceContainerHigh);
    setColour(juce::ComboBox::textColourId, m3OnSurface);
    setColour(juce::ComboBox::outlineColourId, m3OutlineVariant);
    setColour(juce::ComboBox::arrowColourId, m3OnSurfaceVariant);

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

    // 1. Track Background Arc
    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(slider.isEnabled() ? m3SurfaceContainerHighest : m3SurfaceContainerHighest.withAlpha(0.4f));
    g.strokePath(bgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. Active Track Arc
    if (slider.isEnabled())
    {
        bool isBalance = slider.getName().containsIgnoreCase("balance");
        float centerAngle = 0.5f * (rotaryStartAngle + rotaryEndAngle);

        if (isBalance)
        {
            float arcStart = centerAngle;
            float arcEnd = currentAngle;
            if (arcStart > arcEnd) std::swap(arcStart, arcEnd);

            if (std::abs(sliderPosProportional - 0.5f) > 0.008f)
            {
                juce::Path balArc;
                balArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, arcStart, arcEnd, true);
                g.setColour(m3OnSurface);
                g.strokePath(balArc, juce::PathStrokeType(trackThickness + 1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            // Center Notch Pip at 12 o'clock
            float topX = centre.x + std::sin(centerAngle) * arcRadius;
            float topY = centre.y - std::cos(centerAngle) * arcRadius;
            g.setColour(m3Outline.withAlpha(0.7f));
            g.fillEllipse(topX - 1.5f, topY - 1.5f, 3.0f, 3.0f);
        }
        else if (sliderPosProportional > 0.001f)
        {
            juce::Path activeArc;
            activeArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, currentAngle, true);

            juce::Colour trackCol = m3Primary;
            if (slider.getName().containsIgnoreCase("gain") || slider.getName().containsIgnoreCase("sens"))
                trackCol = m3Secondary;

            g.setColour(trackCol);
            g.strokePath(activeArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // 3. Dial Face
    float knobRadius = radius - trackThickness - 3.5f;
    auto knobBounds = juce::Rectangle<float>(centre.x - knobRadius, centre.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

    g.setColour(slider.isEnabled() ? m3SurfaceContainerHigh : m3SurfaceContainerHigh.withAlpha(0.4f));
    g.fillEllipse(knobBounds);

    g.setColour(m3OutlineVariant);
    g.drawEllipse(knobBounds, 1.0f);

    // Indicator Notch
    if (slider.isEnabled())
    {
        float pointerX = centre.x + std::sin(currentAngle) * (knobRadius - 2.0f);
        float pointerY = centre.y - std::cos(currentAngle) * (knobRadius - 2.0f);
        float pointerInnerX = centre.x + std::sin(currentAngle) * (knobRadius - 6.5f);
        float pointerInnerY = centre.y - std::cos(currentAngle) * (knobRadius - 6.5f);

        g.setColour(m3OnSurface);
        g.drawLine(pointerInnerX, pointerInnerY, pointerX, pointerY, 2.5f);
    }
}

void CustomLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPos,
                                         float minSliderPos,
                                         float maxSliderPos,
                                         const juce::Slider::SliderStyle /*style*/,
                                         juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
    float cornerRadius = 6.0f;

    // Track Background
    g.setColour(slider.isEnabled() ? m3SurfaceContainerHigh : m3SurfaceContainerHigh.withAlpha(0.4f));
    g.fillRoundedRectangle(bounds, cornerRadius);

    // Active Bar
    float minX = minSliderPos;
    float maxX = maxSliderPos;
    float currentX = sliderPos;

    float norm = (maxX > minX) ? juce::jlimit(0.0f, 1.0f, (currentX - minX) / (maxX - minX)) : 0.5f;
    auto activeBounds = juce::Rectangle<float>(bounds.getX(), bounds.getY(), bounds.getWidth() * norm, bounds.getHeight());

    if (slider.isEnabled() && norm > 0.01f)
    {
        g.setColour(m3PrimaryContainer.withAlpha(0.6f));
        g.fillRoundedRectangle(activeBounds, cornerRadius);
    }

    g.setColour(m3OutlineVariant);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);
}

void CustomLookAndFeel::drawToggleButton(juce::Graphics& g,
                                         juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted,
                                         bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
    bool isOn = button.getToggleState();

    float cornerRadius = bounds.getHeight() * 0.5f;

    juce::Colour bgCol = isOn ? m3PrimaryContainer : m3SurfaceContainerHigh;
    if (shouldDrawButtonAsHighlighted && button.isEnabled())
        bgCol = bgCol.brighter(0.08f);

    g.setColour(button.isEnabled() ? bgCol : bgCol.withAlpha(0.35f));
    g.fillRoundedRectangle(bounds, cornerRadius);

    g.setColour(isOn ? m3Primary : m3OutlineVariant);
    g.drawRoundedRectangle(bounds, cornerRadius, 1.0f);

    auto textBounds = bounds.reduced(8.0f, 2.0f);
    float dotRadius = 3.0f;
    float dotX = textBounds.getX() + 6.0f;
    float dotY = textBounds.getCentreY();

    g.setColour(isOn ? m3Primary : m3OnSurfaceVariant.withAlpha(button.isEnabled() ? 1.0f : 0.4f));
    g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2.0f, dotRadius * 2.0f);

    g.setColour(isOn ? m3OnSurface : m3OnSurfaceVariant.withAlpha(button.isEnabled() ? 1.0f : 0.4f));
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
    float cornerRadius = bounds.getHeight() * 0.5f;

    bool isSelected = button.getToggleState();
    juce::Colour bg = isSelected ? m3SurfaceContainerHighest : m3SurfaceContainerHigh;

    if (shouldDrawButtonAsDown && button.isEnabled())
        bg = bg.darker(0.1f);
    else if (shouldDrawButtonAsHighlighted && button.isEnabled())
        bg = bg.brighter(0.08f);

    g.setColour(button.isEnabled() ? bg : bg.withAlpha(0.35f));
    g.fillRoundedRectangle(bounds, cornerRadius);

    g.setColour(isSelected ? m3Primary : m3OutlineVariant);
    g.drawRoundedRectangle(bounds, cornerRadius, isSelected ? 1.5f : 1.0f);
}

void CustomLookAndFeel::drawButtonText(juce::Graphics& g,
                                       juce::TextButton& button,
                                       bool /*shouldDrawButtonAsHighlighted*/,
                                       bool /*shouldDrawButtonAsDown*/)
{
    bool isSelected = button.getToggleState();
    juce::Colour textCol = isSelected ? m3OnSurface : m3OnSurfaceVariant;
    if (!button.isEnabled())
        textCol = textCol.withAlpha(0.35f);

    g.setColour(textCol);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
}

void CustomLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                     int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                     juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(1.0f);
    float cornerRadius = 8.0f;

    g.setColour(box.isEnabled() ? m3SurfaceContainerHigh : m3SurfaceContainerHigh.withAlpha(0.35f));
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

    g.setColour(box.isEnabled() ? m3OnSurfaceVariant : m3OnSurfaceVariant.withAlpha(0.35f));
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
        auto alpha = label.isEnabled() ? 1.0f : 0.4f;
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
