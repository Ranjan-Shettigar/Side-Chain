#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "AudioVisualizerFifo.h"

class DuckingProcessor
{
public:
    DuckingProcessor();
    ~DuckingProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setParameters(float amountPercent,
                       float attackMs,
                       float releaseMs,
                       float lowCutHz,
                       float highCutHz,
                       float outputGainDb,
                       bool sidechainListen,
                       bool bypass);

    void process(juce::AudioBuffer<float>& mainBuffer,
                 const juce::AudioBuffer<float>& sidechainBuffer,
                 AudioVisualizerFifo& visualizerFifo);

    float getCurrentGainReduction() const noexcept { return currentGainReduction.load(); }
    float getCurrentSidechainLevel() const noexcept { return currentSidechainLevel.load(); }

private:
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;

    // Smoothed parameters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputGain;
    float attackTimeMs = 10.0f;
    float releaseTimeMs = 100.0f;
    float lowCutFreq = 20.0f;
    float highCutFreq = 20000.0f;
    bool listenSidechain = false;
    bool isBypassed = false;

    // Envelope state
    float envelopeState = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    void updateCoefficients();

    // Sidechain filtering
    using Filter = juce::dsp::StateVariableTPTFilter<float>;
    Filter lowCutFilterL, lowCutFilterR;
    Filter highCutFilterL, highCutFilterR;

    juce::AudioBuffer<float> sidechainFilterBuffer;

    // Telemetry atomic values for UI
    std::atomic<float> currentGainReduction { 1.0f };
    std::atomic<float> currentSidechainLevel { 0.0f };
};
