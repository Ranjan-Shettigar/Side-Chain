#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "AudioVisualizerFifo.h"

/**
 * 32-Band Dynamic Spectral Ducking Processor (Trackspacer-style)
 * Dynamically ducks ONLY the specific frequencies in the main signal that are active
 * in the external sidechain signal, carving out precise spectral space.
 */
class DuckingProcessor
{
public:
    static constexpr int NumBands = 32;

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

    // Parameters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputGain;
    float attackTimeMs = 10.0f;
    float releaseTimeMs = 100.0f;
    float lowCutFreq = 20.0f;
    float highCutFreq = 20000.0f;
    bool listenSidechain = false;
    bool isBypassed = false;

    // Smoothing coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    void updateCoefficients();
    void initializeBands();

    // 32-Band Dynamic Spectral EQ structures
    struct DynamicBand
    {
        float centerFreq = 1000.0f;
        float q = 1.8f;

        // Sidechain band detection filters (Bandpass)
        juce::dsp::IIR::Filter<float> scFilterL;
        juce::dsp::IIR::Filter<float> scFilterR;

        // Main signal dynamic peaking EQ filters
        juce::dsp::IIR::Filter<float> mainEqL;
        juce::dsp::IIR::Filter<float> mainEqR;

        // Envelope state
        float envelopeState = 0.0f;
        float currentGainDb = 0.0f;
        float targetGainDb = 0.0f;
    };

    std::array<DynamicBand, NumBands> bands;

    // Master Sidechain Low-Cut and High-Cut filters for global trigger shaping
    using SVFilter = juce::dsp::StateVariableTPTFilter<float>;
    SVFilter lowCutFilterL, lowCutFilterR;
    SVFilter highCutFilterL, highCutFilterR;

    juce::AudioBuffer<float> sidechainFilterBuffer;

    // Telemetry atomic values for UI
    std::atomic<float> currentGainReduction { 1.0f };
    std::atomic<float> currentSidechainLevel { 0.0f };
};
