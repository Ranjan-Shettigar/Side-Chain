#include "DuckingProcessor.h"

DuckingProcessor::DuckingProcessor()
{
    initializeBands();
    for (int i = 0; i < NumBands; ++i)
        bandReductionTelemetry[i].store(0.0f);
}

void DuckingProcessor::initializeBands()
{
    // 48 log-spaced center frequencies from ~20 Hz to ~20,000 Hz
    const float minLog = std::log10(20.0f);
    const float maxLog = std::log10(20000.0f);

    for (int i = 0; i < NumBands; ++i)
    {
        float norm = static_cast<float>(i) / static_cast<float>(NumBands - 1);
        float logFreq = minLog + norm * (maxLog - minLog);
        bands[i].centerFreq = std::pow(10.0f, logFreq);
        bands[i].q = 2.4f; // 48-band overlapping Q
        bands[i].envelopeStateL = 0.0f;
        bands[i].envelopeStateR = 0.0f;
        bands[i].currentGainDbL = 0.0f;
        bands[i].currentGainDbR = 0.0f;
    }
}

void DuckingProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    samplesPerBlock = spec.maximumBlockSize > 0 ? static_cast<int>(spec.maximumBlockSize) : 512;

    sidechainCopyBuffer.setSize(2, samplesPerBlock);
    soloBuffer.setSize(2, samplesPerBlock);

    smoothedAmount.reset(sampleRate, 0.02);
    smoothedOutputGain.reset(sampleRate, 0.02);

    for (int i = 0; i < NumBands; ++i)
    {
        auto scCoeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, bands[i].centerFreq, bands[i].q);
        bands[i].scFilterL.coefficients = scCoeffs;
        bands[i].scFilterR.coefficients = scCoeffs;

        auto mainCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, bands[i].centerFreq, bands[i].q, 1.0f);
        bands[i].mainEqL.coefficients = mainCoeffs;
        bands[i].mainEqR.coefficients = mainCoeffs;

        bands[i].scFilterL.reset();
        bands[i].scFilterR.reset();
        bands[i].mainEqL.reset();
        bands[i].mainEqR.reset();
        bands[i].envelopeStateL = 0.0f;
        bands[i].envelopeStateR = 0.0f;
        bands[i].currentGainDbL = 0.0f;
        bands[i].currentGainDbR = 0.0f;
    }

    reset();
}

void DuckingProcessor::reset()
{
    for (int i = 0; i < NumBands; ++i)
    {
        bands[i].scFilterL.reset();
        bands[i].scFilterR.reset();
        bands[i].mainEqL.reset();
        bands[i].mainEqR.reset();
        bands[i].envelopeStateL = 0.0f;
        bands[i].envelopeStateR = 0.0f;
        bands[i].currentGainDbL = 0.0f;
        bands[i].currentGainDbR = 0.0f;
        bandReductionTelemetry[i].store(0.0f);
    }

    currentGainReduction.store(1.0f);
    currentSidechainLevel.store(0.0f);
    updateCoefficients();
}

void DuckingProcessor::updateCoefficients()
{
    float attackSec = juce::jmax(0.0001f, attackTimeMs * 0.001f);
    float releaseSec = juce::jmax(0.001f, releaseTimeMs * 0.001f);

    attackCoeff = std::exp(-1.0f / (attackSec * static_cast<float>(sampleRate)));
    releaseCoeff = std::exp(-1.0f / (releaseSec * static_cast<float>(sampleRate)));
}

float DuckingProcessor::evaluateSensitivityDb(float freq, int channel) const
{
    float totalDb = 0.0f;

    for (int b = 0; b < NumParametricBands; ++b)
    {
        const auto& band = currentBands[b];
        if (band.bypassed)
            continue;

        // Check Stereo Placement matching
        if (band.stereoPlacement == StereoPlacement::LeftOnly && channel != 0)
            continue;
        if (band.stereoPlacement == StereoPlacement::RightOnly && channel != 1)
            continue;

        float f0 = juce::jlimit(20.0f, 20000.0f, band.freq);
        float q = juce::jlimit(0.1f, 10.0f, band.q);
        float gain = band.gainDb;

        float delta = (freq / f0) - (f0 / freq);

        switch (band.shape)
        {
            case FilterShape::Bell:
            {
                float denom = 1.0f + std::pow(q * delta, 2.0f);
                totalDb += gain / denom;
                break;
            }
            case FilterShape::LowCut:
            {
                float order = 2.0f;
                switch (band.slope)
                {
                    case FilterSlope::Slope6dB:   order = 1.0f;  break;
                    case FilterSlope::Slope12dB:  order = 2.0f;  break;
                    case FilterSlope::Slope18dB:  order = 3.0f;  break;
                    case FilterSlope::Slope24dB:  order = 4.0f;  break;
                    case FilterSlope::Slope30dB:  order = 5.0f;  break;
                    case FilterSlope::Slope36dB:  order = 6.0f;  break;
                    case FilterSlope::Slope48dB:  order = 8.0f;  break;
                    case FilterSlope::Slope72dB:  order = 12.0f; break;
                    case FilterSlope::Slope96dB:  order = 16.0f; break;
                    case FilterSlope::Brickwall:  order = 24.0f; break;
                }
                float ratio = f0 / juce::jmax(1.0f, freq);
                float cutDb = -10.0f * order * std::log10(1.0f + std::pow(ratio, 2.0f));
                totalDb += juce::jlimit(-60.0f, 0.0f, cutDb);
                break;
            }
            case FilterShape::HighCut:
            {
                float order = 2.0f;
                switch (band.slope)
                {
                    case FilterSlope::Slope6dB:   order = 1.0f;  break;
                    case FilterSlope::Slope12dB:  order = 2.0f;  break;
                    case FilterSlope::Slope18dB:  order = 3.0f;  break;
                    case FilterSlope::Slope24dB:  order = 4.0f;  break;
                    case FilterSlope::Slope30dB:  order = 5.0f;  break;
                    case FilterSlope::Slope36dB:  order = 6.0f;  break;
                    case FilterSlope::Slope48dB:  order = 8.0f;  break;
                    case FilterSlope::Slope72dB:  order = 12.0f; break;
                    case FilterSlope::Slope96dB:  order = 16.0f; break;
                    case FilterSlope::Brickwall:  order = 24.0f; break;
                }
                float ratio = freq / juce::jmax(1.0f, f0);
                float cutDb = -10.0f * order * std::log10(1.0f + std::pow(ratio, 2.0f));
                totalDb += juce::jlimit(-60.0f, 0.0f, cutDb);
                break;
            }
            case FilterShape::LowShelf:
            {
                float ratio = freq / f0;
                float denom = 1.0f + std::pow(ratio, 2.0f * q);
                totalDb += gain / denom;
                break;
            }
            case FilterShape::HighShelf:
            {
                float ratio = f0 / freq;
                float denom = 1.0f + std::pow(ratio, 2.0f * q);
                totalDb += gain / denom;
                break;
            }
            case FilterShape::Notch:
            {
                float denom = 1.0f + std::pow(q * delta, 2.0f);
                totalDb += -48.0f / denom;
                break;
            }
            case FilterShape::BandPass:
            {
                float denom = 1.0f + std::pow(q * delta, 2.0f);
                totalDb += -48.0f * (1.0f - 1.0f / denom);
                break;
            }
            case FilterShape::TiltShelf:
            {
                float logRatio = std::log10(freq / f0);
                totalDb += gain * std::tanh(1.5f * logRatio);
                break;
            }
            case FilterShape::FlatTilt:
            {
                float tiltNorm = std::log10(freq / 1000.0f) / std::log10(20000.0f / 20.0f);
                totalDb += gain * tiltNorm * 2.0f;
                break;
            }
        }
    }

    return juce::jlimit(-48.0f, 24.0f, totalDb);
}

void DuckingProcessor::setParameters(float amountPercent,
                                     float attackMs,
                                     float releaseMs,
                                     float outputGainDb,
                                     bool sidechainListen,
                                     bool bypass,
                                     const std::array<ParametricBandConfig, NumParametricBands>& bandConfigs)
{
    smoothedAmount.setTargetValue(juce::jlimit(0.0f, 100.0f, amountPercent) / 100.0f);
    smoothedOutputGain.setTargetValue(juce::Decibels::decibelsToGain(outputGainDb));

    if (std::abs(attackTimeMs - attackMs) > 0.001f || std::abs(releaseTimeMs - releaseMs) > 0.001f)
    {
        attackTimeMs = attackMs;
        releaseTimeMs = releaseMs;
        updateCoefficients();
    }

    listenSidechain = sidechainListen;
    isBypassed = bypass;
    currentBands = bandConfigs;
}

void DuckingProcessor::process(juce::AudioBuffer<float>& mainBuffer,
                              const juce::AudioBuffer<float>& sidechainBuffer,
                              AudioVisualizerFifo& visualizerFifo)
{
    const int numSamples = mainBuffer.getNumSamples();
    const int numMainChannels = mainBuffer.getNumChannels();
    const int numSidechainChannels = sidechainBuffer.getNumChannels();

    if (numSamples == 0)
        return;

    // Check if any band is in Solo / Audition mode
    int soloBandIndex = -1;
    for (int b = 0; b < NumParametricBands; ++b)
    {
        if (currentBands[b].solo && !currentBands[b].bypassed)
        {
            soloBandIndex = b;
            break;
        }
    }

    // If bypassed, pass audio through
    if (isBypassed)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float inL = numMainChannels > 0 ? mainBuffer.getSample(0, i) : 0.0f;
            float inR = numMainChannels > 1 ? mainBuffer.getSample(1, i) : inL;
            float inMono = 0.5f * (inL + inR);
            visualizerFifo.pushSample(inMono, inMono, 0.0f, 1.0f);
        }
        for (int b = 0; b < NumBands; ++b)
            bandReductionTelemetry[b].store(0.0f);

        currentGainReduction.store(1.0f);
        currentSidechainLevel.store(0.0f);
        return;
    }

    if (sidechainCopyBuffer.getNumSamples() < numSamples)
        sidechainCopyBuffer.setSize(2, numSamples, false, false, true);

    if (numSidechainChannels > 0)
    {
        sidechainCopyBuffer.copyFrom(0, 0, sidechainBuffer, 0, 0, numSamples);
        if (numSidechainChannels > 1)
            sidechainCopyBuffer.copyFrom(1, 0, sidechainBuffer, 1, 0, numSamples);
        else
            sidechainCopyBuffer.copyFrom(1, 0, sidechainBuffer, 0, 0, numSamples);
    }
    else
    {
        sidechainCopyBuffer.clear();
    }

    // Sidechain Master Listen or Per-Band Solo Audition Mode
    if (listenSidechain || soloBandIndex >= 0)
    {
        const float* srcL = sidechainCopyBuffer.getReadPointer(0);
        const float* srcR = sidechainCopyBuffer.getReadPointer(1);

        for (int ch = 0; ch < numMainChannels; ++ch)
        {
            const float* src = (ch == 0) ? srcL : srcR;
            mainBuffer.copyFrom(ch, 0, src, numSamples);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float scMono = 0.5f * (srcL[i] + srcR[i]);
            visualizerFifo.pushSample(scMono, scMono, scMono, 1.0f);
        }
        for (int b = 0; b < NumBands; ++b)
            bandReductionTelemetry[b].store(0.0f);

        currentGainReduction.store(1.0f);
        currentSidechainLevel.store(0.0f);
        return;
    }

    juce::AudioBuffer<float> originalInputCopy;
    originalInputCopy.makeCopyOf(mainBuffer);

    float curAmount = smoothedAmount.getNextValue();
    float maxSidechainInBlock = 0.0f;
    float maxGainReductionInBlock = 0.0f;

    const float* scL = sidechainCopyBuffer.getReadPointer(0);
    const float* scR = sidechainCopyBuffer.getReadPointer(1);

    // Process all 48 frequency bands
    for (int b = 0; b < NumBands; ++b)
    {
        auto& band = bands[b];
        float centerF = band.centerFreq;

        // Evaluate sensitivity weight for Left and Right channels
        float sensDbL = evaluateSensitivityDb(centerF, 0);
        float sensDbR = evaluateSensitivityDb(centerF, 1);

        float sensWeightL = juce::jlimit(0.0f, 6.0f, juce::Decibels::decibelsToGain(sensDbL));
        float sensWeightR = juce::jlimit(0.0f, 6.0f, juce::Decibels::decibelsToGain(sensDbR));

        // Extract band energy from sidechain
        float bandMaxEnergy = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float fScL = band.scFilterL.processSample(scL[i]);
            float fScR = band.scFilterR.processSample(scR[i]);

            float scMagL = std::abs(fScL);
            float scMagR = std::abs(fScR);
            float scMagMax = juce::jmax(scMagL, scMagR);

            if (scMagL > band.envelopeStateL)
                band.envelopeStateL = attackCoeff * band.envelopeStateL + (1.0f - attackCoeff) * scMagL;
            else
                band.envelopeStateL = releaseCoeff * band.envelopeStateL + (1.0f - releaseCoeff) * scMagL;

            if (scMagR > band.envelopeStateR)
                band.envelopeStateR = attackCoeff * band.envelopeStateR + (1.0f - attackCoeff) * scMagR;
            else
                band.envelopeStateR = releaseCoeff * band.envelopeStateR + (1.0f - releaseCoeff) * scMagR;

            if (scMagMax > bandMaxEnergy)
                bandMaxEnergy = scMagMax;
        }

        if (bandMaxEnergy > maxSidechainInBlock)
            maxSidechainInBlock = bandMaxEnergy;

        // Calculate dynamic ducking cut per channel
        float envNormL = juce::jlimit(0.0f, 2.0f, band.envelopeStateL * 2.8f);
        float envNormR = juce::jlimit(0.0f, 2.0f, band.envelopeStateR * 2.8f);

        float targetCutDbL = juce::jlimit(-48.0f, 0.0f, -48.0f * curAmount * envNormL * sensWeightL);
        float targetCutDbR = juce::jlimit(-48.0f, 0.0f, -48.0f * curAmount * envNormR * sensWeightR);

        band.currentGainDbL = band.currentGainDbL * 0.4f + targetCutDbL * 0.6f;
        band.currentGainDbR = band.currentGainDbR * 0.4f + targetCutDbR * 0.6f;

        float avgCutDb = 0.5f * (band.currentGainDbL + band.currentGainDbR);
        bandReductionTelemetry[b].store(avgCutDb);

        if (std::abs(avgCutDb) > maxGainReductionInBlock)
            maxGainReductionInBlock = std::abs(avgCutDb);

        // Update dynamic peaking EQ on main audio
        float gainLinearL = juce::Decibels::decibelsToGain(band.currentGainDbL);
        float gainLinearR = juce::Decibels::decibelsToGain(band.currentGainDbR);

        auto eqCoeffsL = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, centerF, band.q, gainLinearL);
        auto eqCoeffsR = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, centerF, band.q, gainLinearR);

        band.mainEqL.coefficients = eqCoeffsL;
        band.mainEqR.coefficients = eqCoeffsR;

        float* ch0 = numMainChannels > 0 ? mainBuffer.getWritePointer(0) : nullptr;
        float* ch1 = numMainChannels > 1 ? mainBuffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            if (ch0 != nullptr) ch0[i] = band.mainEqL.processSample(ch0[i]);
            if (ch1 != nullptr) ch1[i] = band.mainEqR.processSample(ch1[i]);
        }
    }

    // Apply Output Gain & push audio to visualizer FIFO
    float* finalL = numMainChannels > 0 ? mainBuffer.getWritePointer(0) : nullptr;
    float* finalR = numMainChannels > 1 ? mainBuffer.getWritePointer(1) : nullptr;

    const float* origL = originalInputCopy.getReadPointer(0);
    const float* origR = numMainChannels > 1 ? originalInputCopy.getReadPointer(1) : origL;

    for (int i = 0; i < numSamples; ++i)
    {
        float outGain = smoothedOutputGain.getNextValue();
        if (finalL != nullptr) finalL[i] *= outGain;
        if (finalR != nullptr) finalR[i] *= outGain;

        float inMono = 0.5f * (origL[i] + origR[i]);
        float outMono = 0.5f * ((finalL != nullptr ? finalL[i] : 0.0f) + (finalR != nullptr ? finalR[i] : 0.0f));
        float scMono = 0.5f * (scL[i] + scR[i]);
        float minLinearGr = juce::Decibels::decibelsToGain(-maxGainReductionInBlock);

        visualizerFifo.pushSample(inMono, outMono, scMono, minLinearGr);
    }

    float overallLinearGr = juce::Decibels::decibelsToGain(-maxGainReductionInBlock);
    currentGainReduction.store(overallLinearGr);
    currentSidechainLevel.store(maxSidechainInBlock);
}
