#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>
#include <algorithm>
#include "AudioVisualizerFifo.h"

/**
 * Dynamic Spectral Ducking Processor with 16 to 128 Adjustable Frequency Bands
 * and FabFilter Pro-Q 4 Inspired Parametric Sensitivity EQ
 */
class DuckingProcessor
{
public:
    static constexpr int MaxBands = 64;
    static constexpr int MinBands = 16;
    static constexpr int NumParametricBands = 6;

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
                       int numBands,
                       const std::array<ParametricBandConfig, NumParametricBands>& bandConfigs,
                       const StereoConfig& stereoConfig);

    void process(juce::AudioBuffer<float>& mainBuffer,
                 const juce::AudioBuffer<float>& sidechainBuffer,
                 AudioVisualizerFifo& visualizerFifo,
                 juce::AudioPlayHead* playHead = nullptr);

    float getCurrentGainReduction() const noexcept { return currentGainReduction.load(); }
    float getCurrentSidechainLevel() const noexcept { return currentSidechainLevel.load(); }

    int getActiveNumBands() const noexcept { return activeNumBands; }

    float getBandReductionDb(int bandIndex) const noexcept
    {
        if (bandIndex >= 0 && bandIndex < activeNumBands)
            return bandReductionTelemetry[bandIndex].load();
        return 0.0f;
    }

    float getBandCenterFreq(int bandIndex) const noexcept
    {
        if (bandIndex >= 0 && bandIndex < activeNumBands)
            return bands[bandIndex].centerFreq;
        return 1000.0f;
    }

    float evaluateSensitivityDb(float freq, int channel = 0) const;
    const std::array<ParametricBandConfig, NumParametricBands>& getBandConfigs() const noexcept { return currentBands; }
    const StereoConfig& getStereoConfig() const noexcept { return currentStereo; }
    bool isStereoInstance() const noexcept { return numChannels >= 2; }

private:
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    int numChannels = 2;
    int activeNumBands = 32;

    // Master Parameters
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAmount;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedOutputGain;
    float attackTimeMs = 10.0f;
    float releaseTimeMs = 100.0f;
    bool listenSidechain = false;
    bool isBypassed = false;

    std::array<ParametricBandConfig, NumParametricBands> currentBands;
    StereoConfig currentStereo;

    // Smoothing coefficients
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;

    void updateCoefficients();
    void initializeBands(int count);

    void encodeToMidSide(juce::AudioBuffer<float>& buffer);
    void decodeFromMidSide(juce::AudioBuffer<float>& buffer);

    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        void reset() noexcept
        {
            z1 = 0.0f;
            z2 = 0.0f;
        }

        void setBandPass(float sampleRate, float centerFreq, float q) noexcept
        {
            float w0 = 2.0f * juce::MathConstants<float>::pi * centerFreq / sampleRate;
            float alpha = std::sin(w0) / (2.0f * q);
            float cosw0 = std::cos(w0);

            float a0 = 1.0f + alpha;
            b0 = alpha / a0;
            b1 = 0.0f;
            b2 = -alpha / a0;
            a1 = (-2.0f * cosw0) / a0;
            a2 = (1.0f - alpha) / a0;
        }

        void setPeak(float sampleRate, float centerFreq, float q, float gainLinear) noexcept
        {
            if (std::abs(gainLinear - 1.0f) < 0.00005f)
            {
                b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
                a1 = 0.0f; a2 = 0.0f;
                return;
            }

            float A = std::sqrt(gainLinear);
            float w0 = 2.0f * juce::MathConstants<float>::pi * centerFreq / sampleRate;
            float alpha = std::sin(w0) / (2.0f * q);
            float cosw0 = std::cos(w0);

            float a0 = 1.0f + alpha / A;
            b0 = (1.0f + alpha * A) / a0;
            b1 = (-2.0f * cosw0) / a0;
            b2 = (1.0f - alpha * A) / a0;
            a1 = (-2.0f * cosw0) / a0;
            a2 = (1.0f - alpha / A) / a0;
        }

        inline float processSample(float in) noexcept
        {
            float out = b0 * in + z1;
            z1 = b1 * in - a1 * out + z2;
            z2 = b2 * in - a2 * out;
            return out;
        }
    };

    struct DynamicBand
    {
        float centerFreq = 1000.0f;
        float q = 2.4f;

        Biquad scFilterL;
        Biquad scFilterR;

        Biquad mainEqL;
        Biquad mainEqR;

        float envelopeStateL = 0.0f;
        float envelopeStateR = 0.0f;
        float currentGainDbL = 0.0f;
        float currentGainDbR = 0.0f;

        float sensWeightL = 1.0f;
        float sensWeightR = 1.0f;
    };

    std::array<DynamicBand, MaxBands> bands;

    juce::AudioBuffer<float> originalInputBuffer;
    juce::AudioBuffer<float> sidechainCopyBuffer;
    juce::AudioBuffer<float> soloBuffer;
    std::vector<AudioVisualizerFifo::VisualizerSample> visualizerBlockBuffer;

    // Telemetry
    std::atomic<float> currentGainReduction { 1.0f };
    std::atomic<float> currentSidechainLevel { 0.0f };
    std::array<std::atomic<float>, MaxBands> bandReductionTelemetry;
};
