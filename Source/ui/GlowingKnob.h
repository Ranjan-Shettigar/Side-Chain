#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

class GlowingKnob : public juce::Slider
{
public:
    GlowingKnob();
    ~GlowingKnob() override = default;

    void paint(juce::Graphics& g) override;
    void setDynamicGainReduction(float gr) { currentGainReduction = gr; repaint(); }

private:
    float currentGainReduction = 1.0f; // 1.0 = 0dB ducking, 0.0 = max ducking
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlowingKnob)
};
