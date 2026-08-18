#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/AudioVisualizerFifo.h"

/**
 * Real-time Frequency Spectrum Analyzer displaying:
 * 1. Main Input audio spectrum (20Hz - 20kHz)
 * 2. Ducked Output audio spectrum
 * 3. Sidechain trigger frequency spectrum
 * 4. Real-time dynamic ducking carve-out zone (gain reduction)
 * 5. Low-Cut & High-Cut sidechain filter curtains
 */
class DuckingVisualizer : public juce::Component, public juce::Timer
{
public:
    explicit DuckingVisualizer(AudioVisualizerFifo& fifo);
    ~DuckingVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setFilterFrequencies(float lowCutHz, float highCutHz);
    void setSampleRate(double newSampleRate) { sampleRate = newSampleRate > 0 ? newSampleRate : 44100.0; }

private:
    AudioVisualizerFifo& audioFifo;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048
    static constexpr int numSpectrumBins = 320;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    // Time-domain accumulation buffers
    std::array<float, fftSize> fifoBufferIn {};
    std::array<float, fftSize> fifoBufferOut {};
    std::array<float, fftSize> fifoBufferSC {};
    int fifoIndex = 0;

    // FFT frequency-domain buffers (size = 2 * fftSize for complex output)
    std::array<float, 2 * fftSize> fftDataIn {};
    std::array<float, 2 * fftSize> fftDataOut {};
    std::array<float, 2 * fftSize> fftDataSC {};

    // Smoothed spectrum magnitudes across log frequency points
    std::vector<float> spectrumIn;
    std::vector<float> spectrumOut;
    std::vector<float> spectrumSC;

    // Log-spaced frequency mapping lookup
    std::vector<float> binFrequencies;

    double sampleRate = 44100.0;
    float lowCutFreq = 20.0f;
    float highCutFreq = 20000.0f;

    // Temporary read buffer for polling FIFO
    std::vector<AudioVisualizerFifo::VisualizerSample> readBuffer;

    void processNextFftBlock();
    void computeSpectrumPoints();
    float getMagnitudeForFrequency(const std::array<float, 2 * fftSize>& fftData, float frequency) const;
    float mapFrequencyToX(float freq, float startX, float width) const;
    float mapDbToY(float db, float topY, float height) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckingVisualizer)
};
