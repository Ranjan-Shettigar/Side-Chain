#include "DuckingVisualizer.h"
#include "CustomLookAndFeel.h"

DuckingVisualizer::DuckingVisualizer(AudioVisualizerFifo& fifo)
    : audioFifo(fifo),
      forwardFFT(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    spectrumIn.assign(numSpectrumBins, -80.0f);
    spectrumOut.assign(numSpectrumBins, -80.0f);
    spectrumSC.assign(numSpectrumBins, -80.0f);
    binFrequencies.resize(numSpectrumBins);

    // Precalculate log-spaced frequency points from 20 Hz to 20,000 Hz
    float minLog = std::log10(20.0f);
    float maxLog = std::log10(20000.0f);
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float norm = static_cast<float>(i) / static_cast<float>(numSpectrumBins - 1);
        float logFreq = minLog + norm * (maxLog - minLog);
        binFrequencies[i] = std::pow(10.0f, logFreq);
    }

    readBuffer.resize(4096);
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

void DuckingVisualizer::timerCallback()
{
    int numRead = audioFifo.readAvailable(readBuffer.data(), static_cast<int>(readBuffer.size()));
    bool hadNewFft = false;

    if (numRead > 0)
    {
        for (int i = 0; i < numRead; ++i)
        {
            fifoBufferIn[fifoIndex] = readBuffer[i].inSample;
            fifoBufferOut[fifoIndex] = readBuffer[i].outSample;
            fifoBufferSC[fifoIndex] = readBuffer[i].scSample;

            if (++fifoIndex >= fftSize)
            {
                fifoIndex = 0;
                processNextFftBlock();
                hadNewFft = true;
            }
        }
    }

    // Apply continuous smooth decay even during silence
    float decayRate = 0.88f;
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        spectrumIn[i] = spectrumIn[i] * decayRate + (-80.0f) * (1.0f - decayRate);
        spectrumOut[i] = spectrumOut[i] * decayRate + (-80.0f) * (1.0f - decayRate);
        spectrumSC[i] = spectrumSC[i] * decayRate + (-80.0f) * (1.0f - decayRate);
    }

    if (hadNewFft)
    {
        computeSpectrumPoints();
    }

    repaint();
}

void DuckingVisualizer::processNextFftBlock()
{
    // Copy into FFT working buffers (2 * fftSize)
    std::fill(fftDataIn.begin(), fftDataIn.end(), 0.0f);
    std::fill(fftDataOut.begin(), fftDataOut.end(), 0.0f);
    std::fill(fftDataSC.begin(), fftDataSC.end(), 0.0f);

    std::copy(fifoBufferIn.begin(), fifoBufferIn.end(), fftDataIn.begin());
    std::copy(fifoBufferOut.begin(), fifoBufferOut.end(), fftDataOut.begin());
    std::copy(fifoBufferSC.begin(), fifoBufferSC.end(), fftDataSC.begin());

    // Apply Hann window
    window.multiplyWithWindowingTable(fftDataIn.data(), fftSize);
    window.multiplyWithWindowingTable(fftDataOut.data(), fftSize);
    window.multiplyWithWindowingTable(fftDataSC.data(), fftSize);

    // Forward FFT
    forwardFFT.performFrequencyOnlyForwardTransform(fftDataIn.data());
    forwardFFT.performFrequencyOnlyForwardTransform(fftDataOut.data());
    forwardFFT.performFrequencyOnlyForwardTransform(fftDataSC.data());
}

float DuckingVisualizer::getMagnitudeForFrequency(const std::array<float, 2 * fftSize>& fftData, float frequency) const
{
    float binIndexFloat = (frequency / static_cast<float>(sampleRate)) * static_cast<float>(fftSize);
    int bin0 = juce::jlimit(0, fftSize / 2 - 1, static_cast<int>(binIndexFloat));
    int bin1 = juce::jmin(fftSize / 2 - 1, bin0 + 1);
    float frac = binIndexFloat - static_cast<float>(bin0);

    float mag0 = fftData[bin0];
    float mag1 = fftData[bin1];
    float mag = mag0 + frac * (mag1 - mag0);

    // Normalize FFT amplitude
    float normalizedMag = mag / static_cast<float>(fftSize * 0.5f);
    float db = juce::Decibels::gainToDecibels(juce::jmax(1e-5f, normalizedMag), -80.0f);
    return juce::jlimit(-80.0f, 12.0f, db);
}

void DuckingVisualizer::computeSpectrumPoints()
{
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float freq = binFrequencies[i];

        float targetIn = getMagnitudeForFrequency(fftDataIn, freq);
        float targetOut = getMagnitudeForFrequency(fftDataOut, freq);
        float targetSC = getMagnitudeForFrequency(fftDataSC, freq);

        // Smooth attack / decay
        auto smoothDb = [](float current, float target) {
            float alpha = target > current ? 0.65f : 0.25f;
            return current + alpha * (target - current);
        };

        spectrumIn[i] = smoothDb(spectrumIn[i], targetIn);
        spectrumOut[i] = smoothDb(spectrumOut[i], targetOut);
        spectrumSC[i] = smoothDb(spectrumSC[i], targetSC);
    }
}

float DuckingVisualizer::mapFrequencyToX(float freq, float startX, float width) const
{
    float minLog = std::log10(20.0f);
    float maxLog = std::log10(20000.0f);
    float norm = (std::log10(juce::jlimit(20.0f, 20000.0f, freq)) - minLog) / (maxLog - minLog);
    return startX + norm * width;
}

float DuckingVisualizer::mapDbToY(float db, float topY, float height) const
{
    const float minDb = -72.0f;
    const float maxDb = 6.0f;
    float norm = (juce::jlimit(minDb, maxDb, db) - minDb) / (maxDb - minDb);
    return topY + height - norm * height;
}

void DuckingVisualizer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. Background Card & Frame
    g.setColour(CustomLookAndFeel::bgCardInner);
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(CustomLookAndFeel::borderSubtle);
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    auto displayArea = bounds.reduced(8.0f);
    float startX = displayArea.getX();
    float startY = displayArea.getY();
    float width = displayArea.getWidth();
    float height = displayArea.getHeight();

    // 2. Frequency Grid Lines & Labels (Log scale)
    const std::vector<std::pair<float, juce::String>> freqMarkers = {
        { 50.0f, "50" }, { 100.0f, "100" }, { 250.0f, "250" }, { 500.0f, "500" },
        { 1000.0f, "1k" }, { 2500.0f, "2.5k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k" }
    };

    g.setFont(juce::Font(9.0f, juce::Font::plain));
    for (const auto& [freq, label] : freqMarkers)
    {
        float x = mapFrequencyToX(freq, startX, width);
        g.setColour(juce::Colour::fromRGB(28, 35, 46));
        g.drawVerticalLine(static_cast<int>(x), startY, startY + height);

        g.setColour(juce::Colour::fromRGB(90, 100, 115));
        g.drawText(label, static_cast<int>(x - 15.0f), static_cast<int>(startY + height - 14.0f), 30, 12, juce::Justification::centred);
    }

    // 3. dB Grid Lines & Labels
    const std::vector<float> dbMarkers = { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f };
    for (float db : dbMarkers)
    {
        float y = mapDbToY(db, startY, height);
        g.setColour(juce::Colour::fromRGB(24, 30, 40));
        g.drawHorizontalLine(static_cast<int>(y), startX, startX + width);

        g.setColour(juce::Colour::fromRGB(75, 85, 98));
        g.drawText(juce::String(static_cast<int>(db)) + "dB",
                   static_cast<int>(startX + width - 36.0f), static_cast<int>(y - 11.0f), 32, 10, juce::Justification::right);
    }

    // 4. Sidechain Frequency Curtains (Low Cut & High Cut active band)
    float lowCutX = mapFrequencyToX(lowCutFreq, startX, width);
    float highCutX = mapFrequencyToX(highCutFreq, startX, width);

    if (lowCutX > startX)
    {
        g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.09f));
        g.fillRect(startX, startY, lowCutX - startX, height);

        g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.5f));
        g.drawVerticalLine(static_cast<int>(lowCutX), startY, startY + height);
    }

    if (highCutX < startX + width)
    {
        g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.09f));
        g.fillRect(highCutX, startY, (startX + width) - highCutX, height);

        g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.5f));
        g.drawVerticalLine(static_cast<int>(highCutX), startY, startY + height);
    }

    // 5. Build Spectrum Paths
    juce::Path inPath, outPath, scPath;
    std::vector<juce::Point<float>> inPoints;
    std::vector<juce::Point<float>> outPoints;

    float bottomY = startY + height;

    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float freq = binFrequencies[i];
        float x = mapFrequencyToX(freq, startX, width);

        float yIn = mapDbToY(spectrumIn[i], startY, height);
        float yOut = mapDbToY(spectrumOut[i], startY, height);
        float ySC = mapDbToY(spectrumSC[i], startY, height);

        inPoints.push_back({ x, yIn });
        outPoints.push_back({ x, yOut });

        if (i == 0)
        {
            inPath.startNewSubPath(x, yIn);
            outPath.startNewSubPath(x, yOut);
            scPath.startNewSubPath(x, ySC);
        }
        else
        {
            inPath.lineTo(x, yIn);
            outPath.lineTo(x, yOut);
            scPath.lineTo(x, ySC);
        }
    }

    // 6. Shaded Ducking Delta Zone (Magenta carve-out between Input and Ducked Output Spectrum)
    if (!inPoints.empty() && !outPoints.empty())
    {
        juce::Path duckingShadePath;
        duckingShadePath.startNewSubPath(inPoints[0]);
        for (size_t i = 1; i < inPoints.size(); ++i)
            duckingShadePath.lineTo(inPoints[i]);
        for (int i = static_cast<int>(outPoints.size()) - 1; i >= 0; --i)
            duckingShadePath.lineTo(outPoints[i]);
        duckingShadePath.closeSubPath();

        g.setColour(CustomLookAndFeel::accentMagenta.withAlpha(0.35f));
        g.fillPath(duckingShadePath);
    }

    // 7. Render Input Spectrum Silhouette (Ghost background)
    juce::Path inFillPath = inPath;
    inFillPath.lineTo(startX + width, bottomY);
    inFillPath.lineTo(startX, bottomY);
    inFillPath.closeSubPath();

    g.setColour(juce::Colour::fromRGB(70, 95, 125).withAlpha(0.12f));
    g.fillPath(inFillPath);

    g.setColour(juce::Colour::fromRGB(85, 110, 140).withAlpha(0.6f));
    g.strokePath(inPath, juce::PathStrokeType(1.2f));

    // 8. Render Sidechain Frequency Spectrum (Amber Glow)
    juce::Path scFillPath = scPath;
    scFillPath.lineTo(startX + width, bottomY);
    scFillPath.lineTo(startX, bottomY);
    scFillPath.closeSubPath();

    g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.08f));
    g.fillPath(scFillPath);

    g.setColour(CustomLookAndFeel::accentAmber.withAlpha(0.85f));
    g.strokePath(scPath, juce::PathStrokeType(1.5f));

    // 9. Render Ducked Output Frequency Spectrum (Bright Cyan with Bloom)
    juce::Path outFillPath = outPath;
    outFillPath.lineTo(startX + width, bottomY);
    outFillPath.lineTo(startX, bottomY);
    outFillPath.closeSubPath();

    juce::ColourGradient outGrad(CustomLookAndFeel::accentCyan.withAlpha(0.2f), 0.0f, startY + 20.0f,
                                juce::Colours::transparentBlack, 0.0f, bottomY, false);
    g.setGradientFill(outGrad);
    g.fillPath(outFillPath);

    // Cyan Bloom outer stroke
    g.setColour(CustomLookAndFeel::accentCyan.withAlpha(0.3f));
    g.strokePath(outPath, juce::PathStrokeType(3.5f));

    // Cyan Core stroke
    g.setColour(CustomLookAndFeel::accentCyan);
    g.strokePath(outPath, juce::PathStrokeType(1.8f));

    // 10. Top Header Legend Badges
    float badgeY = startY + 6.0f;
    float badgeX = startX + 8.0f;

    auto drawBadge = [&](const juce::String& text, juce::Colour col) {
        g.setColour(col);
        g.fillEllipse(badgeX, badgeY + 3.0f, 6.0f, 6.0f);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.setColour(CustomLookAndFeel::textSecondary);
        g.drawText(text, static_cast<int>(badgeX + 9.0f), static_cast<int>(badgeY), 65, 12, juce::Justification::centredLeft);
        badgeX += 75.0f;
    };

    drawBadge("OUTPUT", CustomLookAndFeel::accentCyan);
    drawBadge("DUCKING", CustomLookAndFeel::accentMagenta);
    drawBadge("SIDECHAIN", CustomLookAndFeel::accentAmber);
    drawBadge("INPUT", juce::Colour::fromRGB(110, 130, 155));
}
