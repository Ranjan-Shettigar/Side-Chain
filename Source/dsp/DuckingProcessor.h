#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "AudioVisualizerFifo.h"
#include "CustomEnvelope.h"

/**
 * 48-Band Dynamic Spectral Ducking Processor with FabFilter Pro-Q 4 Inspired Parametric Sensitivity EQ
 * and Internal Custom Envelope / LFO Engine
 */
class DuckingProcessor
{
public:
    static constexpr int NumBands = 48;
    static constexpr int NumParametricBands = 6;

    enum class SidechainSource
    {
        ExternalAudio = 0,
        InternalEnvelope
    };

    enum class FilterShape
    {
        Bell = 0,
        LowCut,
        HighCut,
        LowShelf,
        HighShelf,
        Notch,
        BandPass,
        TiltShelf,
        FlatTilt
    };

    enum class FilterSlope
    {
        Slope6dB = 0,
        Slope12dB,
        Slope18dB,
        Slope24dB,
        Slope30dB,
        Slope36dB,
        Slope48dB,
        Slope72dB,
        Slope96dB,
        Brickwall
    };

    enum class StereoPlacement
    {
        Stereo = 0,
        LeftOnly,
        RightOnly,
        MidOnly,
        SideOnly
    };

    enum class StereoMode
    {
        LeftRight = 0,
        MidSide
    };

    struct ParametricBandConfig
    {
        FilterShape shape = FilterShape::Bell;
        FilterSlope slope = FilterSlope::Slope12dB;
        StereoPlacement stereoPlacement = StereoPlacement::Stereo;
        float freq = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.4f;
        bool bypassed = false;
        bool solo = false;
    };

    struct StereoConfig
    {
        StereoMode mode = StereoMode::LeftRight;
        float linkPercent = 100.0f;   // 0 to 100
        float balancePercent = 50.0f; // 0 (full left/mid) to 100 (full right/side), 50 = even
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
                       SidechainSource scSource,
                       const std::array<ParametricBandConfig, NumParametricBands>& bandConfigs,
                       const StereoConfig& stereoConfig);

    void process(juce::AudioBuffer<float>& mainBuffer,
                 const juce::AudioBuffer<float>& sidechainBuffer,
                 AudioVisualizerFifo& visualizerFifo,
                 juce::AudioPlayHead* playHead = nullptr);

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

    float evaluateSensitivityDb(float freq, int channel = 0) const;
    const std::array<ParametricBandConfig, NumParametricBands>& getBandConfigs() const noexcept { return currentBands; }
    const StereoConfig& getStereoConfig() const noexcept { return currentStereo; }
    bool isStereoInstance() const noexcept { return numChannels >= 2; }
    SidechainSource getSidechainSource() const noexcept { return currentScSource; }

    CustomEnvelope& getCustomEnvelope() noexcept { return customEnvelope; }

private:
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    int numChannels = 2;

    // Master Parameters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputGain;
    float attackTimeMs = 10.0f;
    float releaseTimeMs = 100.0f;
    bool listenSidechain = false;
    bool isBypassed = false;
    SidechainSource currentScSource = SidechainSource::ExternalAudio;

    std::array<ParametricBandConfig, NumParametricBands> currentBands;
    StereoConfig currentStereo;
    CustomEnvelope customEnvelope;

    // Smoothing coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    void updateCoefficients();
    void initializeBands();

    void encodeToMidSide(juce::AudioBuffer<float>& buffer);
    void decodeFromMidSide(juce::AudioBuffer<float>& buffer);

    struct DynamicBand
    {
        float centerFreq = 1000.0f;
        float q = 2.4f;

        juce::dsp::IIR::Filter<float> scFilterL;
        juce::dsp::IIR::Filter<float> scFilterR;

        juce::dsp::IIR::Filter<float> mainEqL;
        juce::dsp::IIR::Filter<float> mainEqR;

        float envelopeStateL = 0.0f;
        float envelopeStateR = 0.0f;
        float currentGainDbL = 0.0f;
        float currentGainDbR = 0.0f;
    };

    std::array<DynamicBand, NumBands> bands;

    juce::AudioBuffer<float> sidechainCopyBuffer;
    juce::AudioBuffer<float> soloBuffer;

    // Telemetry
    std::atomic<float> currentGainReduction { 1.0f };
    std::atomic<float> currentSidechainLevel { 0.0f };
    std::array<std::atomic<float>, NumBands> bandReductionTelemetry;
};
