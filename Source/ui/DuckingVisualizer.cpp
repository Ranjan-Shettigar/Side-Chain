#include "DuckingVisualizer.h"
#include "CustomLookAndFeel.h"
#include "../PluginProcessor.h"

// Google Material Design 3 / Pro-Q Tonal Node Palette
const std::array<juce::Colour, 6> DuckingVisualizer::bandColours = {
    juce::Colour::fromRGB(120, 217, 236), // Band 1: Cyan (#78D9EC)
    juce::Colour::fromRGB(249, 214, 120), // Band 2: Amber (#F9D678)
    juce::Colour::fromRGB(245, 162, 122), // Band 3: Peach (#F5A27A)
    juce::Colour::fromRGB(244, 120, 136), // Band 4: Rose (#F47888)
    juce::Colour::fromRGB(197, 138, 249), // Band 5: Lavender (#C58AF9)
    juce::Colour::fromRGB(248, 134, 184)  // Band 6: Pink (#F886B8)
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

    float decayRate = 0.86f;
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        spectrumIn[i] = spectrumIn[i] * decayRate + (-80.0f) * (1.0f - decayRate);
        spectrumOut[i] = spectrumOut[i] * decayRate + (-80.0f) * (1.0f - decayRate);
        spectrumSC[i] = spectrumSC[i] * decayRate + (-80.0f) * (1.0f - decayRate);
    }

    if (hadNewFft)
        computeSpectrumPoints();

    int activeBands = processorRef.getDuckingProcessor().getActiveNumBands();
    for (int b = 0; b < activeBands; ++b)
    {
        float target = processorRef.getDuckingProcessor().getBandReductionDb(b);
        float smoothFactor = target < smoothedReduction[b] ? 0.75f : 0.30f;
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

    float normalizedMag = mag / static_cast<float>(fftSize * 0.45f);
    float db = juce::Decibels::gainToDecibels(juce::jmax(1e-5f, normalizedMag), -80.0f);
    return juce::jlimit(-80.0f, 12.0f, db);
}

void DuckingVisualizer::computeSpectrumPoints()
{
    std::vector<float> rawIn(numSpectrumBins, -80.0f);
    std::vector<float> rawOut(numSpectrumBins, -80.0f);
    std::vector<float> rawSC(numSpectrumBins, -80.0f);

    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float freq = binFrequencies[i];
        rawIn[i]  = getMagnitudeForFrequency(fftDataIn, freq);
        rawOut[i] = getMagnitudeForFrequency(fftDataOut, freq);
        rawSC[i]  = getMagnitudeForFrequency(fftDataSC, freq);
    }

    // 5-Point Gaussian Spectral Smoothing for ultra-fluid Pro-Q style curves
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float targetIn = 0.0f, targetOut = 0.0f, targetSC = 0.0f;
        float totalWeight = 0.0f;

        for (int k = -2; k <= 2; ++k)
        {
            int idx = juce::jlimit(0, numSpectrumBins - 1, i + k);
            float weight = (k == 0) ? 0.4f : (std::abs(k) == 1 ? 0.22f : 0.08f);
            targetIn  += rawIn[idx]  * weight;
            targetOut += rawOut[idx] * weight;
            targetSC  += rawSC[idx]  * weight;
            totalWeight += weight;
        }

        targetIn  /= totalWeight;
        targetOut /= totalWeight;
        targetSC  /= totalWeight;

        auto smoothDb = [](float current, float target) {
            float alpha = target > current ? 0.70f : 0.30f;
            return current + alpha * (target - current);
        };

        spectrumIn[i]  = smoothDb(spectrumIn[i], targetIn);
        spectrumOut[i] = smoothDb(spectrumOut[i], targetOut);
        spectrumSC[i]  = smoothDb(spectrumSC[i], targetSC);
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
    int bNum = bandIdx + 1;
    juce::String prefix = "b" + juce::String(bNum) + "_";

    float freq = 1000.0f;
    float gain = 0.0f;

    if (auto* pF = apvts.getRawParameterValue(prefix + "freq")) freq = pF->load();
    if (auto* pG = apvts.getRawParameterValue(prefix + "gain")) gain = pG->load();

    int shape = 0;
    if (auto* pS = apvts.getRawParameterValue(prefix + "shape")) shape = static_cast<int>(pS->load());

    // Cut bands (LowCut = 1, HighCut = 2) have node handle sitting at 0 dB line
    if (shape == 1 || shape == 2)
        gain = 0.0f;

    float posX = juce::jlimit(startX + 12.0f, startX + width - 12.0f, mapFrequencyToX(freq, startX, width));
    float posY = juce::jlimit(startY + 12.0f, startY + height - 12.0f, mapDbToY(gain, startY, height));

    return { posX, posY };
}

int DuckingVisualizer::getBandNodeAtPosition(juce::Point<float> pos) const
{
    auto displayArea = getLocalBounds().toFloat().reduced(12.0f);
    float startX = displayArea.getX();
    float startY = displayArea.getY();
    float width = displayArea.getWidth();
    float height = displayArea.getHeight() - 16.0f;

    for (int i = 0; i < 6; ++i)
    {
        auto nodePos = getNodePosition(i, startX, startY, width, height);
        if (pos.getDistanceFrom(nodePos) <= 15.0f)
            return i;
    }
    return -1;
}

void DuckingVisualizer::mouseMove(const juce::MouseEvent& e)
{
    int hit = getBandNodeAtPosition(e.position);
    if (hit != hoveredBandIndex)
    {
        hoveredBandIndex = hit;
        repaint();
    }
}

void DuckingVisualizer::mouseExit(const juce::MouseEvent&)
{
    if (hoveredBandIndex != -1)
    {
        hoveredBandIndex = -1;
        repaint();
    }
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
        auto displayArea = getLocalBounds().toFloat().reduced(12.0f);
        float width = displayArea.getWidth();
        float height = displayArea.getHeight() - 16.0f;

        float newFreq = mapXToFrequency(e.position.x, displayArea.getX(), width);
        float newGain = mapYToDb(e.position.y, displayArea.getY(), height);
        newGain = juce::jlimit(-18.0f, 18.0f, newGain);
        newFreq = juce::jlimit(20.0f, 20000.0f, newFreq);

        auto& apvts = processorRef.getAPVTS();
        int bNum = draggingBandIndex + 1;
        juce::String prefix = "b" + juce::String(bNum) + "_";

        if (auto* pF = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "freq")))
            *pF = newFreq;

        int shape = 0;
        if (auto* pS = apvts.getRawParameterValue(prefix + "shape")) shape = static_cast<int>(pS->load());

        if (shape != 1 && shape != 2) // Cut filters don't have gain
        {
            if (auto* pG = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "gain")))
                *pG = newGain;
        }

        repaint();
    }
}

void DuckingVisualizer::mouseUp(const juce::MouseEvent&)
{
    draggingBandIndex = -1;
    repaint();
}

void DuckingVisualizer::mouseDoubleClick(const juce::MouseEvent& e)
{
    int hitBand = getBandNodeAtPosition(e.position);
    if (hitBand >= 0)
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

    int bNum = band + 1;
    juce::String prefix = "b" + juce::String(bNum) + "_";
    auto& apvts = processorRef.getAPVTS();

    int shape = 0;
    if (auto* pS = apvts.getRawParameterValue(prefix + "shape")) shape = static_cast<int>(pS->load());

    if (shape == 1 || shape == 2) // Slope for Cut filters
    {
        if (auto* pSlope = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(prefix + "slope")))
        {
            int curSlope = pSlope->getIndex();
            if (wheel.deltaY > 0 && curSlope < 9)
                *pSlope = curSlope + 1;
            else if (wheel.deltaY < 0 && curSlope > 0)
                *pSlope = curSlope - 1;
            repaint();
        }
    }
    else // Q for parametric filters
    {
        if (auto* pQ = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "q")))
        {
            float curQ = pQ->get();
            float delta = wheel.deltaY > 0 ? 1.15f : 0.87f;
            *pQ = juce::jlimit(0.1f, 10.0f, curQ * delta);
            repaint();
        }
    }
}

void DuckingVisualizer::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // 1. M3 Surface Canvas
    g.setColour(CustomLookAndFeel::m3SurfaceContainerLowest);
    g.fillRoundedRectangle(bounds, 16.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bounds, 16.0f, 1.0f);

    auto displayArea = bounds.reduced(12.0f);
    float startX = displayArea.getX();
    float startY = displayArea.getY();
    float width = displayArea.getWidth();
    float height = displayArea.getHeight() - 16.0f;

    // 2. Frequency Grid Lines & Labels
    const std::vector<std::pair<float, juce::String>> freqMarkers = {
        { 50.0f, "50" }, { 100.0f, "100" }, { 250.0f, "250" }, { 500.0f, "500" },
        { 1000.0f, "1k" }, { 2500.0f, "2.5k" }, { 5000.0f, "5k" }, { 10000.0f, "10k" }, { 20000.0f, "20k" }
    };

    g.setFont(juce::Font(9.0f, juce::Font::plain));
    for (const auto& [freq, label] : freqMarkers)
    {
        float x = mapFrequencyToX(freq, startX, width);
        g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.35f));
        g.drawVerticalLine(static_cast<int>(x), startY, startY + height);

        g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.6f));
        int textX = (freq >= 20000.0f) ? static_cast<int>(x - 22.0f) : static_cast<int>(x - 14.0f);
        g.drawText(label, textX, static_cast<int>(startY + height + 2.0f), 28, 12, juce::Justification::centred);
    }

    // 3. dB Grid Lines (+18, +12, +6, 0 dB baseline, -6, -12, -18, -24 dB)
    const std::vector<float> dbMarkers = { 18.0f, 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f };
    float zeroY = mapDbToY(0.0f, startY, height);

    for (float db : dbMarkers)
    {
        float y = mapDbToY(db, startY, height);
        if (std::abs(db) < 0.01f)
        {
            g.setColour(CustomLookAndFeel::m3Outline.withAlpha(0.6f));
            g.drawHorizontalLine(static_cast<int>(y), startX, startX + width);
        }
        else
        {
            g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.25f));
            g.drawHorizontalLine(static_cast<int>(y), startX, startX + width);
        }

        g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.5f));
        juce::String dbText = (db > 0 ? "+" : "") + juce::String(static_cast<int>(db)) + "dB";
        g.drawText(dbText, static_cast<int>(startX + width - 36.0f), static_cast<int>(y - 10.0f), 34, 10, juce::Justification::right);
    }

    // 4. Main Audio Input Spectrum (Subtle Soft Blue)
    juce::Path inFftPath;
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float x = mapFrequencyToX(binFrequencies[i], startX, width);
        float normFft = (spectrumIn[i] + 70.0f) / 70.0f;
        float y = startY + height - juce::jlimit(0.0f, 1.0f, normFft) * (height * 0.72f);

        if (i == 0) inFftPath.startNewSubPath(x, y);
        else inFftPath.lineTo(x, y);
    }
    inFftPath.lineTo(startX + width, startY + height);
    inFftPath.lineTo(startX, startY + height);
    inFftPath.closeSubPath();

    g.setColour(CustomLookAndFeel::m3Primary.withAlpha(0.08f));
    g.fillPath(inFftPath);

    // 5. Luminous Sidechain Signal FFT Spectrum (Vibrant Mint Green #7CDB8A)
    juce::Path scFftPath;
    juce::Path scTopLine;
    for (int i = 0; i < numSpectrumBins; ++i)
    {
        float x = mapFrequencyToX(binFrequencies[i], startX, width);
        float normFft = (spectrumSC[i] + 70.0f) / 70.0f;
        float y = startY + height - juce::jlimit(0.0f, 1.0f, normFft) * (height * 0.80f);

        if (i == 0)
        {
            scFftPath.startNewSubPath(x, y);
            scTopLine.startNewSubPath(x, y);
        }
        else
        {
            scFftPath.lineTo(x, y);
            scTopLine.lineTo(x, y);
        }
    }
    scFftPath.lineTo(startX + width, startY + height);
    scFftPath.lineTo(startX, startY + height);
    scFftPath.closeSubPath();

    juce::ColourGradient scGrad(CustomLookAndFeel::m3Secondary.withAlpha(0.24f), 0.0f, startY + height * 0.2f,
                                CustomLookAndFeel::m3Secondary.withAlpha(0.02f), 0.0f, startY + height, false);
    g.setGradientFill(scGrad);
    g.fillPath(scFftPath);

    g.setColour(CustomLookAndFeel::m3Secondary.withAlpha(0.75f));
    g.strokePath(scTopLine, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved));

    // 5. Downward Dynamic Reduction Graph across active frequency bands
    juce::Path reductionPath;
    reductionPath.startNewSubPath(startX, zeroY);

    int activeBands = processorRef.getDuckingProcessor().getActiveNumBands();
    for (int b = 0; b < activeBands; ++b)
    {
        float freq = processorRef.getDuckingProcessor().getBandCenterFreq(b);
        float x = mapFrequencyToX(freq, startX, width);
        float redDb = smoothedReduction[b];
        float y = mapDbToY(redDb, startY, height);

        reductionPath.lineTo(x, y);
    }
    reductionPath.lineTo(startX + width, zeroY);
    reductionPath.closeSubPath();

    juce::ColourGradient redGrad(CustomLookAndFeel::m3Tertiary.withAlpha(0.38f), 0.0f, zeroY,
                                CustomLookAndFeel::m3Tertiary.withAlpha(0.03f), 0.0f, startY + height, false);
    g.setGradientFill(redGrad);
    g.fillPath(reductionPath);

    g.setColour(CustomLookAndFeel::m3Tertiary);
    g.strokePath(reductionPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved));

    // 6. Pro-Q 4 Style Interactive Sensitivity Curve
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
    g.strokePath(sensCurvePath, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved));

    // 7. Interactive Draggable M3 / Pro-Q Nodes (1 to 6)
    int activeHudBand = (draggingBandIndex >= 0) ? draggingBandIndex : hoveredBandIndex;
    if (activeHudBand < 0)
        activeHudBand = selectedBandIndex;

    juce::Point<float> activeHudPos;

    for (int i = 0; i < 6; ++i)
    {
        auto nodePos = getNodePosition(i, startX, startY, width, height);
        juce::Colour col = bandColours[i];
        bool isSelected = (i == selectedBandIndex);
        bool isHovered = (i == hoveredBandIndex);

        if (i == activeHudBand)
            activeHudPos = nodePos;

        // Selection / Hover Halo
        if (isSelected || isHovered)
        {
            g.setColour(col.withAlpha(isSelected ? 0.25f : 0.15f));
            g.fillEllipse(nodePos.x - 15.0f, nodePos.y - 15.0f, 30.0f, 30.0f);
            g.setColour(col.withAlpha(0.8f));
            g.drawEllipse(nodePos.x - 15.0f, nodePos.y - 15.0f, 30.0f, 30.0f, 1.5f);
        }

        // Dark Border & Node Body
        g.setColour(CustomLookAndFeel::m3SurfaceContainerLowest);
        g.fillEllipse(nodePos.x - 10.0f, nodePos.y - 10.0f, 20.0f, 20.0f);

        g.setColour(col);
        g.fillEllipse(nodePos.x - 8.0f, nodePos.y - 8.0f, 16.0f, 16.0f);

        // Node Number
        g.setColour(CustomLookAndFeel::m3Surface);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(juce::String(i + 1), static_cast<int>(nodePos.x - 8.0f), static_cast<int>(nodePos.y - 8.0f), 16, 16, juce::Justification::centred);
    }

    // 8. Pro-Q 4 Floating HUD Tooltip on Hover / Drag / Active Node
    if (activeHudBand >= 0 && activeHudBand < 6)
    {
        int bNum = activeHudBand + 1;
        juce::String prefix = "b" + juce::String(bNum) + "_";
        auto& apvts = processorRef.getAPVTS();

        float freq = 1000.0f;
        float gain = 0.0f;
        float q = 1.4f;
        int shape = 0;
        int slope = 1;
        int stereo = 0;

        if (auto* p = apvts.getRawParameterValue(prefix + "freq")) freq = p->load();
        if (auto* p = apvts.getRawParameterValue(prefix + "gain")) gain = p->load();
        if (auto* p = apvts.getRawParameterValue(prefix + "q")) q = p->load();
        if (auto* p = apvts.getRawParameterValue(prefix + "shape")) shape = static_cast<int>(p->load());
        if (auto* p = apvts.getRawParameterValue(prefix + "slope")) slope = static_cast<int>(p->load());
        if (auto* p = apvts.getRawParameterValue(prefix + "stereo")) stereo = static_cast<int>(p->load());

        const juce::StringArray shapeNames = { "Bell", "Low Cut", "High Cut", "Low Shelf", "High Shelf", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt" };
        const juce::StringArray slopeNames = { "6dB", "12dB", "18dB", "24dB", "30dB", "36dB", "48dB", "72dB", "96dB", "Brickwall" };
        const juce::StringArray stereoNames = { "Stereo", "Left", "Right", "Mid", "Side" };

        juce::String freqStr = freq >= 1000.0f ? juce::String(freq / 1000.0f, 2) + " kHz" : juce::String(freq, 0) + " Hz";
        juce::String hudText = "BAND " + juce::String(bNum) + " | " + shapeNames[shape].toUpperCase() + " | " + freqStr;

        if (shape == 1 || shape == 2)
            hudText += " | " + slopeNames[slope];
        else
            hudText += " | " + juce::String(gain > 0 ? "+" : "") + juce::String(gain, 1) + " dB | Q " + juce::String(q, 2);

        hudText += " | " + stereoNames[stereo].toUpperCase();

        // Dynamic width calculation so text is always fully visible and readable
        juce::Font hudFont(11.0f, juce::Font::bold);
        int hudW = hudFont.getStringWidth(hudText) + 24;
        int hudH = 24;
        int hudX = juce::jlimit(static_cast<int>(startX + 8.0f), static_cast<int>(startX + width - hudW - 8.0f), static_cast<int>(activeHudPos.x - hudW / 2));
        int hudY = (activeHudPos.y > startY + 40.0f) ? static_cast<int>(activeHudPos.y - 38.0f) : static_cast<int>(activeHudPos.y + 22.0f);

        auto hudRect = juce::Rectangle<float>(static_cast<float>(hudX), static_cast<float>(hudY), static_cast<float>(hudW), static_cast<float>(hudH));
        g.setColour(CustomLookAndFeel::m3SurfaceContainerHigh.withAlpha(0.95f));
        g.fillRoundedRectangle(hudRect, 12.0f);

        g.setColour(bandColours[activeHudBand].withAlpha(0.85f));
        g.drawRoundedRectangle(hudRect, 12.0f, 1.2f);

        g.setColour(CustomLookAndFeel::m3OnSurface);
        g.setFont(hudFont);
        g.drawText(hudText, hudRect.toNearestInt(), juce::Justification::centred);
    }

    // 9. Header Badges
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
