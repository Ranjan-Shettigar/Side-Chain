#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../dsp/AudioVisualizerFifo.h"
#include "../dsp/DuckingProcessor.h"

class SideDuckerAudioProcessor;

/**
 * FabFilter Pro-Q 4 Inspired Interactive Frequency Sensitivity & Real-Time Reduction Graph
 */
class DuckingVisualizer : public juce::Component, public juce::Timer
{
public:
    explicit DuckingVisualizer(SideDuckerAudioProcessor& processor);
    ~DuckingVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    int getSelectedBandIndex() const noexcept { return selectedBandIndex; }
    void setSelectedBandIndex(int index);

    std::function<void(int)> onBandSelected;

    static const std::array<juce::Colour, 6> bandColours;

private:
    SideDuckerAudioProcessor& processorRef;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048
    static constexpr int numSpectrumBins = 320;

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize> fifoBufferIn {};
    std::array<float, fftSize> fifoBufferOut {};
    std::array<float, fftSize> fifoBufferSC {};
    int fifoIndex = 0;

    std::array<float, 2 * fftSize> fftDataIn {};
    std::array<float, 2 * fftSize> fftDataOut {};
    std::array<float, 2 * fftSize> fftDataSC {};

    std::vector<float> spectrumIn;
    std::vector<float> spectrumOut;
    std::vector<float> spectrumSC;
    std::vector<float> binFrequencies;

    // Smoothed 48-band reduction
    std::array<float, DuckingProcessor::NumBands> smoothedReduction;

    int selectedBandIndex = 0;
    int hoveredBandIndex = -1;
    int draggingBandIndex = -1;

    std::vector<AudioVisualizerFifo::VisualizerSample> readBuffer;

    void processNextFftBlock();
    void computeSpectrumPoints();
    float getMagnitudeForFrequency(const std::array<float, 2 * fftSize>& fftData, float frequency) const;
    float mapFrequencyToX(float freq, float startX, float width) const;
    float mapXToFrequency(float x, float startX, float width) const;
    float mapDbToY(float db, float topY, float height) const;
    float mapYToDb(float y, float topY, float height) const;

    int getBandNodeAtPosition(juce::Point<float> pos) const;
    juce::Point<float> getNodePosition(int bandIdx, float startX, float startY, float width, float height) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckingVisualizer)
};
