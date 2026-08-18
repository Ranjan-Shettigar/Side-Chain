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
    auto bounds = getLocalBounds().toFloat().reduced(5.0f);
    auto centre = bounds.getCentre();
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;

    float startAngle = juce::MathConstants<float>::pi * 1.2f;
    float endAngle   = juce::MathConstants<float>::pi * 2.8f;
    float normValue  = static_cast<float>(valueToProportionOfLength(getValue()));
    float currentAngle = startAngle + normValue * (endAngle - startAngle);

    // 1. M3 Track Background Arc
    float trackThickness = 7.0f;
    float arcRadius = radius - trackThickness * 0.5f;

    juce::Path bgArc;
    bgArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour(CustomLookAndFeel::m3SurfaceContainerHighest);
    g.strokePath(bgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 2. Active Primary Arc (Material 3 Soft Blue)
    if (normValue > 0.001f)
    {
        juce::Path activeArc;
        activeArc.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, currentAngle, true);

        g.setColour(CustomLookAndFeel::m3Primary);
        g.strokePath(activeArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3. Dynamic Real-time Ducking Reduction Arc (Material 3 Coral/Pink)
    float duckIntensity = juce::jlimit(0.0f, 1.0f, 1.0f - currentGainReduction);
    if (duckIntensity > 0.01f && normValue > 0.01f)
    {
        float dynamicEndAngle = startAngle + (normValue * duckIntensity) * (endAngle - startAngle);
        float innerArcRadius = arcRadius - 7.0f;

        juce::Path duckArc;
        duckArc.addCentredArc(centre.x, centre.y, innerArcRadius, innerArcRadius, 0.0f, startAngle, dynamicEndAngle, true);

        g.setColour(CustomLookAndFeel::m3Tertiary.withAlpha(0.4f));
        g.strokePath(duckArc, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(CustomLookAndFeel::m3Tertiary);
        g.strokePath(duckArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 4. M3 Center Dial Surface (Flat Minimalist Elevation)
    float innerRadius = radius - trackThickness - 7.0f;
    auto innerBounds = juce::Rectangle<float>(centre.x - innerRadius, centre.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

    g.setColour(CustomLookAndFeel::m3SurfaceContainerHigh);
    g.fillEllipse(innerBounds);

    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawEllipse(innerBounds, 1.0f);

    // M3 Pill Pointer on circumference
    float pointerX = centre.x + std::sin(currentAngle) * (innerRadius - 3.0f);
    float pointerY = centre.y - std::cos(currentAngle) * (innerRadius - 3.0f);
    float pointerInnerX = centre.x + std::sin(currentAngle) * (innerRadius - 12.0f);
    float pointerInnerY = centre.y - std::cos(currentAngle) * (innerRadius - 12.0f);

    g.setColour(CustomLookAndFeel::m3Primary);
    g.drawLine(pointerInnerX, pointerInnerY, pointerX, pointerY, 3.0f);

    // 5. Center Typography (Clean Minimalist Display)
    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("AMOUNT", static_cast<int>(centre.x - innerRadius), static_cast<int>(centre.y - innerRadius * 0.45f),
               static_cast<int>(innerRadius * 2.0f), 14, juce::Justification::centred);

    // Big clean percentage
    g.setColour(CustomLookAndFeel::m3OnSurface);
    g.setFont(juce::Font(26.0f, juce::Font::bold));
    juce::String valStr = juce::String(static_cast<int>(getValue())) + "%";
    g.drawText(valStr, static_cast<int>(centre.x - innerRadius), static_cast<int>(centre.y - 13.0f),
               static_cast<int>(innerRadius * 2.0f), 28, juce::Justification::centred);

    // Ducking Status Pill Badge
    if (duckIntensity > 0.05f)
    {
        float grDb = juce::Decibels::gainToDecibels(juce::jmax(0.001f, currentGainReduction));
        auto badgeRect = juce::Rectangle<float>(centre.x - 30.0f, centre.y + innerRadius * 0.32f, 60.0f, 16.0f);
        
        g.setColour(CustomLookAndFeel::m3Tertiary.withAlpha(0.2f));
        g.fillRoundedRectangle(badgeRect, 8.0f);

        g.setColour(CustomLookAndFeel::m3Tertiary);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(juce::String(grDb, 1) + " dB", badgeRect.toNearestInt(), juce::Justification::centred);
    }
    else
    {
        g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.6f));
        g.setFont(juce::Font(9.0f, juce::Font::plain));
        g.drawText("DUCKING", static_cast<int>(centre.x - innerRadius), static_cast<int>(centre.y + innerRadius * 0.35f),
                   static_cast<int>(innerRadius * 2.0f), 14, juce::Justification::centred);
    }
}
