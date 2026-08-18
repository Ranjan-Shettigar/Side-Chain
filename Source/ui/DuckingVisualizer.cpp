#include "DuckingVisualizer.h"
#include "CustomLookAndFeel.h"

DuckingVisualizer::DuckingVisualizer(AudioVisualizerFifo& fifo)
    : audioFifo(fifo)
{
    history.resize(NumHistoryPoints);
    readBuffer.resize(1024);

    startTimerHz(60); // 60 FPS update rate
}

DuckingVisualizer::~DuckingVisualizer()
{
    stopTimer();
}

void DuckingVisualizer::resized()
{
}

void DuckingVisualizer::setFilterFrequencies(float lowCutHz, float highCutHz)
{
    lowCutFreq = lowCutHz;
    highCutFreq = highCutHz;
}

void DuckingVisualizer::pushPoint(float in, float out, float sc, float gr)
{
    history[writeIndex] = { in, out, sc, gr };
    writeIndex = (writeIndex + 1) % NumHistoryPoints;
}

void DuckingVisualizer::timerCallback()
{
    int numRead = audioFifo.readAvailable(readBuffer.data(), static_cast<int>(readBuffer.size()));

    if (numRead > 0)
    {
        // Aggregate samples into history buffer
        for (int i = 0; i < numRead; ++i)
        {
            const auto& s = readBuffer[i];
            pushPoint(std::abs(s.inputSample), std::abs(s.outputSample), std::abs(s.sidechainSample), s.gainReduction);
        }
        repaint();
    }
}

void DuckingVisualizer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Background Card
    g.setColour(CustomLookAndFeel::bgCardInner);
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(CustomLookAndFeel::borderSubtle);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    auto displayArea = bounds.reduced(8.0f);
    float width = displayArea.getWidth();
    float height = displayArea.getHeight();
    float centerY = displayArea.getCentreY();
    float halfHeight = height * 0.46f;

    // 2. Subtle Grid Lines
    g.setColour(juce::Colour::fromRGB(25, 32, 42));
    g.drawHorizontalLine(static_cast<int>(centerY), displayArea.getX(), displayArea.getRight());
    g.drawHorizontalLine(static_cast<int>(centerY - halfHeight * 0.5f), displayArea.getX(), displayArea.getRight());
    g.drawHorizontalLine(static_cast<int>(centerY + halfHeight * 0.5f), displayArea.getX(), displayArea.getRight());

    // 3. Sidechain Frequency Curtains (Low Cut & High Cut visual indicators)
    float minLog = std::log10(20.0f);
    float maxLog = std::log10(20000.0f);
    float lowCutNorm = juce::jlimit(0.0f, 1.0f, (std::log10(juce::jmax(20.0f, lowCutFreq)) - minLog) / (maxLog - minLog));
    float highCutNorm = juce::jlimit(0.0f, 1.0f, (std::log10(juce::jmax(20.0f, highCutFreq)) - minLog) / (maxLog - minLog));

    float lowCutX = displayArea.getX() + lowCutNorm * width;
    float highCutX = displayArea.getX() + highCutNorm * width;

    // Filter curtain shading
    if (lowCutX > displayArea.getX())
    {
        g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.08f));
        g.fillRect(displayArea.getX(), displayArea.getY(), lowCutX - displayArea.getX(), height);

        g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.4f));
        g.drawVerticalLine(static_cast<int>(lowCutX), displayArea.getY(), displayArea.getBottom());
    }

    if (highCutX < displayArea.getRight())
    {
        g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.08f));
        g.fillRect(highCutX, displayArea.getY(), displayArea.getRight() - highCutX, height);

        g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.4f));
        g.drawVerticalLine(static_cast<int>(highCutX), displayArea.getY(), displayArea.getBottom());
    }

    // 4. Build Waveform Paths
    juce::Path inPathTop, inPathBottom;
    juce::Path outPathTop, outPathBottom;
    juce::Path duckZonePath;
    juce::Path scPath;

    int numPoints = NumHistoryPoints;
    float stepX = width / static_cast<float>(numPoints - 1);

    bool started = false;

    // Vectors to hold points for closed polygon shading
    std::vector<juce::Point<float>> inPointsTop, inPointsBottom;
    std::vector<juce::Point<float>> outPointsTop, outPointsBottom;

    for (int i = 0; i < numPoints; ++i)
    {
        int histIdx = (writeIndex + i) % numPoints;
        const auto& pt = history[histIdx];

        float x = displayArea.getX() + i * stepX;

        // Apply gentle visual scaling and compression curve
        float inAmp = juce::jlimit(0.0f, 1.2f, pt.inLevel) * halfHeight;
        float outAmp = juce::jlimit(0.0f, 1.2f, pt.outLevel) * halfHeight;
        float scAmp = juce::jlimit(0.0f, 1.2f, pt.scLevel) * halfHeight * 0.8f;

        inPointsTop.push_back({ x, centerY - inAmp });
        inPointsBottom.push_back({ x, centerY + inAmp });

        outPointsTop.push_back({ x, centerY - outAmp });
        outPointsBottom.push_back({ x, centerY + outAmp });

        if (!started)
        {
            inPathTop.startNewSubPath(x, centerY - inAmp);
            inPathBottom.startNewSubPath(x, centerY + inAmp);

            outPathTop.startNewSubPath(x, centerY - outAmp);
            outPathBottom.startNewSubPath(x, centerY + outAmp);

            scPath.startNewSubPath(x, centerY - scAmp);
            started = true;
        }
        else
        {
            inPathTop.lineTo(x, centerY - inAmp);
            inPathBottom.lineTo(x, centerY + inAmp);

            outPathTop.lineTo(x, centerY - outAmp);
            outPathBottom.lineTo(x, centerY + outAmp);

            scPath.lineTo(x, centerY - scAmp);
        }
    }

    // 5. Render Ducking Cutout Area (Magenta shaded zone between Input and Output)
    if (!inPointsTop.empty() && !outPointsTop.empty())
    {
        // Top ducking zone
        juce::Path duckZoneTop;
        duckZoneTop.startNewSubPath(inPointsTop[0]);
        for (size_t i = 1; i < inPointsTop.size(); ++i)
            duckZoneTop.lineTo(inPointsTop[i]);
        for (int i = static_cast<int>(outPointsTop.size()) - 1; i >= 0; --i)
            duckZoneTop.lineTo(outPointsTop[i]);
        duckZoneTop.closeSubPath();

        // Bottom ducking zone
        juce::Path duckZoneBottom;
        duckZoneBottom.startNewSubPath(outPointsBottom[0]);
        for (size_t i = 1; i < outPointsBottom.size(); ++i)
            duckZoneBottom.lineTo(outPointsBottom[i]);
        for (int i = static_cast<int>(inPointsBottom.size()) - 1; i >= 0; --i)
            duckZoneBottom.lineTo(inPointsBottom[i]);
        duckZoneBottom.closeSubPath();

        g.setColour(CustomLookAndFeel::accentMagenta.withAlpha(0.35f));
        g.fillPath(duckZoneTop);
        g.fillPath(duckZoneBottom);
    }

    // 6. Draw Un-ducked Input Waveform (Ghost outline)
    g.setColour(juce::Colour::fromRGB(70, 90, 115).withAlpha(0.5f));
    g.strokePath(inPathTop, juce::PathStrokeType(1.0f));
    g.strokePath(inPathBottom, juce::PathStrokeType(1.0f));

    // 7. Draw External Sidechain Pulse Waveform (Amber)
    g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.4f));
    g.strokePath(scPath, juce::PathStrokeType(1.2f));

    // 8. Draw Ducked Output Waveform (Crisp Cyan with Bloom)
    g.setColour(CustomLookAndFeel::accentCyan.withAlpha(0.25f));
    g.strokePath(outPathTop, juce::PathStrokeType(3.5f));
    g.strokePath(outPathBottom, juce::PathStrokeType(3.5f));

    g.setColour(CustomLookAndFeel::accentCyan);
    g.strokePath(outPathTop, juce::PathStrokeType(1.5f));
    g.strokePath(outPathBottom, juce::PathStrokeType(1.5f));

    // 9. Legend Badges
    float badgeY = displayArea.getY() + 6.0f;
    float badgeX = displayArea.getX() + 8.0f;

    auto drawBadge = [&](const juce::String& text, juce::Colour col) {
        g.setColour(col);
        g.fillEllipse(badgeX, badgeY + 3.0f, 6.0f, 6.0f);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.setColour(CustomLookAndFeel::textSecondary);
        g.drawText(text, static_cast<int>(badgeX + 9.0f), static_cast<int>(badgeY), 65, 12, juce::Justification::centredLeft);
        badgeX += 75.0f;
    };

    drawBadge("OUTPUT", CustomLookAndFeel::accentCyan);
    drawBadge("DUCKED", CustomLookAndFeel::accentMagenta);
    drawBadge("SIDECHAIN", CustomLookAndFeel::accentAmber);
    drawBadge("INPUT", juce::Colour::fromRGB(110, 130, 155));
}
