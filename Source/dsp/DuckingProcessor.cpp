#include "DuckingProcessor.h"

DuckingProcessor::DuckingProcessor()
{
    lowCutFilterL.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    lowCutFilterR.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    highCutFilterL.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    highCutFilterR.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
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

    reset();
}

void DuckingProcessor::reset()
{
    lowCutFilterL.reset();
    lowCutFilterR.reset();
    highCutFilterL.reset();
    highCutFilterR.reset();

    envelopeState = 0.0f;
    currentGainReduction.store(1.0f);
    currentSidechainLevel.store(0.0f);
    updateCoefficients();
}

void DuckingProcessor::updateCoefficients()
{
    // Convert attack/release ms to smoothing coefficients
    float attackSec = juce::jmax(0.0001f, attackTimeMs * 0.001f);
    float releaseSec = juce::jmax(0.001f, releaseTimeMs * 0.001f);

    attackCoeff = std::exp(-1.0f / (attackSec * static_cast<float>(sampleRate)));
    releaseCoeff = std::exp(-1.0f / (releaseSec * static_cast<float>(sampleRate)));

    // Update filter cutoffs
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

    // If bypassed, push clean telemetry and return
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

    // Prepare sidechain signal into temporary filter buffer
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
        // No sidechain input routed, clear
        sidechainFilterBuffer.clear();
    }

    // Apply Low-Cut and High-Cut sidechain filtering
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

    // If Sidechain Listen is activated, route filtered sidechain straight to output
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

    // Main ducking processing loop
    float minGainReductionInBlock = 1.0f;
    float maxSidechainInBlock = 0.0f;

    float* mainL = numMainChannels > 0 ? mainBuffer.getWritePointer(0) : nullptr;
    float* mainR = numMainChannels > 1 ? mainBuffer.getWritePointer(1) : nullptr;

    const float* scFilteredL = sidechainFilterBuffer.getReadPointer(0);
    const float* scFilteredR = sidechainFilterBuffer.getReadPointer(1);

    for (int i = 0; i < numSamples; ++i)
    {
        float curAmount = smoothedAmount.getNextValue();
        float curGain = smoothedOutputGain.getNextValue();

        // 1. Detect sidechain trigger amplitude
        float scSampleL = std::abs(scFilteredL[i]);
        float scSampleR = std::abs(scFilteredR[i]);
        float scRect = juce::jmax(scSampleL, scSampleR);

        if (scRect > maxSidechainInBlock)
            maxSidechainInBlock = scRect;

        // 2. Ballistics Envelope Follower
        if (scRect > envelopeState)
            envelopeState = attackCoeff * envelopeState + (1.0f - attackCoeff) * scRect;
        else
            envelopeState = releaseCoeff * envelopeState + (1.0f - releaseCoeff) * scRect;

        // 3. Calculate Gain Reduction
        // Using dynamic compression/ducking curve:
        // When amount = 0.0, gainFactor = 1.0 (0 dB reduction)
        // When amount = 1.0, ducking reaches max attenuation (-48dB or full silence depending on trigger)
        float envNorm = juce::jlimit(0.0f, 2.0f, envelopeState);
        
        // Trackspacer-style logarithmic ducking curve
        float reductionDb = -48.0f * curAmount * envNorm;
        reductionDb = juce::jlimit(-60.0f, 0.0f, reductionDb);
        float gainFactor = juce::Decibels::decibelsToGain(reductionDb);

        if (gainFactor < minGainReductionInBlock)
            minGainReductionInBlock = gainFactor;

        // 4. Capture original main input for visualizer
        float inL = mainL != nullptr ? mainL[i] : 0.0f;
        float inR = mainR != nullptr ? mainR[i] : 0.0f;
        float inMono = 0.5f * (inL + inR);

        // 5. Apply ducking and output gain
        float outL = inL * gainFactor * curGain;
        float outR = inR * gainFactor * curGain;

        if (mainL != nullptr) mainL[i] = outL;
        if (mainR != nullptr) mainR[i] = outR;

        float outMono = 0.5f * (outL + outR);

        // 6. Push to visualizer FIFO
        visualizerFifo.pushSample(inMono, outMono, scRect, gainFactor);
    }

    currentGainReduction.store(minGainReductionInBlock);
    currentSidechainLevel.store(maxSidechainInBlock);
}
