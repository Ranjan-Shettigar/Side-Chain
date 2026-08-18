#include "DuckingProcessor.h"

DuckingProcessor::DuckingProcessor()
{
    initializeBands();
    for (int i = 0; i < NumBands; ++i)
        bandReductionTelemetry[i].store(0.0f);
}

void DuckingProcessor::initializeBands()
{
    // 48 log-spaced center frequencies from ~22 Hz to ~19,000 Hz
    const float minLog = std::log10(22.0f);
    const float maxLog = std::log10(19000.0f);

    for (int i = 0; i < NumBands; ++i)
    {
        float norm = static_cast<float>(i) / static_cast<float>(NumBands - 1);
        float logFreq = minLog + norm * (maxLog - minLog);
        bands[i].centerFreq = std::pow(10.0f, logFreq);
        bands[i].q = 2.4f; // 48-band overlapping Q
        bands[i].envelopeState = 0.0f;
        bands[i].currentGainDb = 0.0f;
        bands[i].targetGainDb = 0.0f;
    }
}

void DuckingProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    samplesPerBlock = spec.maximumBlockSize > 0 ? static_cast<int>(spec.maximumBlockSize) : 512;

    sidechainCopyBuffer.setSize(2, samplesPerBlock);

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
        bands[i].envelopeState = 0.0f;
        bands[i].currentGainDb = 0.0f;
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
        bands[i].envelopeState = 0.0f;
        bands[i].currentGainDb = 0.0f;
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

float DuckingProcessor::evaluateSensitivityDb(float freq) const
{
    float totalDb = 0.0f;

    for (int b = 0; b < NumParametricBands; ++b)
    {
        const auto& band = currentBands[b];
        if (band.bypassed)
            continue;

        float f0 = juce::jlimit(20.0f, 20000.0f, band.freq);
        float q = juce::jlimit(0.1f, 10.0f, band.q);
        float gain = band.gainDb;

        // Band 0: Low Cut
        if (b == 0)
        {
            float order = 2.0f;
            switch (band.slope)
            {
                case SlopeType::Slope6dB:  order = 1.0f; break;
                case SlopeType::Slope12dB: order = 2.0f; break;
                case SlopeType::Slope24dB: order = 4.0f; break;
                case SlopeType::Slope48dB: order = 8.0f; break;
            }
            float ratio = f0 / juce::jmax(1.0f, freq);
            float cutDb = -10.0f * order * std::log10(1.0f + std::pow(ratio, 2.0f));
            totalDb += juce::jlimit(-48.0f, 0.0f, cutDb);
        }
        // Band 5: High Cut
        else if (b == 5)
        {
            float order = 2.0f;
            switch (band.slope)
            {
                case SlopeType::Slope6dB:  order = 1.0f; break;
                case SlopeType::Slope12dB: order = 2.0f; break;
                case SlopeType::Slope24dB: order = 4.0f; break;
                case SlopeType::Slope48dB: order = 8.0f; break;
            }
            float ratio = freq / juce::jmax(1.0f, f0);
            float cutDb = -10.0f * order * std::log10(1.0f + std::pow(ratio, 2.0f));
            totalDb += juce::jlimit(-48.0f, 0.0f, cutDb);
        }
        // General Purpose Bands (1 to 4)
        else
        {
            switch (band.type)
            {
                case BandType::Bell:
                {
                    float ratio = freq / f0 - f0 / freq;
                    float denom = 1.0f + std::pow(q * ratio, 2.0f);
                    totalDb += gain / denom;
                    break;
                }
                case BandType::LowShelf:
                {
                    float ratio = freq / f0;
                    float denom = 1.0f + std::pow(ratio, 2.0f * q);
                    totalDb += gain / denom;
                    break;
                }
                case BandType::HighShelf:
                {
                    float ratio = f0 / freq;
                    float denom = 1.0f + std::pow(ratio, 2.0f * q);
                    totalDb += gain / denom;
                    break;
                }
                case BandType::BandShelf:
                {
                    float logRatio = std::log(freq / f0) / (1.0f / juce::jmax(0.2f, q));
                    float factor = std::exp(-std::pow(logRatio, 4.0f));
                    totalDb += gain * factor;
                    break;
                }
                case BandType::BandReject:
                {
                    float ratio = freq / f0 - f0 / freq;
                    float denom = 1.0f + std::pow(q * ratio, 2.0f);
                    totalDb += -36.0f / denom;
                    break;
                }
                case BandType::Tilt:
                {
                    float tiltNorm = std::log10(freq / f0) / std::log10(20000.0f / 20.0f);
                    totalDb += gain * tiltNorm * 2.0f;
                    break;
                }
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

    // If bypassed, pass audio through and reset telemetry
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

    // Sidechain Listen mode
    if (listenSidechain)
    {
        for (int ch = 0; ch < numMainChannels; ++ch)
        {
            int srcCh = juce::jmin(ch, 1);
            mainBuffer.copyFrom(ch, 0, sidechainCopyBuffer, srcCh, 0, numSamples);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float scMono = 0.5f * (sidechainCopyBuffer.getSample(0, i) + sidechainCopyBuffer.getSample(1, i));
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

        // Evaluate sensitivity weight for this frequency
        float sensitivityDb = evaluateSensitivityDb(centerF);
        float sensitivityWeight = juce::Decibels::decibelsToGain(sensitivityDb);
        sensitivityWeight = juce::jlimit(0.0f, 6.0f, sensitivityWeight);

        // Extract band energy from sidechain
        float bandMaxEnergy = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float filteredScL = band.scFilterL.processSample(scL[i]);
            float filteredScR = band.scFilterR.processSample(scR[i]);
            float scMag = juce::jmax(std::abs(filteredScL), std::abs(filteredScR));

            if (scMag > band.envelopeState)
                band.envelopeState = attackCoeff * band.envelopeState + (1.0f - attackCoeff) * scMag;
            else
                band.envelopeState = releaseCoeff * band.envelopeState + (1.0f - releaseCoeff) * scMag;

            if (band.envelopeState > bandMaxEnergy)
                bandMaxEnergy = band.envelopeState;
        }

        if (bandMaxEnergy > maxSidechainInBlock)
            maxSidechainInBlock = bandMaxEnergy;

        // Calculate dynamic ducking cut:
        // Boosting sensitivity makes the ducker more sensitive (more ducking); cutting sensitivity protects the band.
        float bandEnvNorm = juce::jlimit(0.0f, 2.0f, band.envelopeState * 2.8f);
        float targetCutDb = -48.0f * curAmount * bandEnvNorm * sensitivityWeight;
        targetCutDb = juce::jlimit(-48.0f, 0.0f, targetCutDb);

        // Smooth transition
        band.currentGainDb = band.currentGainDb * 0.4f + targetCutDb * 0.6f;
        bandReductionTelemetry[b].store(band.currentGainDb);

        if (std::abs(band.currentGainDb) > maxGainReductionInBlock)
            maxGainReductionInBlock = std::abs(band.currentGainDb);

        // Update dynamic peaking EQ on main audio
        float gainLinear = juce::Decibels::decibelsToGain(band.currentGainDb);
        auto eqCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, centerF, band.q, gainLinear);
        band.mainEqL.coefficients = eqCoeffs;
        band.mainEqR.coefficients = eqCoeffs;

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
