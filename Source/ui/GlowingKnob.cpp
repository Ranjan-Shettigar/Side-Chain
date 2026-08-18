#include "GlowingKnob.h"
#include "CustomLookAndFeel.h"

GlowingKnob::GlowingKnob()
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                        juce::MathConstants<float>::pi * 2.8f,
                        true);
}

void GlowingKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(6.0f);
    auto centre = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    float startAngle = juce::MathConstants<float>::pi * 1.2f;
    float endAngle   = juce::MathConstants<float>::pi * 2.8f;
    float normValue  = static_cast<float>(valueToProportionOfLength(getValue()));
    float currentAngle = startAngle + normValue * (endAngle - startAngle);

    // 1. Background Ring Track
    float trackThickness = 10.0f;
    float arcRadius = radius - trackThickness * 0.5f;

    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(juce::Colour::fromRGB(25, 30, 38));
    g.strokePath(bgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. Active Amount Arc (Cyan Glow)
    if (normValue > 0.001f)
    {
        juce::Path activeArc;
        activeArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, currentAngle, true);

        // Multi-layer bloom
        g.setColour(CustomLookAndFeel::accentCyan.withAlpha(0.2f));
        g.strokePath(activeArc, juce::PathStrokeType(trackThickness + 8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(CustomLookAndFeel::accentCyan.withAlpha(0.45f));
        g.strokePath(activeArc, juce::PathStrokeType(trackThickness + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(CustomLookAndFeel::accentCyan);
        g.strokePath(activeArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3. Dynamic Real-time Ducking Reduction Arc (Magenta/Red pulse inside the track)
    float duckIntensity = juce::jlimit(0.0f, 1.0f, 1.0f - currentGainReduction);
    if (duckIntensity > 0.01f && normValue > 0.01f)
    {
        float dynamicEndAngle = startAngle + (normValue * duckIntensity) * (endAngle - startAngle);
        float innerArcRadius = arcRadius - 8.0f;

        juce::Path duckArc;
        duckArc.addCentredArc(centre.x, centre.y, innerArcRadius, innerArcRadius, 0.0f, startAngle, dynamicEndAngle, true);

        g.setColour(CustomLookAndFeel::accentMagenta.withAlpha(0.35f * duckIntensity));
        g.strokePath(duckArc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(CustomLookAndFeel::accentMagenta);
        g.strokePath(duckArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 4. Inner Dial Cap
    float innerRadius = radius - trackThickness - 10.0f;
    auto innerBounds = juce::Rectangle<float>(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

    // Subtle drop shadow
    g.setColour(juce::Colour::fromRGB(8, 10, 14));
    g.fillEllipse(innerBounds.expanded(2.0f));

    // Metallic Dial Face
    juce::ColourGradient dialFace(juce::Colour::fromRGB(36, 42, 52), centre.x, centre.y - innerRadius,
                                  juce::Colour::fromRGB(18, 22, 28), centre.x, centre.y + innerRadius, false);
    g.setGradientFill(dialFace);
    g.fillEllipse(innerBounds);

    // Outer and Inner decorative rings
    g.setColour(CustomLookAndFeel::borderSubtle);
    g.drawEllipse(innerBounds, 1.5f);

    g.setColour(juce::Colour::fromRGB(30, 36, 45));
    g.drawEllipse(innerBounds.reduced(6.0f), 1.0f);

    // Indicator pointer line on the knob circumference
    float pointerX = centre.x + std::sin(currentAngle) * (innerRadius - 4.0f);
    float pointerY = centre.y - std::cos(currentAngle) * (innerRadius - 4.0f);
    float pointerInnerX = centre.x + std::sin(currentAngle) * (innerRadius - 16.0f);
    float pointerInnerY = centre.y - std::cos(currentAngle) * (innerRadius - 16.0f);

    g.setColour(CustomLookAndFeel::accentCyan);
    g.drawLine(pointerInnerX, pointerInnerY, pointerX, pointerY, 3.0f);

    // 5. Center Value & Label Readout
    g.setColour(CustomLookAndFeel::textSecondary);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("AMOUNT", static_cast<int>(centre.x - innerRadius), static_cast<int>(centre.y - innerRadius * 0.45f),
               static_cast<int>(innerRadius * 2.0f), 16, juce::Justification::centred);

    // Big percentage value
    g.setColour(CustomLookAndFeel::textPrimary);
    g.setFont(juce::Font(28.0f, juce::Font::bold));
    juce::String valStr = juce::String(static_cast<int>(getValue())) + "%";
    g.drawText(valStr, static_cast<int>(centre.x - innerRadius), static_cast<int>(centre.y - 14.0f),
               static_cast<int>(innerRadius * 2.0f), 30, juce::Justification::centred);

    // Real-time Ducking / GR badge
    if (duckIntensity > 0.05f)
    {
        float grDb = juce::Decibels::gainToDecibels(juce::jmax(0.001f, currentGainReduction));
        g.setColour(CustomLookAndFeel::accentMagenta);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(juce::String(grDb, 1) + " dB",
                   static_cast<int>(centre.x - innerRadius), static_cast<int>(centre.y + innerRadius * 0.35f),
                   static_cast<int>(innerRadius * 2.0f), 16, juce::Justification::centred);
    }
    else
    {
        g.setColour(CustomLookAndFeel::textSecondary.withAlpha(0.6f));
        g.setFont(juce::Font(10.0f, juce::Font::plain));
        g.drawText("DUCKING", static_cast<int>(centre.x - innerRadius), static_cast<int>(centre.y + innerRadius * 0.35f),
                   static_cast<int>(innerRadius * 2.0f), 16, juce::Justification::centred);
    }
}
