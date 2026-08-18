#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "AudioVisualizerFifo.h"

/**
 * 48-Band Dynamic Spectral Ducking Processor with Soothe 2-style 6-Band Parametric Sensitivity EQ
 */
class DuckingProcessor
{
public:
    static constexpr int NumBands = 48;
    static constexpr int NumParametricBands = 6;

    enum class BandType
    {
        Bell = 0,
        LowShelf,
        HighShelf,
        BandShelf,
        BandReject,
        Tilt
    };

    enum class SlopeType
    {
        Slope6dB = 0,
        Slope12dB,
        Slope24dB,
        Slope48dB
    };

    struct ParametricBandConfig
    {
        BandType type = BandType::Bell;
        SlopeType slope = SlopeType::Slope12dB;
        float freq = 1000.0f;
        float gainDb = 0.0f; // -18 to +18 dB
        float q = 1.4f;      // 0.1 to 10.0
        bool bypassed = false;
    };

    DuckingProcessor();
    ~DuckingProcessor() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setParameters(float amountPercent,
                       float attackMs,
                       float releaseMs,
                       float outputGainDb,
                       bool sidechainListen,
                       bool bypass,
                       const std::array<ParametricBandConfig, NumParametricBands>& bandConfigs);

    void process(juce::AudioBuffer<float>& mainBuffer,
                 const juce::AudioBuffer<float>& sidechainBuffer,
                 AudioVisualizerFifo& visualizerFifo);

    float getCurrentGainReduction() const noexcept { return currentGainReduction.load(); }
    float getCurrentSidechainLevel() const noexcept { return currentSidechainLevel.load(); }

    float getBandReductionDb(int bandIndex) const noexcept
    {
        if (bandIndex >= 0 && bandIndex < NumBands)
            return bandReductionTelemetry[bandIndex].load();
        return 0.0f;
    }

    float getBandCenterFreq(int bandIndex) const noexcept
    {
        if (bandIndex >= 0 && bandIndex < NumBands)
            return bands[bandIndex].centerFreq;
        return 1000.0f;
    }

    float evaluateSensitivityDb(float freq) const;

private:
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;

    // Parameters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputGain;
    float attackTimeMs = 10.0f;
    float releaseTimeMs = 100.0f;
    bool listenSidechain = false;
    bool isBypassed = false;

    std::array<ParametricBandConfig, NumParametricBands> currentBands;

    // Smoothing coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    void updateCoefficients();
    void initializeBands();

    // 48-Band Dynamic Spectral EQ structures
    struct DynamicBand
    {
        float centerFreq = 1000.0f;
        float q = 2.4f;

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

    juce::AudioBuffer<float> sidechainCopyBuffer;

    // Telemetry atomic values for UI
    std::atomic<float> currentGainReduction { 1.0f };
    std::atomic<float> currentSidechainLevel { 0.0f };
    std::array<std::atomic<float>, NumBands> bandReductionTelemetry;
};
