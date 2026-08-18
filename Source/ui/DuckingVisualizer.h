#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/AudioVisualizerFifo.h"

class DuckingVisualizer : public juce::Component, public juce::Timer
{
public:
    DuckingVisualizer(AudioVisualizerFifo& fifo);
    ~DuckingVisualizer() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setFilterFrequencies(float lowCutHz, float highCutHz);

private:
    AudioVisualizerFifo& audioFifo;

    static constexpr int NumHistoryPoints = 300;
    struct HistoryPoint
    {
        float inLevel = 0.0f;
        float outLevel = 0.0f;
        float scLevel = 0.0f;
        float gainReduction = 1.0f;
    };

    std::vector<HistoryPoint> history;
    int writeIndex = 0;

    float lowCutFreq = 20.0f;
    float highCutFreq = 20000.0f;

    // Temporary read buffer for polling FIFO
    std::vector<AudioVisualizerFifo::VisualizerSample> readBuffer;

    void pushPoint(float in, float out, float sc, float gr);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckingVisualizer)
};
