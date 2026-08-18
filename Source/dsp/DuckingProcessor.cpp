#include "DuckingProcessor.h"

DuckingProcessor::DuckingProcessor()
{
    lowCutFilterL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    lowCutFilterR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    highCutFilterL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    highCutFilterR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    initializeBands();
}

void DuckingProcessor::initializeBands()
{
    // Compute 32 log-spaced center frequencies from ~25 Hz to ~18,500 Hz (1/3 octave spacing)
    const float minLog = std::log10(25.0f);
    const float maxLog = std::log10(18500.0f);

    for (int i = 0; i < NumBands; ++i)
    {
        float norm = static_cast<float>(i) / static_cast<float>(NumBands - 1);
        float logFreq = minLog + norm * (maxLog - minLog);
        bands[i].centerFreq = std::pow(10.0f, logFreq);
        bands[i].q = 1.8f; // standard bandwidth for 32-band overlapping coverage
        bands[i].envelopeState = 0.0f;
        bands[i].currentGainDb = 0.0f;
        bands[i].targetGainDb = 0.0f;
    }
}

void DuckingProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    samplesPerBlock = spec.maximumBlockSize > 0 ? static_cast<int>(spec.maximumBlockSize) : 512;

    juce::dsp::ProcessSpec filterSpec;
    filterSpec.sampleRate = sampleRate;
    filterSpec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    filterSpec.numChannels = 1;

    lowCutFilterL.prepare(filterSpec);
    lowCutFilterR.prepare(filterSpec);
    highCutFilterL.prepare(filterSpec);
    highCutFilterR.prepare(filterSpec);

    sidechainFilterBuffer.setSize(2, samplesPerBlock);

    smoothedAmount.reset(sampleRate, 0.02); // 20ms ramp
    smoothedOutputGain.reset(sampleRate, 0.02);

    // Initialize filter coefficients for all 32 bands
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
    lowCutFilterL.reset();
    lowCutFilterR.reset();
    highCutFilterL.reset();
    highCutFilterR.reset();

    for (int i = 0; i < NumBands; ++i)
    {
        bands[i].scFilterL.reset();
        bands[i].scFilterR.reset();
        bands[i].mainEqL.reset();
        bands[i].mainEqR.reset();
        bands[i].envelopeState = 0.0f;
        bands[i].currentGainDb = 0.0f;
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

    float safeLowCut = juce::jlimit(20.0f, 20000.0f, lowCutFreq);
    float safeHighCut = juce::jlimit(20.0f, 20000.0f, highCutFreq);

    lowCutFilterL.setCutoffFrequency(safeLowCut);
    lowCutFilterR.setCutoffFrequency(safeLowCut);
    highCutFilterL.setCutoffFrequency(safeHighCut);
    highCutFilterR.setCutoffFrequency(safeHighCut);
}

void DuckingProcessor::setParameters(float amountPercent,
                                     float attackMs,
                                     float releaseMs,
                                     float lowCutHz,
                                     float highCutHz,
                                     float outputGainDb,
                                     bool sidechainListen,
                                     bool bypass)
{
    smoothedAmount.setTargetValue(juce::jlimit(0.0f, 100.0f, amountPercent) / 100.0f);
    smoothedOutputGain.setTargetValue(juce::Decibels::decibelsToGain(outputGainDb));

    bool needCoeffUpdate = false;
    if (std::abs(attackTimeMs - attackMs) > 0.001f || std::abs(releaseTimeMs - releaseMs) > 0.001f)
    {
        attackTimeMs = attackMs;
        releaseTimeMs = releaseMs;
        needCoeffUpdate = true;
    }

    if (std::abs(lowCutFreq - lowCutHz) > 0.1f || std::abs(highCutFreq - highCutHz) > 0.1f)
    {
        lowCutFreq = lowCutHz;
        highCutFreq = highCutHz;
        needCoeffUpdate = true;
    }

    if (needCoeffUpdate)
        updateCoefficients();

    listenSidechain = sidechainListen;
    isBypassed = bypass;
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

    // If bypassed, push clean audio and return
    if (isBypassed)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float inL = numMainChannels > 0 ? mainBuffer.getSample(0, i) : 0.0f;
            float inR = numMainChannels > 1 ? mainBuffer.getSample(1, i) : inL;
            float inMono = 0.5f * (inL + inR);
            visualizerFifo.pushSample(inMono, inMono, 0.0f, 1.0f);
        }
        currentGainReduction.store(1.0f);
        currentSidechainLevel.store(0.0f);
        return;
    }

    // Ensure sidechain filter buffer size
    if (sidechainFilterBuffer.getNumSamples() < numSamples)
        sidechainFilterBuffer.setSize(2, numSamples, false, false, true);

    // Copy sidechain signal into temporary filter buffer
    if (numSidechainChannels > 0)
    {
        sidechainFilterBuffer.copyFrom(0, 0, sidechainBuffer, 0, 0, numSamples);
        if (numSidechainChannels > 1)
            sidechainFilterBuffer.copyFrom(1, 0, sidechainBuffer, 1, 0, numSamples);
        else
            sidechainFilterBuffer.copyFrom(1, 0, sidechainBuffer, 0, 0, numSamples);
    }
    else
    {
        sidechainFilterBuffer.clear();
    }

    // Apply global Low-Cut and High-Cut sidechain filtering
    for (int i = 0; i < numSamples; ++i)
    {
        float scL = sidechainFilterBuffer.getSample(0, i);
        float scR = sidechainFilterBuffer.getSample(1, i);

        scL = lowCutFilterL.processSample(0, scL);
        scL = highCutFilterL.processSample(0, scL);

        scR = lowCutFilterR.processSample(0, scR);
        scR = highCutFilterR.processSample(0, scR);

        sidechainFilterBuffer.setSample(0, i, scL);
        sidechainFilterBuffer.setSample(1, i, scR);
    }

    // If Sidechain Listen is activated, route filtered sidechain directly to output
    if (listenSidechain)
    {
        for (int ch = 0; ch < numMainChannels; ++ch)
        {
            int srcCh = juce::jmin(ch, 1);
            mainBuffer.copyFrom(ch, 0, sidechainFilterBuffer, srcCh, 0, numSamples);
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float scMono = 0.5f * (sidechainFilterBuffer.getSample(0, i) + sidechainFilterBuffer.getSample(1, i));
            visualizerFifo.pushSample(scMono, scMono, scMono, 1.0f);
        }
        currentGainReduction.store(1.0f);
        currentSidechainLevel.store(0.0f);
        return;
    }

    // Capture initial un-ducked main audio for visualizer comparison
    juce::AudioBuffer<float> originalInputCopy;
    originalInputCopy.makeCopyOf(mainBuffer);

    float curAmount = smoothedAmount.getNextValue();
    float maxSidechainInBlock = 0.0f;
    float maxGainReductionInBlock = 0.0f; // in dB

    const float* scL = sidechainFilterBuffer.getReadPointer(0);
    const float* scR = sidechainFilterBuffer.getReadPointer(1);

    // 1. Process 32 frequency bands independently
    for (int b = 0; b < NumBands; ++b)
    {
        auto& band = bands[b];
        float centerF = band.centerFreq;

        // Band filtering weight based on Low-Cut and High-Cut range
        float bandWeight = 1.0f;
        if (centerF < lowCutFreq)
        {
            float ratio = centerF / juce::jmax(1.0f, lowCutFreq);
            bandWeight = juce::jlimit(0.0f, 1.0f, ratio * ratio);
        }
        else if (centerF > highCutFreq)
        {
            float ratio = highCutFreq / centerF;
            bandWeight = juce::jlimit(0.0f, 1.0f, ratio * ratio);
        }

        // Detect sidechain energy specifically in this band
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

        // Calculate dynamic ducking cut specifically for this band (Trackspacer dynamic EQ curve)
        // Up to -48 dB cut at 100% amount when sidechain has high energy
        float bandEnvNorm = juce::jlimit(0.0f, 2.0f, band.envelopeState * 2.5f);
        float targetCutDb = -48.0f * curAmount * bandEnvNorm * bandWeight;
        targetCutDb = juce::jlimit(-48.0f, 0.0f, targetCutDb);

        // Smooth band gain transition
        band.currentGainDb = band.currentGainDb * 0.4f + targetCutDb * 0.6f;

        if (std::abs(band.currentGainDb) > maxGainReductionInBlock)
            maxGainReductionInBlock = std::abs(band.currentGainDb);

        // Update dynamic peaking bell EQ coefficients on the main audio for this frequency band
        float gainLinear = juce::Decibels::decibelsToGain(band.currentGainDb);
        auto eqCoeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter(sampleRate, centerF, band.q, gainLinear);
        band.mainEqL.coefficients = eqCoeffs;
        band.mainEqR.coefficients = eqCoeffs;

        // Apply dynamic EQ cut to main audio channels
        float* ch0 = numMainChannels > 0 ? mainBuffer.getWritePointer(0) : nullptr;
        float* ch1 = numMainChannels > 1 ? mainBuffer.getWritePointer(1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            if (ch0 != nullptr) ch0[i] = band.mainEqL.processSample(ch0[i]);
            if (ch1 != nullptr) ch1[i] = band.mainEqR.processSample(ch1[i]);
        }
    }

    // 2. Apply Output Gain Trim & push telemetry to Visualizer FIFO
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
