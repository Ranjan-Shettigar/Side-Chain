#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <array>
#include <memory>
#include <vector>
#include "dsp/DuckingProcessor.h"
#include "dsp/AudioVisualizerFifo.h"

class SideDuckerAudioProcessor : public juce::AudioProcessor
{
public:
    SideDuckerAudioProcessor();
    ~SideDuckerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    AudioVisualizerFifo& getVisualizerFifo() noexcept { return visualizerFifo; }
    DuckingProcessor& getDuckingProcessor() noexcept { return duckingProcessor; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    DuckingProcessor duckingProcessor;
    AudioVisualizerFifo visualizerFifo;

    // Master parameter atomic pointers
    std::atomic<float>* amountParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* outputGainParam = nullptr;
    std::atomic<float>* scListenParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* spectralBandsParam = nullptr;

    // Master Stereo Section parameter pointers
    std::atomic<float>* stereoModeParam = nullptr;
    std::atomic<float>* stereoLinkParam = nullptr;
    std::atomic<float>* stereoBalanceParam = nullptr;

    // 6 Parametric Band parameter pointers
    struct BandParamPointers
    {
        std::atomic<float>* shapeParam = nullptr;
        std::atomic<float>* slopeParam = nullptr;
        std::atomic<float>* stereoParam = nullptr;
        std::atomic<float>* freqParam = nullptr;
        std::atomic<float>* gainParam = nullptr;
        std::atomic<float>* qParam = nullptr;
        std::atomic<float>* bypassParam = nullptr;
        std::atomic<float>* soloParam = nullptr;
    };
    std::array<BandParamPointers, 6> bandParams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideDuckerAudioProcessor)
};
