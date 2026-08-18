#include "DuckingProcessor.h"

DuckingProcessor::DuckingProcessor()
{
    initializeBands(48);
    for (int i = 0; i < MaxBands; ++i)
        bandReductionTelemetry[i].store(0.0f);
}

void DuckingProcessor::initializeBands(int count)
{
    activeNumBands = juce::jlimit(MinBands, MaxBands, count);
    const float minLog = std::log10(20.0f);
    const float maxLog = std::log10(20000.0f);

    float bandQ = 2.4f * (static_cast<float>(activeNumBands) / 48.0f);
    bandQ = juce::jlimit(1.2f, 8.0f, bandQ);

    for (int i = 0; i < activeNumBands; ++i)
    {
        float norm = static_cast<float>(i) / static_cast<float>(activeNumBands - 1);
        float logFreq = minLog + norm * (maxLog - minLog);
        bands[i].centerFreq = std::pow(10.0f, logFreq);
        bands[i].q = bandQ;
        bands[i].envelopeStateL = 0.0f;
        bands[i].envelopeStateR = 0.0f;
        bands[i].currentGainDbL = 0.0f;
        bands[i].currentGainDbR = 0.0f;

        if (sampleRate > 0.0)
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
        }
    }
}

void DuckingProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    samplesPerBlock = spec.maximumBlockSize > 0 ? static_cast<int>(spec.maximumBlockSize) : 512;
    numChannels = static_cast<int>(spec.numChannels);

    sidechainCopyBuffer.setSize(2, samplesPerBlock);
    soloBuffer.setSize(2, samplesPerBlock);

    smoothedAmount.reset(sampleRate, 0.02);
    smoothedOutputGain.reset(sampleRate, 0.02);
    customEnvelope.setSampleRate(sampleRate);

    initializeBands(activeNumBands);
    reset();
}

void DuckingProcessor::reset()
{
    for (int i = 0; i < MaxBands; ++i)
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
    customEnvelope.reset();
    updateCoefficients();
}

void DuckingProcessor::updateCoefficients()
{
    float attackSec = juce::jmax(0.0001f, attackTimeMs * 0.001f);
    float releaseSec = juce::jmax(0.001f, releaseTimeMs * 0.001f);

    attackCoeff = std::exp(-1.0f / (attackSec * static_cast<float>(sampleRate)));
    releaseCoeff = std::exp(-1.0f / (releaseSec * static_cast<float>(sampleRate)));
}

void DuckingProcessor::encodeToMidSide(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return;

    float* chL = buffer.getWritePointer(0);
    float* chR = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        float l = chL[i];
        float r = chR[i];
        chL[i] = 0.5f * (l + r);
        chR[i] = 0.5f * (l - r);
    }
}

void DuckingProcessor::decodeFromMidSide(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() < 2)
        return;

    float* chM = buffer.getWritePointer(0);
    float* chS = buffer.getWritePointer(1);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        float m = chM[i];
        float s = chS[i];
        chM[i] = m + s;
        chS[i] = m - s;
    }
}

float DuckingProcessor::evaluateSensitivityDb(float freq, int channel) const
{
    float totalDb = 0.0f;

    for (int b = 0; b < NumParametricBands; ++b)
    {
        const auto& band = currentBands[b];
        if (band.bypassed)
            continue;

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
                                     SidechainSource scSource,
                                     int numBands,
                                     const std::array<ParametricBandConfig, NumParametricBands>& bandConfigs,
                                     const StereoConfig& stereoConfig)
{
    smoothedAmount.setTargetValue(juce::jlimit(0.0f, 100.0f, amountPercent) / 100.0f);
    smoothedOutputGain.setTargetValue(juce::Decibels::decibelsToGain(outputGainDb));

    if (std::abs(attackTimeMs - attackMs) > 0.001f || std::abs(releaseTimeMs - releaseMs) > 0.001f)
    {
        attackTimeMs = attackMs;
        releaseTimeMs = releaseMs;
        updateCoefficients();
    }

    if (numBands != activeNumBands)
    {
        initializeBands(numBands);
    }

    listenSidechain = sidechainListen;
    isBypassed = bypass;
    currentScSource = scSource;
    currentBands = bandConfigs;
    currentStereo = stereoConfig;
}

void DuckingProcessor::process(juce::AudioBuffer<float>& mainBuffer,
                              const juce::AudioBuffer<float>& sidechainBuffer,
                              AudioVisualizerFifo& visualizerFifo,
                              juce::AudioPlayHead* playHead)
{
    const int numSamples = mainBuffer.getNumSamples();
    const int numMainChannels = mainBuffer.getNumChannels();
    const int numSidechainChannels = sidechainBuffer.getNumChannels();

    if (numSamples == 0)
        return;

    int soloBandIndex = -1;
    for (int b = 0; b < NumParametricBands; ++b)
    {
        if (currentBands[b].solo && !currentBands[b].bypassed)
        {
            soloBandIndex = b;
            break;
        }
    }

    if (isBypassed)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float inL = numMainChannels > 0 ? mainBuffer.getSample(0, i) : 0.0f;
            float inR = numMainChannels > 1 ? mainBuffer.getSample(1, i) : inL;
            float inMono = 0.5f * (inL + inR);
            visualizerFifo.pushSample(inMono, inMono, 0.0f, 1.0f);
        }
        for (int b = 0; b < activeNumBands; ++b)
            bandReductionTelemetry[b].store(0.0f);

        currentGainReduction.store(1.0f);
        currentSidechainLevel.store(0.0f);
        return;
    }

    if (sidechainCopyBuffer.getNumSamples() < numSamples)
        sidechainCopyBuffer.setSize(2, numSamples, false, false, true);

    if (currentScSource == SidechainSource::InternalEnvelope)
    {
        float* scL = sidechainCopyBuffer.getWritePointer(0);
        float* scR = sidechainCopyBuffer.getWritePointer(1);
        customEnvelope.processBlock(scL, scR, numSamples, playHead);
    }
    else if (numSidechainChannels > 0)
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
        for (int b = 0; b < activeNumBands; ++b)
            bandReductionTelemetry[b].store(0.0f);

        currentGainReduction.store(1.0f);
        currentSidechainLevel.store(0.0f);
        return;
    }

    juce::AudioBuffer<float> originalInputCopy;
    originalInputCopy.makeCopyOf(mainBuffer);

    bool useMidSide = (currentStereo.mode == StereoMode::MidSide) && (numMainChannels >= 2);
    if (useMidSide)
    {
        encodeToMidSide(mainBuffer);
        encodeToMidSide(sidechainCopyBuffer);
    }

    float curAmount = smoothedAmount.getNextValue();
    float maxSidechainInBlock = 0.0f;
    float maxGainReductionInBlock = 0.0f;

    float linkFactor = juce::jlimit(0.0f, 1.0f, currentStereo.linkPercent / 100.0f);
    float balanceNorm = juce::jlimit(0.0f, 1.0f, currentStereo.balancePercent / 100.0f);
    float processingWeightL = juce::jlimit(0.0f, 1.0f, 2.0f * (1.0f - balanceNorm));
    float processingWeightR = juce::jlimit(0.0f, 1.0f, 2.0f * balanceNorm);

    const float* scL = sidechainCopyBuffer.getReadPointer(0);
    const float* scR = sidechainCopyBuffer.getReadPointer(1);

    // Process all active frequency bands (16 to 128)
    for (int b = 0; b < activeNumBands; ++b)
    {
        auto& band = bands[b];
        float centerF = band.centerFreq;

        float sensDbL = evaluateSensitivityDb(centerF, 0);
        float sensDbR = evaluateSensitivityDb(centerF, 1);

        float sensWeightL = juce::jlimit(0.0f, 6.0f, juce::Decibels::decibelsToGain(sensDbL));
        float sensWeightR = juce::jlimit(0.0f, 6.0f, juce::Decibels::decibelsToGain(sensDbR));

        float bandMaxEnergy = 0.0f;

        if (currentScSource == SidechainSource::InternalEnvelope)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float scMagL = std::abs(scL[i]);
                float scMagR = std::abs(scR[i]);
                float scMagMax = juce::jmax(scMagL, scMagR);

                band.envelopeStateL = scMagL;
                band.envelopeStateR = scMagR;

                if (scMagMax > bandMaxEnergy)
                    bandMaxEnergy = scMagMax;
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float fScL = band.scFilterL.processSample(scL[i]);
                float fScR = band.scFilterR.processSample(scR[i]);

                float scMagL = std::abs(fScL);
                float scMagR = std::abs(fScR);

                float scSum = 0.5f * (scMagL + scMagR);
                float linkedMagL = scMagL + linkFactor * (scSum - scMagL);
                float linkedMagR = scMagR + linkFactor * (scSum - scMagR);

                float scMagMax = juce::jmax(linkedMagL, linkedMagR);

                if (linkedMagL > band.envelopeStateL)
                    band.envelopeStateL = attackCoeff * band.envelopeStateL + (1.0f - attackCoeff) * linkedMagL;
                else
                    band.envelopeStateL = releaseCoeff * band.envelopeStateL + (1.0f - releaseCoeff) * linkedMagL;

                if (linkedMagR > band.envelopeStateR)
                    band.envelopeStateR = attackCoeff * band.envelopeStateR + (1.0f - attackCoeff) * linkedMagR;
                else
                    band.envelopeStateR = releaseCoeff * band.envelopeStateR + (1.0f - releaseCoeff) * linkedMagR;

                if (scMagMax > bandMaxEnergy)
                    bandMaxEnergy = scMagMax;
            }
        }

        if (bandMaxEnergy > maxSidechainInBlock)
            maxSidechainInBlock = bandMaxEnergy;

        float envNormL = juce::jlimit(0.0f, 2.0f, band.envelopeStateL * 2.8f);
        float envNormR = juce::jlimit(0.0f, 2.0f, band.envelopeStateR * 2.8f);

        float targetCutDbL = juce::jlimit(-48.0f, 0.0f, -48.0f * curAmount * envNormL * sensWeightL * processingWeightL);
        float targetCutDbR = juce::jlimit(-48.0f, 0.0f, -48.0f * curAmount * envNormR * sensWeightR * processingWeightR);

        band.currentGainDbL = band.currentGainDbL * 0.4f + targetCutDbL * 0.6f;
        band.currentGainDbR = band.currentGainDbR * 0.4f + targetCutDbR * 0.6f;

        float avgCutDb = 0.5f * (band.currentGainDbL + band.currentGainDbR);
        bandReductionTelemetry[b].store(avgCutDb);

        if (std::abs(avgCutDb) > maxGainReductionInBlock)
            maxGainReductionInBlock = std::abs(avgCutDb);

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

    if (useMidSide)
        decodeFromMidSide(mainBuffer);

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
        float scMono = 0.5f * (sidechainCopyBuffer.getSample(0, i) + sidechainCopyBuffer.getSample(1, i));
        float minLinearGr = juce::Decibels::decibelsToGain(-maxGainReductionInBlock);

        visualizerFifo.pushSample(inMono, outMono, scMono, minLinearGr);
    }

    float overallLinearGr = juce::Decibels::decibelsToGain(-maxGainReductionInBlock);
    currentGainReduction.store(overallLinearGr);
    currentSidechainLevel.store(maxSidechainInBlock);
}
