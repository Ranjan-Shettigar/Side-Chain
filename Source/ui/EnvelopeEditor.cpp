#include "EnvelopeEditor.h"
#include "CustomLookAndFeel.h"
#include "../PluginProcessor.h"

EnvelopeEditor::EnvelopeEditor(SideDuckerAudioProcessor& processor)
    : processorRef(processor),
      envelopeRef(processor.getDuckingProcessor().getCustomEnvelope())
{
    // Rate Selector
    rateLabel.setText("RATE", juce::dontSendNotification);
    rateLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    rateLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(rateLabel);

    rateComboBox.addItemList({ "1/2", "1/4", "1/8", "1/16", "1/32", "1/2T", "1/4T", "1/8T", "1/16T", "1/32T", "1/2D", "1/4D", "1/8D", "1/16D" }, 1);
    addAndMakeVisible(rateComboBox);
    rateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef.getAPVTS(), "env_rate", rateComboBox
    );

    // Snap to Grid
    snapLabel.setText("SNAP", juce::dontSendNotification);
    snapLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    snapLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(snapLabel);

    snapComboBox.addItemList({ "Off", "1/4", "1/8", "1/16", "1/32" }, 1);
    snapComboBox.setSelectedId(4, juce::dontSendNotification); // Default 1/16 snap
    addAndMakeVisible(snapComboBox);

    // Presets
    presetLabel.setText("PRESET", juce::dontSendNotification);
    presetLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    presetLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(presetLabel);

    presetComboBox.addItemList({ "Classic Pump", "Surgical Kick Dip", "1/16th Gate", "Saw Ramp", "Smooth Swell" }, 1);
    presetComboBox.setText("Presets", juce::dontSendNotification);
    presetComboBox.onChange = [this]() {
        int id = presetComboBox.getSelectedId();
        switch (id)
        {
            case 1: envelopeRef.loadPresetClassicPump(); break;
            case 2: envelopeRef.loadPresetKickDip(); break;
            case 3: envelopeRef.loadPresetGate16th(); break;
            case 4: envelopeRef.loadPresetSawRamp(); break;
            case 5: envelopeRef.loadPresetSmoothSwell(); break;
        }
        repaint();
    };
    addAndMakeVisible(presetComboBox);

    // Clear Button
    clearButton.setButtonText("RESET");
    clearButton.onClick = [this]() {
        envelopeRef.loadPresetClassicPump();
        repaint();
    };
    addAndMakeVisible(clearButton);

    startTimerHz(60);
}

EnvelopeEditor::~EnvelopeEditor()
{
    stopTimer();
}

void EnvelopeEditor::timerCallback()
{
    repaint();
}

void EnvelopeEditor::resized()
{
    auto area = getLocalBounds();
    auto topBar = area.removeFromTop(32).reduced(8, 2);

    int startX = 6;
    rateLabel.setBounds(startX, topBar.getY() + 4, 38, 20);
    rateComboBox.setBounds(startX + 40, topBar.getY(), 75, 24);

    int snapX = startX + 130;
    snapLabel.setBounds(snapX, topBar.getY() + 4, 38, 20);
    snapComboBox.setBounds(snapX + 40, topBar.getY(), 75, 24);

    int presetX = snapX + 135;
    presetLabel.setBounds(presetX, topBar.getY() + 4, 52, 20);
    presetComboBox.setBounds(presetX + 54, topBar.getY(), 130, 24);

    clearButton.setBounds(getWidth() - 85, topBar.getY(), 75, 24);
}

juce::Point<float> EnvelopeEditor::pointToScreen(const CustomEnvelope::Point& p, const juce::Rectangle<float>& area) const
{
    float x = area.getX() + p.x * area.getWidth();
    float y = area.getY() + (1.0f - p.y) * area.getHeight();
    return { x, y };
}

CustomEnvelope::Point EnvelopeEditor::screenToPoint(juce::Point<float> pos, const juce::Rectangle<float>& area) const
{
    float x = juce::jlimit(0.0f, 1.0f, (pos.x - area.getX()) / area.getWidth());
    float y = juce::jlimit(0.0f, 1.0f, 1.0f - (pos.y - area.getY()) / area.getHeight());
    return { x, y, 0.0f };
}

float EnvelopeEditor::applySnap(float x) const
{
    int snapId = snapComboBox.getSelectedId();
    int divisions = 0;
    switch (snapId)
    {
        case 2: divisions = 4; break;  // 1/4
        case 3: divisions = 8; break;  // 1/8
        case 4: divisions = 16; break; // 1/16
        case 5: divisions = 32; break; // 1/32
    }

    if (divisions > 0)
    {
        float gridStep = 1.0f / static_cast<float>(divisions);
        return std::round(x / gridStep) * gridStep;
    }
    return x;
}

int EnvelopeEditor::findPointAtPosition(juce::Point<float> pos, const juce::Rectangle<float>& area) const
{
    int numPts = envelopeRef.getNumPoints();
    for (int i = 0; i < numPts; ++i)
    {
        auto pt = envelopeRef.getPoint(i);
        auto screenPt = pointToScreen(pt, area);
        if (pos.getDistanceFrom(screenPt) <= 12.0f)
            return i;
    }
    return -1;
}

void EnvelopeEditor::mouseDown(const juce::MouseEvent& e)
{
    auto canvasArea = getLocalBounds().toFloat().reduced(12.0f);
    canvasArea.removeFromTop(30.0f);
    canvasArea.removeFromBottom(16.0f);

    if (e.mods.isRightButtonDown())
    {
        int hitIndex = findPointAtPosition(e.position, canvasArea);
        if (hitIndex > 0 && hitIndex < envelopeRef.getNumPoints() - 1)
        {
            juce::PopupMenu m;
            m.addItem(1, "Delete Point");
            m.addItem(2, "Set to 100% (Full Volume)");
            m.addItem(3, "Set to 50% Volume");
            m.addItem(4, "Set to 0% (Ducked / Silence)");
            m.addItem(5, "Curve Tension (+0.5)");
            m.addItem(6, "Curve Tension (-0.5)");
            m.addItem(7, "Reset Tension (Linear)");

            m.showMenuAsync(juce::PopupMenu::Options(), [this, hitIndex](int result) {
                if (result == 1) envelopeRef.removePoint(hitIndex);
                else if (result == 2) { auto p = envelopeRef.getPoint(hitIndex); envelopeRef.updatePoint(hitIndex, p.x, 1.0f, p.tension); }
                else if (result == 3) { auto p = envelopeRef.getPoint(hitIndex); envelopeRef.updatePoint(hitIndex, p.x, 0.5f, p.tension); }
                else if (result == 4) { auto p = envelopeRef.getPoint(hitIndex); envelopeRef.updatePoint(hitIndex, p.x, 0.0f, p.tension); }
                else if (result == 5) { auto p = envelopeRef.getPoint(hitIndex); envelopeRef.updatePoint(hitIndex, p.x, p.y, 0.5f); }
                else if (result == 6) { auto p = envelopeRef.getPoint(hitIndex); envelopeRef.updatePoint(hitIndex, p.x, p.y, -0.5f); }
                else if (result == 7) { auto p = envelopeRef.getPoint(hitIndex); envelopeRef.updatePoint(hitIndex, p.x, p.y, 0.0f); }
                repaint();
            });
            return;
        }
    }

    int hit = findPointAtPosition(e.position, canvasArea);
    if (hit >= 0)
    {
        draggedPointIndex = hit;
    }
    else if (canvasArea.contains(e.position))
    {
        // Add new point on click
        auto pt = screenToPoint(e.position, canvasArea);
        pt.x = applySnap(pt.x);
        if (envelopeRef.addPoint(pt.x, pt.y, 0.0f))
        {
            draggedPointIndex = findPointAtPosition(e.position, canvasArea);
        }
    }
    repaint();
}

void EnvelopeEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedPointIndex >= 0)
    {
        auto canvasArea = getLocalBounds().toFloat().reduced(12.0f);
        canvasArea.removeFromTop(30.0f);
        canvasArea.removeFromBottom(16.0f);

        auto pt = screenToPoint(e.position, canvasArea);
        float newX = e.mods.isShiftDown() ? pt.x : applySnap(pt.x);
        float newY = pt.y;

        auto currentPt = envelopeRef.getPoint(draggedPointIndex);
        envelopeRef.updatePoint(draggedPointIndex, newX, newY, currentPt.tension);
        repaint();
    }
}

void EnvelopeEditor::mouseUp(const juce::MouseEvent&)
{
    draggedPointIndex = -1;
    repaint();
}

void EnvelopeEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    auto canvasArea = getLocalBounds().toFloat().reduced(12.0f);
    canvasArea.removeFromTop(30.0f);
    canvasArea.removeFromBottom(16.0f);

    int hit = findPointAtPosition(e.position, canvasArea);
    if (hit > 0 && hit < envelopeRef.getNumPoints() - 1)
    {
        envelopeRef.removePoint(hit);
        repaint();
    }
    else if (canvasArea.contains(e.position))
    {
        auto pt = screenToPoint(e.position, canvasArea);
        envelopeRef.addPoint(applySnap(pt.x), pt.y, 0.0f);
        repaint();
    }
}

void EnvelopeEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto canvasArea = getLocalBounds().toFloat().reduced(12.0f);
    canvasArea.removeFromTop(30.0f);
    canvasArea.removeFromBottom(16.0f);

    int hit = findPointAtPosition(e.position, canvasArea);
    if (hit >= 0)
    {
        auto pt = envelopeRef.getPoint(hit);
        float newTension = juce::jlimit(-1.0f, 1.0f, pt.tension + wheel.deltaY * 0.25f);
        envelopeRef.updatePoint(hit, pt.x, pt.y, newTension);
        repaint();
    }
}

void EnvelopeEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. M3 Dark Canvas Background
    g.setColour(CustomLookAndFeel::m3SurfaceContainerLowest);
    g.fillRoundedRectangle(bounds, 16.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bounds, 16.0f, 1.0f);

    auto canvasArea = bounds.reduced(12.0f);
    canvasArea.removeFromTop(32.0f);
    canvasArea.removeFromBottom(16.0f);

    float startX = canvasArea.getX();
    float startY = canvasArea.getY();
    float width = canvasArea.getWidth();
    float height = canvasArea.getHeight();

    // 2. Beat Grid Lines (1/4, 1/8, 1/16, 1/32)
    int gridDivs = 16;
    for (int i = 0; i <= gridDivs; ++i)
    {
        float normX = static_cast<float>(i) / static_cast<float>(gridDivs);
        float x = startX + normX * width;

        if (i % 4 == 0) // 1/4 bar line (bold)
        {
            g.setColour(CustomLookAndFeel::m3Outline.withAlpha(0.5f));
            g.drawVerticalLine(static_cast<int>(x), startY, startY + height);

            // Beat number label
            g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.6f));
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            int beatNum = (i / 4) + 1;
            g.drawText(juce::String(beatNum), static_cast<int>(x - 10.0f), static_cast<int>(startY + height + 2.0f), 20, 12, juce::Justification::centred);
        }
        else if (i % 2 == 0) // 1/8 line (medium)
        {
            g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.35f));
            g.drawVerticalLine(static_cast<int>(x), startY, startY + height);
        }
        else // 1/16 line (fine)
        {
            g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.18f));
            g.drawVerticalLine(static_cast<int>(x), startY, startY + height);
        }
    }

    // Horizontal Lines: 0%, 25%, 50%, 75%, 100% Ducking
    for (int j = 0; j <= 4; ++j)
    {
        float normY = static_cast<float>(j) / 4.0f;
        float y = startY + normY * height;

        g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(j == 4 || j == 0 ? 0.35f : 0.18f));
        g.drawHorizontalLine(static_cast<int>(y), startX, startX + width);

        // Value text
        g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.5f));
        g.setFont(juce::Font(9.0f, juce::Font::plain));
        int pct = (4 - j) * 25;
        g.drawText(juce::String(pct) + "%", static_cast<int>(startX + width - 32.0f), static_cast<int>(y - 10.0f), 30, 10, juce::Justification::right);
    }

    // 3. Render Continuous Curve & Filled Area
    juce::Path curvePath;
    const int numCurveSamples = 300;

    for (int s = 0; s < numCurveSamples; ++s)
    {
        float phase = static_cast<float>(s) / static_cast<float>(numCurveSamples - 1);
        float val = envelopeRef.evaluateAt(phase);
        float x = startX + phase * width;
        float y = startY + (1.0f - val) * height;

        if (s == 0) curvePath.startNewSubPath(x, y);
        else curvePath.lineTo(x, y);
    }

    // Fill under curve
    juce::Path fillPath = curvePath;
    fillPath.lineTo(startX + width, startY + height);
    fillPath.lineTo(startX, startY + height);
    fillPath.closeSubPath();

    juce::ColourGradient fillGrad(CustomLookAndFeel::m3Primary.withAlpha(0.28f), 0.0f, startY,
                                 CustomLookAndFeel::m3Primary.withAlpha(0.02f), 0.0f, startY + height, false);
    g.setGradientFill(fillGrad);
    g.fillPath(fillPath);

    // Glowing Curve Line
    g.setColour(CustomLookAndFeel::m3Primary);
    g.strokePath(curvePath, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 4. Render Interactive Nodes (up to 48 points)
    auto allPoints = envelopeRef.getAllPoints();
    for (size_t i = 0; i < allPoints.size(); ++i)
    {
        auto screenPos = pointToScreen(allPoints[i], canvasArea);
        bool isDragged = (static_cast<int>(i) == draggedPointIndex);

        if (isDragged)
        {
            g.setColour(CustomLookAndFeel::m3Primary.withAlpha(0.35f));
            g.fillEllipse(screenPos.x - 14.0f, screenPos.y - 14.0f, 28.0f, 28.0f);
        }

        // Outer Dark Ring
        g.setColour(CustomLookAndFeel::m3SurfaceContainerLowest);
        g.fillEllipse(screenPos.x - 8.0f, screenPos.y - 8.0f, 16.0f, 16.0f);

        // Core Glowing Node
        g.setColour(isDragged ? CustomLookAndFeel::m3Secondary : CustomLookAndFeel::m3Primary);
        g.fillEllipse(screenPos.x - 6.0f, screenPos.y - 6.0f, 12.0f, 12.0f);

        g.setColour(CustomLookAndFeel::m3OnSurface);
        g.drawEllipse(screenPos.x - 6.0f, screenPos.y - 6.0f, 12.0f, 12.0f, 1.0f);
    }

    // 5. Real-Time Glowing Playhead Laser Line
    float currentPhase = envelopeRef.getCurrentPhase();
    float playheadX = startX + currentPhase * width;

    g.setColour(CustomLookAndFeel::m3Tertiary.withAlpha(0.35f));
    g.drawLine(playheadX, startY, playheadX, startY + height, 3.5f);

    g.setColour(CustomLookAndFeel::m3Tertiary);
    g.drawLine(playheadX, startY, playheadX, startY + height, 1.5f);

    // Playhead glowing dot on curve
    float playheadVal = envelopeRef.evaluateAt(currentPhase);
    float playheadY = startY + (1.0f - playheadVal) * height;

    g.setColour(CustomLookAndFeel::m3Tertiary);
    g.fillEllipse(playheadX - 4.5f, playheadY - 4.5f, 9.0f, 9.0f);
}
