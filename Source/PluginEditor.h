#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ui/CustomLookAndFeel.h"
#include "ui/GlowingKnob.h"
#include "ui/DuckingVisualizer.h"
#include "ui/EnvelopeEditor.h"

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

    // View Switcher (Spectrum Visualizer vs Custom Envelope)
    juce::TextButton viewSpectrumButton;
    juce::TextButton viewEnvelopeButton;

    // Visualizers
    DuckingVisualizer visualizer;
    EnvelopeEditor envelopeEditor;

    // Master Controls
    GlowingKnob amountSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider outputGainSlider;

    juce::ToggleButton scListenButton;
    juce::ToggleButton bypassButton;

    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label outputGainLabel;

    // Master Stereo Section Controls
    juce::Label stereoTitleLabel;
    juce::Label stereoModeLabel;
    juce::TextButton stereoModeButton;

    juce::Label stereoLinkLabel;
    juce::Slider stereoLinkSlider;

    juce::Label stereoBalanceLabel;
    juce::Slider stereoBalanceSlider;
    juce::Label stereoBalanceValueLabel;

    // Band Selector Pill Buttons (1 to 6)
    std::array<juce::TextButton, 6> bandSelectButtons;

    // Dedicated Pro-Q 4 Controls for Selected Band
    juce::Label bandTitleLabel;
    juce::ComboBox bandShapeComboBox;
    juce::ComboBox bandSlopeComboBox;
    juce::ComboBox bandStereoComboBox;
    juce::Slider bandFreqSlider;
    juce::Slider bandGainSlider;
    juce::Slider bandQSlider;
    juce::ToggleButton bandSoloButton;
    juce::ToggleButton bandBypassButton;
    juce::TextButton resetEqButton;

    // EQ Presets & Spectral Bands Resolution Control
    juce::ComboBox eqPresetComboBox;
    juce::Label eqPresetLabel;

    juce::Slider spectralBandsSlider;
    juce::Label spectralBandsLabel;

    juce::Label bandShapeLabel;
    juce::Label bandSlopeLabel;
    juce::Label bandStereoLabel;
    juce::Label bandFreqLabel;
    juce::Label bandGainLabel;
    juce::Label bandQLabel;

    int currentSelectedBand = 0;

    // APVTS Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> amountAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> outputGainAttachment;
    std::unique_ptr<ButtonAttachment> scListenAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<SliderAttachment> spectralBandsAttachment;

    // Master Stereo Attachments
    std::unique_ptr<SliderAttachment> stereoLinkAttachment;
    std::unique_ptr<SliderAttachment> stereoBalanceAttachment;

    // Dynamic attachments for selected band
    std::unique_ptr<ComboBoxAttachment> selectedShapeAttachment;
    std::unique_ptr<ComboBoxAttachment> selectedSlopeAttachment;
    std::unique_ptr<ComboBoxAttachment> selectedStereoAttachment;
    std::unique_ptr<SliderAttachment> selectedFreqAttachment;
    std::unique_ptr<SliderAttachment> selectedGainAttachment;
    std::unique_ptr<SliderAttachment> selectedQAttachment;
    std::unique_ptr<ButtonAttachment> selectedBypassAttachment;
    std::unique_ptr<ButtonAttachment> selectedSoloAttachment;

    void setupRotaryControl(juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix);
    void updateSelectedBandUI(int bandIndex);
    void updateStereoSectionUI();
    void setViewMode(int modeIndex); // 0 = Spectrum, 1 = Custom Envelope

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SideDuckerAudioProcessorEditor)
};
