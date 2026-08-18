#include "DuckingVisualizer.h"
#include "CustomLookAndFeel.h"
#include "../PluginProcessor.h"

// Google Material Design 3 Tonal Node Palette
const std::array<juce::Colour, 6> DuckingVisualizer::bandColours = {
    juce::Colour::fromRGB(120, 217, 236), // Band 1: M3 Cyan (#78D9EC)
    juce::Colour::fromRGB(249, 214, 120), // Band 2: M3 Amber (#F9D678)
    juce::Colour::fromRGB(245, 162, 122), // Band 3: M3 Peach/Orange (#F5A27A)
    juce::Colour::fromRGB(244, 120, 136), // Band 4: M3 Crimson/Rose (#F47888)
    juce::Colour::fromRGB(197, 138, 249), // Band 5: M3 Lavender/Purple (#C58AF9)
    juce::Colour::fromRGB(248, 134, 184)  // Band 6: M3 Pink (#F886B8)
};

DuckingVisualizer::DuckingVisualizer(SideDuckerAudioProcessor& processor)
    : processorRef(processor),
      forwardFFT(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    spectrumIn.assign(numSpectrumBins, -80.0f);
    spectrumOut.assign(numSpectrumBins, -80.0f);
    spectrumSC.assign(numSpectrumBins, -80.0f);
    binFrequencies.resize(numSpectrumBins);

    smoothedReduction.fill(0.0f);

    const float minLog = std::log10(20.0f);
    const float maxLog = std::log10(20000.0f);
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float norm = static_cast<float>(i) / static_cast<float>(numSpectrumBins - 1);
        float logFreq = minLog + norm * (maxLog - minLog);
        binFrequencies[i] = std::pow(10.0f, logFreq);
    }

    readBuffer.resize(4096);
    startTimerHz(60);
}

DuckingVisualizer::~DuckingVisualizer()
{
    stopTimer();
}

void DuckingVisualizer::resized()
{
}

void DuckingVisualizer::setSelectedBandIndex(int index)
{
    if (index >= 0 && index < 6)
    {
        selectedBandIndex = index;
        repaint();
    }
}

void DuckingVisualizer::timerCallback()
{
    int numRead = processorRef.getVisualizerFifo().readAvailable(readBuffer.data(), static_cast<int>(readBuffer.size()));
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

    float decayRate = 0.88f;
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        spectrumIn[i] = spectrumIn[i] * decayRate + (-80.0f) * (1.0f - decayRate);
        spectrumOut[i] = spectrumOut[i] * decayRate + (-80.0f) * (1.0f - decayRate);
        spectrumSC[i] = spectrumSC[i] * decayRate + (-80.0f) * (1.0f - decayRate);
    }

    if (hadNewFft)
        computeSpectrumPoints();

    for (int b = 0; b < DuckingProcessor::NumBands; ++b)
    {
        float target = processorRef.getDuckingProcessor().getBandReductionDb(b);
        float smoothFactor = target < smoothedReduction[b] ? 0.7f : 0.25f;
        smoothedReduction[b] = smoothedReduction[b] + smoothFactor * (target - smoothedReduction[b]);
    }

    repaint();
}

void DuckingVisualizer::processNextFftBlock()
{
    std::fill(fftDataIn.begin(), fftDataIn.end(), 0.0f);
    std::fill(fftDataOut.begin(), fftDataOut.end(), 0.0f);
    std::fill(fftDataSC.begin(), fftDataSC.end(), 0.0f);

    std::copy(fifoBufferIn.begin(), fifoBufferIn.end(), fftDataIn.begin());
    std::copy(fifoBufferOut.begin(), fifoBufferOut.end(), fftDataOut.begin());
    std::copy(fifoBufferSC.begin(), fifoBufferSC.end(), fftDataSC.begin());

    window.multiplyWithWindowingTable(fftDataIn.data(), fftSize);
    window.multiplyWithWindowingTable(fftDataOut.data(), fftSize);
    window.multiplyWithWindowingTable(fftDataSC.data(), fftSize);

    forwardFFT.performFrequencyOnlyForwardTransform(fftDataIn.data());
    forwardFFT.performFrequencyOnlyForwardTransform(fftDataOut.data());
    forwardFFT.performFrequencyOnlyForwardTransform(fftDataSC.data());
}

float DuckingVisualizer::getMagnitudeForFrequency(const std::array<float, 2 * fftSize>& fftData, float frequency) const
{
    double sr = processorRef.getSampleRate() > 0 ? processorRef.getSampleRate() : 44100.0;
    float binIndexFloat = (frequency / static_cast<float>(sr)) * static_cast<float>(fftSize);
    int bin0 = juce::jlimit(0, fftSize / 2 - 1, static_cast<int>(binIndexFloat));
    int bin1 = juce::jmin(fftSize / 2 - 1, bin0 + 1);
    float frac = binIndexFloat - static_cast<float>(bin0);

    float mag0 = fftData[bin0];
    float mag1 = fftData[bin1];
    float mag = mag0 + frac * (mag1 - mag0);

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

float DuckingVisualizer::mapXToFrequency(float x, float startX, float width) const
{
    float norm = juce::jlimit(0.0f, 1.0f, (x - startX) / width);
    float minLog = std::log10(20.0f);
    float maxLog = std::log10(20000.0f);
    float logFreq = minLog + norm * (maxLog - minLog);
    return std::pow(10.0f, logFreq);
}

float DuckingVisualizer::mapDbToY(float db, float topY, float height) const
{
    const float maxDb = 18.0f;
    const float minDb = -24.0f;
    float norm = (juce::jlimit(minDb, maxDb, db) - minDb) / (maxDb - minDb);
    return topY + height - norm * height;
}

float DuckingVisualizer::mapYToDb(float y, float topY, float height) const
{
    const float maxDb = 18.0f;
    const float minDb = -24.0f;
    float norm = juce::jlimit(0.0f, 1.0f, (topY + height - y) / height);
    return minDb + norm * (maxDb - minDb);
}

juce::Point<float> DuckingVisualizer::getNodePosition(int bandIdx, float startX, float startY, float width, float height) const
{
    auto& apvts = processorRef.getAPVTS();
    float freq = 1000.0f;
    float gain = 0.0f;

    if (bandIdx == 0) // Band 1 Low Cut
    {
        if (auto* p = apvts.getRawParameterValue("b1_freq")) freq = p->load();
        gain = 0.0f;
    }
    else if (bandIdx == 5) // Band 6 High Cut
    {
        if (auto* p = apvts.getRawParameterValue("b6_freq")) freq = p->load();
        gain = 0.0f;
    }
    else // Bands 2-5
    {
        int bNum = bandIdx + 1;
        juce::String prefix = "b" + juce::String(bNum) + "_";
        if (auto* p = apvts.getRawParameterValue(prefix + "freq")) freq = p->load();
        if (auto* p = apvts.getRawParameterValue(prefix + "gain")) gain = p->load();
    }

    return { mapFrequencyToX(freq, startX, width), mapDbToY(gain, startY, height) };
}

int DuckingVisualizer::getBandNodeAtPosition(juce::Point<float> pos) const
{
    auto displayArea = getLocalBounds().toFloat().reduced(8.0f);
    float startX = displayArea.getX();
    float startY = displayArea.getY();
    float width = displayArea.getWidth();
    float height = displayArea.getHeight();

    for (int i = 0; i < 6; ++i)
    {
        auto nodePos = getNodePosition(i, startX, startY, width, height);
        if (pos.getDistanceFrom(nodePos) <= 14.0f)
            return i;
    }
    return -1;
}

void DuckingVisualizer::mouseDown(const juce::MouseEvent& e)
{
    int hitBand = getBandNodeAtPosition(e.position);
    if (hitBand >= 0)
    {
        selectedBandIndex = hitBand;
        draggingBandIndex = hitBand;
        if (onBandSelected)
            onBandSelected(selectedBandIndex);
        repaint();
    }
}

void DuckingVisualizer::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingBandIndex >= 0)
    {
        auto displayArea = getLocalBounds().toFloat().reduced(8.0f);
        float newFreq = mapXToFrequency(e.position.x, displayArea.getX(), displayArea.getWidth());
        float newGain = mapYToDb(e.position.y, displayArea.getY(), displayArea.getHeight());
        newGain = juce::jlimit(-18.0f, 18.0f, newGain);

        auto& apvts = processorRef.getAPVTS();

        if (draggingBandIndex == 0) // Low Cut
        {
            newFreq = juce::jlimit(20.0f, 1000.0f, newFreq);
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("b1_freq")))
                *p = newFreq;
        }
        else if (draggingBandIndex == 5) // High Cut
        {
            newFreq = juce::jlimit(500.0f, 20000.0f, newFreq);
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("b6_freq")))
                *p = newFreq;
        }
        else // Bands 2-5
        {
            int bNum = draggingBandIndex + 1;
            juce::String prefix = "b" + juce::String(bNum) + "_";
            newFreq = juce::jlimit(20.0f, 20000.0f, newFreq);

            if (auto* pF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "freq")))
                *pF = newFreq;
            if (auto* pG = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "gain")))
                *pG = newGain;
        }

        repaint();
    }
}

void DuckingVisualizer::mouseDoubleClick(const juce::MouseEvent& e)
{
    int hitBand = getBandNodeAtPosition(e.position);
    if (hitBand >= 1 && hitBand <= 4)
    {
        int bNum = hitBand + 1;
        juce::String prefix = "b" + juce::String(bNum) + "_";
        if (auto* pG = dynamic_cast<juce::AudioParameterFloat*>(processorRef.getAPVTS().getParameter(prefix + "gain")))
            *pG = 0.0f;
        repaint();
    }
}

void DuckingVisualizer::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    int band = getBandNodeAtPosition(e.position);
    if (band < 0)
        band = selectedBandIndex;

    if (band >= 1 && band <= 4)
    {
        int bNum = band + 1;
        juce::String prefix = "b" + juce::String(bNum) + "_";
        if (auto* pQ = dynamic_cast<juce::AudioParameterFloat*>(processorRef.getAPVTS().getParameter(prefix + "q")))
        {
            float curQ = pQ->get();
            float delta = wheel.deltaY > 0 ? 1.15f : 0.87f;
            *pQ = juce::jlimit(0.1f, 10.0f, curQ * delta);
            repaint();
        }
    }
    else if (band == 0 || band == 5)
    {
        juce::String paramId = (band == 0) ? "b1_slope" : "b6_slope";
        if (auto* pS = dynamic_cast<juce::AudioParameterChoice*>(processorRef.getAPVTS().getParameter(paramId)))
        {
            int curSlope = pS->getIndex();
            if (wheel.deltaY > 0 && curSlope < 3)
                *pS = curSlope + 1;
            else if (wheel.deltaY < 0 && curSlope > 0)
                *pS = curSlope - 1;
            repaint();
        }
    }
}

void DuckingVisualizer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. M3 Surface Canvas (Large 16px Rounded Container)
    g.setColour(CustomLookAndFeel::m3SurfaceContainerLowest);
    g.fillRoundedRectangle(bounds, 16.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bounds, 16.0f, 1.0f);

    auto displayArea = bounds.reduced(12.0f);
    float startX = displayArea.getX();
    float startY = displayArea.getY();
    float width = displayArea.getWidth();
    float height = displayArea.getHeight();

    // 2. Frequency Grid Lines & Labels (M3 Minimalist Typography)
    const std::vector<std::pair<float, juce::String>> freqMarkers = {
        { 50.0f, "50" }, { 100.0f, "100" }, { 250.0f, "250" }, { 500.0f, "500" },
        { 1000.0f, "1k" }, { 2500.0f, "2.5k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k" }
    };

    g.setFont(juce::Font(9.0f, juce::Font::plain));
    for (const auto& [freq, label] : freqMarkers)
    {
        float x = mapFrequencyToX(freq, startX, width);
        g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.45f));
        g.drawVerticalLine(static_cast<int>(x), startY, startY + height);

        g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.6f));
        g.drawText(label, static_cast<int>(x - 15.0f), static_cast<int>(startY + height - 12.0f), 30, 12, juce::Justification::centred);
    }

    // 3. dB Grid Lines (+18, +12, +6, 0 dB baseline, -6, -12, -18, -24 dB)
    const std::vector<float> dbMarkers = { 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f };
    float zeroY = mapDbToY(0.0f, startY, height);

    for (float db : dbMarkers)
    {
        float y = mapDbToY(db, startY, height);
        if (std::abs(db) < 0.01f)
        {
            // 0 dB Baseline (M3 Highlighted center line)
            g.setColour(CustomLookAndFeel::m3Outline.withAlpha(0.6f));
            g.drawHorizontalLine(static_cast<int>(y), startX, startX + width);
        }
        else
        {
            g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.3f));
            g.drawHorizontalLine(static_cast<int>(y), startX, startX + width);
        }

        g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.5f));
        juce::String dbText = (db > 0 ? "+" : "") + juce::String(static_cast<int>(db)) + "dB";
        g.drawText(dbText, static_cast<int>(startX + width - 36.0f), static_cast<int>(y - 10.0f), 34, 10, juce::Justification::right);
    }

    // 4. Ambient Background FFT Spectrum (Sidechain energy in subtle M3 Amber)
    juce::Path scFftPath;
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float x = mapFrequencyToX(binFrequencies[i], startX, width);
        float normFft = (spectrumSC[i] + 70.0f) / 70.0f;
        float y = startY + height - juce::jlimit(0.0f, 1.0f, normFft) * (height * 0.5f);

        if (i == 0) scFftPath.startNewSubPath(x, y);
        else scFftPath.lineTo(x, y);
    }
    scFftPath.lineTo(startX + width, startY + height);
    scFftPath.lineTo(startX, startY + height);
    scFftPath.closeSubPath();

    g.setColour(CustomLookAndFeel::m3Secondary.withAlpha(0.06f));
    g.fillPath(scFftPath);

    // 5. Soothe 2-Style Downward Dynamic Reduction Graph (M3 Coral/Pink #F48FB1)
    juce::Path reductionPath;
    reductionPath.startNewSubPath(startX, zeroY);

    for (int b = 0; b < DuckingProcessor::NumBands; ++b)
    {
        float freq = processorRef.getDuckingProcessor().getBandCenterFreq(b);
        float x = mapFrequencyToX(freq, startX, width);
        float redDb = smoothedReduction[b];
        float y = mapDbToY(redDb, startY, height);

        reductionPath.lineTo(x, y);
    }
    reductionPath.lineTo(startX + width, zeroY);
    reductionPath.closeSubPath();

    // M3 Tonal Downward Gradient Fill
    juce::ColourGradient redGrad(CustomLookAndFeel::m3Tertiary.withAlpha(0.38f), 0.0f, zeroY,
                                CustomLookAndFeel::m3Tertiary.withAlpha(0.03f), 0.0f, startY + height, false);
    g.setGradientFill(redGrad);
    g.fillPath(reductionPath);

    g.setColour(CustomLookAndFeel::m3Tertiary);
    g.strokePath(reductionPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved));

    // 6. Interactive 6-Band Sensitivity EQ Curve (M3 Primary Blue #A8C7FA)
    juce::Path sensCurvePath;
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float freq = binFrequencies[i];
        float x = mapFrequencyToX(freq, startX, width);
        float sensDb = processorRef.getDuckingProcessor().evaluateSensitivityDb(freq);
        float y = mapDbToY(sensDb, startY, height);

        if (i == 0) sensCurvePath.startNewSubPath(x, y);
        else sensCurvePath.lineTo(x, y);
    }

    g.setColour(CustomLookAndFeel::m3Primary);
    g.strokePath(sensCurvePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));

    // 7. Interactive Draggable M3 Nodes (Bands 1 to 6)
    for (int i = 0; i < 6; ++i)
    {
        auto nodePos = getNodePosition(i, startX, startY, width, height);
        juce::Colour col = bandColours[i];
        bool isSelected = (i == selectedBandIndex);

        // M3 Elevation / Selection Halo
        if (isSelected)
        {
            g.setColour(col.withAlpha(0.25f));
            g.fillEllipse(nodePos.x - 15.0f, nodePos.y - 15.0f, 30.0f, 30.0f);
            g.setColour(col.withAlpha(0.8f));
            g.drawEllipse(nodePos.x - 15.0f, nodePos.y - 15.0f, 30.0f, 30.0f, 1.5f);
        }

        // Outer Dark Border
        g.setColour(CustomLookAndFeel::m3SurfaceContainerLowest);
        g.fillEllipse(nodePos.x - 10.0f, nodePos.y - 10.0f, 20.0f, 20.0f);

        // M3 Tonal Node Body
        g.setColour(col);
        g.fillEllipse(nodePos.x - 8.0f, nodePos.y - 8.0f, 16.0f, 16.0f);

        // Node Number (M3 On-Color Text)
        g.setColour(CustomLookAndFeel::m3Surface);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(juce::String(i + 1), static_cast<int>(nodePos.x - 8.0f), static_cast<int>(nodePos.y - 8.0f), 16, 16, juce::Justification::centred);
    }

    // 8. Header Badges (M3 Clean Pill Badges)
    float badgeY = startY + 6.0f;
    float badgeX = startX + 8.0f;

    auto drawBadge = [&](const juce::String& text, juce::Colour col) {
        g.setColour(col);
        g.fillEllipse(badgeX, badgeY + 3.0f, 6.0f, 6.0f);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.setColour(CustomLookAndFeel::m3OnSurfaceVariant);
        g.drawText(text, static_cast<int>(badgeX + 9.0f), static_cast<int>(badgeY), 95, 12, juce::Justification::centredLeft);
        badgeX += 105.0f;
    };

    drawBadge("SENSITIVITY", CustomLookAndFeel::m3Primary);
    drawBadge("REDUCTION", CustomLookAndFeel::m3Tertiary);
    drawBadge("SIDECHAIN", CustomLookAndFeel::m3Secondary);
}
