#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ui/CustomLookAndFeel.h"
#include "ui/GlowingKnob.h"
#include "ui/DuckingVisualizer.h"

class SideDuckerAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit SideDuckerAudioProcessorEditor(SideDuckerAudioProcessor&);
    ~SideDuckerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    SideDuckerAudioProcessor& processorRef;
    CustomLookAndFeel customLookAndFeel;

    // Visualizer
    DuckingVisualizer visualizer;

    // Controls
    GlowingKnob amountSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider lowCutSlider;
    juce::Slider highCutSlider;
    juce::Slider outputGainSlider;

    juce::ToggleButton scListenButton;
    juce::ToggleButton bypassButton;

    // Labels
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label lowCutLabel;
    juce::Label highCutLabel;
    juce::Label outputGainLabel;

    // APVTS Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> lowCutAttachment;
    std::unique_ptr<SliderAttachment> highCutAttachment;
    std::unique_ptr<SliderAttachment> outputGainAttachment;

    std::unique_ptr<ButtonAttachment> scListenAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    void setupRotaryControl(juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideDuckerAudioProcessorEditor)
};
