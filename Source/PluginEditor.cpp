#include "PluginEditor.h"

SideDuckerAudioProcessorEditor::SideDuckerAudioProcessorEditor(SideDuckerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      visualizer(p)
{
    setLookAndFeel(&customLookAndFeel);

    // Visualizer Graph
    addAndMakeVisible(visualizer);
    visualizer.onBandSelected = [this](int bandIdx) {
        updateSelectedBandUI(bandIdx);
    };

    // Hero Amount Knob
    amountSlider.setName("Amount");
    addAndMakeVisible(amountSlider);
    amountAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "amount", amountSlider);

    // Master Envelope Controls
    setupRotaryControl(attackSlider, attackLabel, "ATTACK", "");
    attackSlider.setName("Attack");
    attackAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "attack", attackSlider);

    setupRotaryControl(releaseSlider, releaseLabel, "RELEASE", "");
    releaseSlider.setName("Release");
    releaseAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "release", releaseSlider);

    // Master Output Trim Control
    setupRotaryControl(outputGainSlider, outputGainLabel, "TRIM", "");
    outputGainSlider.setName("Output Gain");
    outputGainAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "outputGain", outputGainSlider);

    // Master Action Chips
    scListenButton.setButtonText("SC LISTEN");
    scListenButton.setName("listen");
    addAndMakeVisible(scListenButton);
    scListenAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), "scListen", scListenButton);

    bypassButton.setButtonText("BYPASS");
    bypassButton.setName("bypass");
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), "bypass", bypassButton);

    // Master Stereo Controls
    stereoModeLabel.setText("mode", juce::dontSendNotification);
    stereoModeLabel.setFont(juce::Font(11.0f, juce::Font::plain));
    stereoModeLabel.setJustificationType(juce::Justification::centred);
    stereoModeLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(stereoModeLabel);

    stereoModeButton.setButtonText("left | right");
    stereoModeButton.setClickingTogglesState(false);
    stereoModeButton.onClick = [this]() {
        auto& apvts = processorRef.getAPVTS();
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("stereo_mode")))
        {
            int nextIdx = (param->getIndex() == 0) ? 1 : 0;
            *param = nextIdx;
            updateStereoSectionUI();
        }
    };
    addAndMakeVisible(stereoModeButton);

    stereoLinkLabel.setText("link", juce::dontSendNotification);
    stereoLinkLabel.setFont(juce::Font(11.0f, juce::Font::plain));
    stereoLinkLabel.setJustificationType(juce::Justification::centred);
    stereoLinkLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(stereoLinkLabel);

    stereoLinkSlider.setSliderStyle(juce::Slider::LinearBar);
    stereoLinkSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    stereoLinkSlider.setName("Stereo Link");
    addAndMakeVisible(stereoLinkSlider);
    stereoLinkAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "stereo_link", stereoLinkSlider);

    stereoBalanceLabel.setText("balance", juce::dontSendNotification);
    stereoBalanceLabel.setFont(juce::Font(11.0f, juce::Font::plain));
    stereoBalanceLabel.setJustificationType(juce::Justification::centred);
    stereoBalanceLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(stereoBalanceLabel);

    stereoBalanceSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    stereoBalanceSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    stereoBalanceSlider.setName("Stereo Balance");
    addAndMakeVisible(stereoBalanceSlider);
    stereoBalanceAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "stereo_balance", stereoBalanceSlider);

    stereoBalanceValueLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    stereoBalanceValueLabel.setJustificationType(juce::Justification::centred);
    stereoBalanceValueLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurface);
    stereoBalanceValueLabel.setColour(juce::Label::backgroundColourId, CustomLookAndFeel::m3SurfaceContainerHigh);
    stereoBalanceValueLabel.setText("even", juce::dontSendNotification);
    addAndMakeVisible(stereoBalanceValueLabel);

    stereoBalanceSlider.onValueChange = [this]() {
        updateStereoSectionUI();
    };

    // Band Selector Pill Buttons (1 to 6)
    for (int i = 0; i < 6; ++i)
    {
        auto& btn = bandSelectButtons[i];
        btn.setButtonText(juce::String(i + 1));
        btn.setClickingTogglesState(false);
        btn.onClick = [this, i]() {
            visualizer.setSelectedBandIndex(i);
            updateSelectedBandUI(i);
        };
        addAndMakeVisible(btn);
    }

    // Band Control Strip Components
    bandTitleLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    bandTitleLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurface);
    addAndMakeVisible(bandTitleLabel);

    bandShapeLabel.setText("SHAPE", juce::dontSendNotification);
    bandShapeLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    bandShapeLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(bandShapeLabel);
    addAndMakeVisible(bandShapeComboBox);

    bandSlopeLabel.setText("SLOPE", juce::dontSendNotification);
    bandSlopeLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    bandSlopeLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(bandSlopeLabel);
    addAndMakeVisible(bandSlopeComboBox);

    bandStereoLabel.setText("PLACEMENT", juce::dontSendNotification);
    bandStereoLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    bandStereoLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(bandStereoLabel);
    addAndMakeVisible(bandStereoComboBox);

    setupRotaryControl(bandFreqSlider, bandFreqLabel, "FREQ", "");
    setupRotaryControl(bandGainSlider, bandGainLabel, "GAIN", "");
    setupRotaryControl(bandQSlider, bandQLabel, "Q", "");

    bandSoloButton.setButtonText("SOLO");
    bandSoloButton.setName("solo");
    addAndMakeVisible(bandSoloButton);

    bandBypassButton.setButtonText("BYPASS");
    bandBypassButton.setName("bandBypass");
    addAndMakeVisible(bandBypassButton);

    resetEqButton.setButtonText("RESET EQ");
    resetEqButton.onClick = [this]() {
        auto& apvts = processorRef.getAPVTS();
        const std::array<int, 6> defaultShapes = { 1, 0, 0, 0, 0, 2 };
        const std::array<float, 6> defaultFreqs = { 30.0f, 120.0f, 380.0f, 1500.0f, 6000.0f, 18000.0f };

        for (int i = 0; i < 6; ++i)
        {
            int bNum = i + 1;
            juce::String prefix = "b" + juce::String(bNum) + "_";

            if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(prefix + "shape")))
                *p = defaultShapes[i];
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(prefix + "slope")))
                *p = 1;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(prefix + "stereo")))
                *p = 0;
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "freq")))
                *p = defaultFreqs[i];
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "gain")))
                *p = 0.0f;
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "q")))
                *p = 1.4f;
            if (auto* p = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(prefix + "bypass")))
                *p = false;
            if (auto* p = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(prefix + "solo")))
                *p = false;
        }

        updateSelectedBandUI(currentSelectedBand);
        visualizer.repaint();
    };
    addAndMakeVisible(resetEqButton);

    // EQ Presets Label & Dropdown
    eqPresetLabel.setText("PRESET", juce::dontSendNotification);
    eqPresetLabel.setFont(juce::Font(10.0f, juce::Font::bold));
    eqPresetLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(eqPresetLabel);

    eqPresetComboBox.addItemList({
        "1. Factory Default (Flat 6-Band)",
        "2. Kick & Sub Squeeze (30-100Hz)",
        "3. Kick Punch Pocket (60-120Hz)",
        "4. Vocal Clarity Carve (1k-4kHz)",
        "5. Vocal Warmth & Body (300Hz & 3.5k)",
        "6. Snare Body & Snap (200Hz & 4kHz)",
        "7. Bass Mud Cleaner (200-500Hz)",
        "8. Hi-Hat & Air Tamer (8k-16kHz)",
        "9. Master Bus Gentle Duck",
        "10. EDM Synth Pump (Bass & Mid Dip)",
        "11. Acoustic Guitar Duck (500Hz-3kHz)"
    }, 1);
    eqPresetComboBox.setText("EQ Presets", juce::dontSendNotification);
    eqPresetComboBox.onChange = [this]() {
        int id = eqPresetComboBox.getSelectedId();
        auto& apvts = processorRef.getAPVTS();

        auto setBand = [&](int bandIdx, int shape, int slope, int stereo, float freq, float gain, float q, bool bypassed) {
            int bNum = bandIdx + 1;
            juce::String prefix = "b" + juce::String(bNum) + "_";
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(prefix + "shape"))) *p = shape;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(prefix + "slope"))) *p = slope;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(prefix + "stereo"))) *p = stereo;
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "freq"))) *p = freq;
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "gain"))) *p = gain;
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(prefix + "q"))) *p = q;
            if (auto* p = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(prefix + "bypass"))) *p = bypassed;
            if (auto* p = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(prefix + "solo"))) *p = false;
        };

        switch (id)
        {
            case 1: // Factory Default (Flat 6-Band)
                setBand(0, 1, 1, 0, 30.0f, 0.0f, 1.4f, false);
                setBand(1, 0, 1, 0, 120.0f, 0.0f, 1.4f, false);
                setBand(2, 0, 1, 0, 380.0f, 0.0f, 1.4f, false);
                setBand(3, 0, 1, 0, 1500.0f, 0.0f, 1.4f, false);
                setBand(4, 0, 1, 0, 6000.0f, 0.0f, 1.4f, false);
                setBand(5, 2, 1, 0, 18000.0f, 0.0f, 1.4f, false);
                break;
            case 2: // Kick & Sub Squeeze (30-100Hz Focus)
                setBand(0, 1, 3, 0, 25.0f, 0.0f, 1.4f, false); // 24dB Low Cut
                setBand(1, 0, 1, 0, 55.0f, +8.0f, 3.2f, false); // Heavy sub duck sensitivity
                setBand(2, 0, 1, 0, 110.0f, +5.0f, 2.8f, false);
                setBand(3, 0, 1, 0, 400.0f, -4.0f, 1.4f, false);
                setBand(4, 0, 1, 0, 2500.0f, -6.0f, 1.0f, false);
                setBand(5, 2, 2, 0, 8000.0f, 0.0f, 1.4f, false);
                break;
            case 3: // Kick Punch Pocket (60-120Hz)
                setBand(0, 1, 1, 0, 35.0f, 0.0f, 1.4f, false);
                setBand(1, 0, 1, 0, 85.0f, +9.0f, 3.5f, false); // Kick fundamental punch
                setBand(2, 0, 1, 0, 180.0f, +4.0f, 2.5f, false);
                setBand(3, 0, 1, 0, 800.0f, -3.0f, 1.2f, false);
                setBand(4, 0, 1, 0, 3000.0f, +2.0f, 2.0f, false);
                setBand(5, 2, 1, 0, 16000.0f, 0.0f, 1.4f, false);
                break;
            case 4: // Vocal Clarity Carve (1k-4kHz)
                setBand(0, 1, 1, 0, 80.0f, 0.0f, 1.4f, false);
                setBand(1, 0, 1, 0, 300.0f, -2.0f, 1.5f, false);
                setBand(2, 0, 1, 0, 1200.0f, +6.0f, 2.2f, false); // Vocal core
                setBand(3, 0, 1, 0, 3500.0f, +7.5f, 2.5f, false); // Articulation
                setBand(4, 4, 1, 0, 10000.0f, +3.0f, 0.7f, false); // High shelf air
                setBand(5, 2, 1, 0, 19000.0f, 0.0f, 1.4f, false);
                break;
            case 5: // Vocal Warmth & Body (300Hz & 3.5k)
                setBand(0, 1, 1, 0, 60.0f, 0.0f, 1.4f, false);
                setBand(1, 0, 1, 0, 280.0f, +6.5f, 2.8f, false); // Vocal chest warmth
                setBand(2, 0, 1, 0, 900.0f, -1.5f, 1.4f, false);
                setBand(3, 0, 1, 0, 3200.0f, +5.5f, 2.2f, false); // Presence
                setBand(4, 4, 1, 0, 8000.0f, +2.5f, 0.8f, false);
                setBand(5, 2, 1, 0, 18000.0f, 0.0f, 1.4f, false);
                break;
            case 6: // Snare Body & Snap (200Hz & 4kHz)
                setBand(0, 1, 1, 0, 90.0f, 0.0f, 1.4f, false);
                setBand(1, 0, 1, 0, 210.0f, +7.5f, 3.2f, false); // Snare body
                setBand(2, 0, 1, 0, 850.0f, -3.0f, 1.5f, false);
                setBand(3, 0, 1, 0, 3800.0f, +8.0f, 3.0f, false); // Snare crack
                setBand(4, 0, 1, 0, 8000.0f, +3.0f, 1.6f, false);
                setBand(5, 2, 1, 0, 18000.0f, 0.0f, 1.4f, false);
                break;
            case 7: // Bass Mud Cleaner (200-500Hz)
                setBand(0, 1, 1, 0, 40.0f, 0.0f, 1.4f, false);
                setBand(1, 0, 1, 0, 90.0f, -2.0f, 1.5f, false);
                setBand(2, 0, 1, 0, 260.0f, +8.0f, 3.5f, false); // Mud focus
                setBand(3, 0, 1, 0, 480.0f, +6.0f, 3.0f, false); // Buildup
                setBand(4, 0, 1, 0, 2200.0f, -4.0f, 1.2f, false);
                setBand(5, 2, 1, 0, 16000.0f, 0.0f, 1.4f, false);
                break;
            case 8: // Hi-Hat & Air Tamer (8k-16kHz)
                setBand(0, 1, 3, 0, 3000.0f, 0.0f, 1.4f, false);
                setBand(1, 0, 1, 0, 6000.0f, +4.0f, 2.0f, false);
                setBand(2, 0, 1, 0, 9000.0f, +7.0f, 2.6f, false);
                setBand(3, 0, 1, 0, 12500.0f, +6.0f, 2.2f, false);
                setBand(4, 4, 1, 0, 15000.0f, +4.0f, 0.7f, false);
                setBand(5, 2, 1, 0, 20000.0f, 0.0f, 1.4f, false);
                break;
            case 9: // Master Bus Gentle Duck
                setBand(0, 1, 0, 0, 20.0f, 0.0f, 0.7f, false);
                setBand(1, 0, 1, 0, 80.0f, +2.0f, 1.0f, false);
                setBand(2, 0, 1, 0, 400.0f, +1.5f, 1.0f, false);
                setBand(3, 0, 1, 0, 2000.0f, +2.0f, 1.0f, false);
                setBand(4, 0, 1, 0, 7000.0f, +1.5f, 1.0f, false);
                setBand(5, 2, 0, 0, 20000.0f, 0.0f, 0.7f, false);
                break;
            case 10: // EDM Synth Pump (Bass & Mid Dip)
                setBand(0, 1, 2, 0, 35.0f, 0.0f, 1.4f, false);
                setBand(1, 0, 1, 0, 65.0f, +9.0f, 2.8f, false);
                setBand(2, 0, 1, 0, 220.0f, +6.0f, 2.2f, false);
                setBand(3, 0, 1, 0, 1000.0f, +4.0f, 1.8f, false);
                setBand(4, 4, 1, 0, 6500.0f, -2.0f, 0.8f, false);
                setBand(5, 2, 1, 0, 18000.0f, 0.0f, 1.4f, false);
                break;
            case 11: // Acoustic Guitar Duck (500Hz-3kHz)
                setBand(0, 1, 1, 0, 70.0f, 0.0f, 1.4f, false);
                setBand(1, 0, 1, 0, 180.0f, -3.0f, 1.4f, false);
                setBand(2, 0, 1, 0, 650.0f, +6.0f, 2.5f, false);
                setBand(3, 0, 1, 0, 1800.0f, +7.0f, 2.8f, false);
                setBand(4, 0, 1, 0, 3200.0f, +5.0f, 2.2f, false);
                setBand(5, 2, 1, 0, 17000.0f, 0.0f, 1.4f, false);
                break;
        }

        updateSelectedBandUI(currentSelectedBand);
        visualizer.repaint();
    };
    addAndMakeVisible(eqPresetComboBox);

    // Sharpness Resolution Knob (16 to 64)
    setupRotaryControl(spectralBandsSlider, spectralBandsLabel, "SHARPNESS", " Bands");
    spectralBandsSlider.setName("Sharpness");
    spectralBandsSlider.setNumDecimalPlacesToDisplay(0);
    addAndMakeVisible(spectralBandsSlider);
    spectralBandsAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "spectral_bands", spectralBandsSlider);

    // Initial Band Selection (Band 1: Cyan)
    updateSelectedBandUI(0);
    updateStereoSectionUI();

    // Dimensions: 880 x 610
    setSize(880, 610);
    setResizable(false, false);

    startTimerHz(60);
}

SideDuckerAudioProcessorEditor::~SideDuckerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void SideDuckerAudioProcessorEditor::setupRotaryControl(juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    slider.setTextValueSuffix(suffix);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(10.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(label);
}

void SideDuckerAudioProcessorEditor::updateStereoSectionUI()
{
    bool isStereo = processorRef.getDuckingProcessor().isStereoInstance();

    stereoModeLabel.setEnabled(isStereo);
    stereoModeButton.setEnabled(isStereo);
    stereoLinkLabel.setEnabled(isStereo);
    stereoLinkSlider.setEnabled(isStereo);
    stereoBalanceLabel.setEnabled(isStereo);
    stereoBalanceSlider.setEnabled(isStereo);
    stereoBalanceValueLabel.setEnabled(isStereo);

    auto& apvts = processorRef.getAPVTS();
    int modeIdx = 0;
    if (auto* pM = apvts.getRawParameterValue("stereo_mode"))
        modeIdx = static_cast<int>(pM->load());

    stereoModeButton.setButtonText(modeIdx == 0 ? "left | right" : "mid | side");

    float bal = static_cast<float>(stereoBalanceSlider.getValue());
    if (std::abs(bal) < 0.5f)
    {
        stereoBalanceValueLabel.setText("even", juce::dontSendNotification);
    }
    else if (modeIdx == 0) // Left | Right
    {
        if (bal < 0.0f)
        {
            int rVal = static_cast<int>(100.0f + bal);
            stereoBalanceValueLabel.setText("100 L | " + juce::String(rVal) + " R", juce::dontSendNotification);
        }
        else
        {
            int lVal = static_cast<int>(100.0f - bal);
            stereoBalanceValueLabel.setText(juce::String(lVal) + " L | 100 R", juce::dontSendNotification);
        }
    }
    else // Mid | Side
    {
        if (bal < 0.0f)
        {
            int sVal = static_cast<int>(100.0f + bal);
            stereoBalanceValueLabel.setText("100 M | " + juce::String(sVal) + " S", juce::dontSendNotification);
        }
        else
        {
            int mVal = static_cast<int>(100.0f - bal);
            stereoBalanceValueLabel.setText(juce::String(mVal) + " M | 100 S", juce::dontSendNotification);
        }
    }

    repaint();
}

void SideDuckerAudioProcessorEditor::updateSelectedBandUI(int bandIndex)
{
    currentSelectedBand = juce::jlimit(0, 5, bandIndex);

    for (int i = 0; i < 6; ++i)
        bandSelectButtons[i].setToggleState(i == currentSelectedBand, juce::dontSendNotification);

    selectedShapeAttachment.reset();
    selectedSlopeAttachment.reset();
    selectedStereoAttachment.reset();
    selectedFreqAttachment.reset();
    selectedGainAttachment.reset();
    selectedQAttachment.reset();
    selectedBypassAttachment.reset();
    selectedSoloAttachment.reset();

    auto& apvts = processorRef.getAPVTS();
    int bNum = currentSelectedBand + 1;
    juce::String prefix = "b" + juce::String(bNum) + "_";

    const std::array<juce::String, 6> colorNames = { "CYAN", "AMBER", "PEACH", "ROSE", "LAVENDER", "PINK" };
    bandTitleLabel.setText("BAND " + juce::String(bNum) + " (" + colorNames[currentSelectedBand] + ")", juce::dontSendNotification);
    bandTitleLabel.setColour(juce::Label::textColourId, DuckingVisualizer::bandColours[currentSelectedBand]);

    bandShapeComboBox.clear(juce::dontSendNotification);
    bandShapeComboBox.addItemList({ "Bell", "Low Cut", "High Cut", "Low Shelf", "High Shelf", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt" }, 1);
    selectedShapeAttachment = std::make_unique<ComboBoxAttachment>(apvts, prefix + "shape", bandShapeComboBox);

    bandSlopeComboBox.clear(juce::dontSendNotification);
    bandSlopeComboBox.addItemList({ "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct", "Brickwall" }, 1);
    selectedSlopeAttachment = std::make_unique<ComboBoxAttachment>(apvts, prefix + "slope", bandSlopeComboBox);

    bandStereoComboBox.clear(juce::dontSendNotification);
    bandStereoComboBox.addItemList({ "Stereo", "Left", "Right", "Mid", "Side" }, 1);
    selectedStereoAttachment = std::make_unique<ComboBoxAttachment>(apvts, prefix + "stereo", bandStereoComboBox);

    selectedFreqAttachment = std::make_unique<SliderAttachment>(apvts, prefix + "freq", bandFreqSlider);
    selectedGainAttachment = std::make_unique<SliderAttachment>(apvts, prefix + "gain", bandGainSlider);
    selectedQAttachment    = std::make_unique<SliderAttachment>(apvts, prefix + "q", bandQSlider);
    selectedBypassAttachment = std::make_unique<ButtonAttachment>(apvts, prefix + "bypass", bandBypassButton);
    selectedSoloAttachment   = std::make_unique<ButtonAttachment>(apvts, prefix + "solo", bandSoloButton);

    int curShape = 0;
    if (auto* pS = apvts.getRawParameterValue(prefix + "shape"))
        curShape = static_cast<int>(pS->load());

    bool isCut = (curShape == 1 || curShape == 2);
    bandSlopeComboBox.setVisible(isCut);
    bandSlopeLabel.setVisible(isCut);
    bandGainSlider.setVisible(!isCut);
    bandGainLabel.setVisible(!isCut);

    bandShapeComboBox.onChange = [this]() {
        int shape = bandShapeComboBox.getSelectedId() - 1;
        bool cut = (shape == 1 || shape == 2);
        bandSlopeComboBox.setVisible(cut);
        bandSlopeLabel.setVisible(cut);
        bandGainSlider.setVisible(!cut);
        bandGainLabel.setVisible(!cut);
        resized();
        visualizer.repaint();
    };

    resized();
    repaint();
}

void SideDuckerAudioProcessorEditor::timerCallback()
{
    float gr = processorRef.getDuckingProcessor().getCurrentGainReduction();
    amountSlider.setDynamicGainReduction(gr);
}

void SideDuckerAudioProcessorEditor::paint(juce::Graphics& g)
{
    // M3 Base Surface
    g.fillAll(CustomLookAndFeel::m3Surface);

    // App Bar Header
    g.setColour(CustomLookAndFeel::m3Surface);
    g.fillRect(0, 0, getWidth(), 46);

    // Logo & Title
    g.setColour(CustomLookAndFeel::m3Primary);
    g.fillEllipse(18.0f, 15.0f, 16.0f, 16.0f);
    g.setColour(CustomLookAndFeel::m3Surface);
    g.fillEllipse(22.0f, 19.0f, 8.0f, 8.0f);

    g.setColour(CustomLookAndFeel::m3OnSurface);
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("SideDucker Pro", 42, 11, 130, 24, juce::Justification::centredLeft);

    // 1. Compact Band Control Strip Card (106px height)
    auto bandArea = juce::Rectangle<int>(16, 290, getWidth() - 32, 106).toFloat();
    g.setColour(CustomLookAndFeel::m3SurfaceContainer);
    g.fillRoundedRectangle(bandArea, 14.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bandArea, 14.0f, 1.0f);

    // Dividers separating Band Card functional zones:
    // Zone 1: Band Selector & Status (x: 16..228)
    // Zone 2: Filter Shape & Placement (x: 228..576)
    // Zone 3: Band Parameters (x: 576..864)
    g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.35f));
    g.drawVerticalLine(228, 298.0f, 388.0f);
    g.drawVerticalLine(576, 298.0f, 388.0f);

    // 2. Bottom Master Card (190px height)
    auto bottomArea = juce::Rectangle<int>(16, 404, getWidth() - 32, 190).toFloat();
    g.setColour(CustomLookAndFeel::m3SurfaceContainer);
    g.fillRoundedRectangle(bottomArea, 16.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bottomArea, 16.0f, 1.0f);

    // Dividers in Master Card
    g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.4f));
    g.drawVerticalLine(198, 416.0f, 584.0f);
    g.drawVerticalLine(362, 416.0f, 584.0f);
    g.drawVerticalLine(630, 416.0f, 584.0f);

    // Section Labels in Master Card
    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.7f));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("ENVELOPE", 32, 412, 140, 16, juce::Justification::centredLeft);
    g.drawText("MASTER CARVE-OUT", 208, 412, 144, 16, juce::Justification::centred);

    // Stereo Header
    bool isStereo = processorRef.getDuckingProcessor().isStereoInstance();
    g.setColour(isStereo ? CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.7f) : CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.3f));
    g.drawText("STEREO", 380, 412, 70, 16, juce::Justification::centredLeft);
    g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(isStereo ? 0.4f : 0.15f));
    g.drawHorizontalLine(420, 436.0f, 615.0f);

    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.7f));
    g.drawText("SHARPNESS & TRIM", 645, 412, 195, 16, juce::Justification::centred);
}

void SideDuckerAudioProcessorEditor::resized()
{
    // 1. Top Header Action Bar (y = 9, h = 28)
    int headerBtnY = 9;

    eqPresetLabel.setBounds(180, headerBtnY + 5, 55, 18);
    eqPresetComboBox.setBounds(240, headerBtnY, 230, 28);
    resetEqButton.setBounds(478, headerBtnY, 80, 28);

    scListenButton.setBounds(getWidth() - 208, headerBtnY, 96, 28);
    bypassButton.setBounds(getWidth() - 104, headerBtnY, 88, 28);

    // 2. Visualizer Graph Section (Expanded height: 236px)
    auto visualizerBounds = juce::Rectangle<int>(16, 48, getWidth() - 32, 236);
    visualizer.setBounds(visualizerBounds);

    // 3. Compact Band Control Strip Card: y = 290, height = 106
    int bandCardY = 290;

    // --- Zone 1: Band Selection & State (x = 28 .. 220) ---
    int pillStartX = 28;
    int pillY = bandCardY + 12;
    int pillW = 24;
    int pillH = 24;
    for (int i = 0; i < 6; ++i)
    {
        bandSelectButtons[i].setBounds(pillStartX + i * (pillW + 7), pillY, pillW, pillH);
    }

    bandTitleLabel.setBounds(28, bandCardY + 44, 186, 20);
    bandSoloButton.setBounds(28, bandCardY + 68, 88, 24);
    bandBypassButton.setBounds(122, bandCardY + 68, 92, 24);

    // --- Zone 2: Filter Shape, Slope & Placement (x = 242 .. 565) ---
    int ctrlLabelY = bandCardY + 20;
    int ctrlComboY = bandCardY + 38;
    bool hasSlope = bandSlopeComboBox.isVisible();

    bandShapeLabel.setBounds(246, ctrlLabelY, 100, 14);
    bandShapeComboBox.setBounds(246, ctrlComboY, 100, 28);

    if (hasSlope)
    {
        bandSlopeLabel.setBounds(356, ctrlLabelY, 96, 14);
        bandSlopeComboBox.setBounds(356, ctrlComboY, 96, 28);

        bandStereoLabel.setBounds(462, ctrlLabelY, 104, 14);
        bandStereoComboBox.setBounds(462, ctrlComboY, 104, 28);
    }
    else
    {
        bandStereoLabel.setBounds(356, ctrlLabelY, 110, 14);
        bandStereoComboBox.setBounds(356, ctrlComboY, 110, 28);
    }

    // --- Zone 3: Band Parameters (x = 590 .. 848) ---
    int dialW = 72;
    int dialH = 66;
    int dialLabelY = bandCardY + 8;
    int dialY = bandCardY + 24;

    bandFreqLabel.setBounds(592, dialLabelY, dialW, 14);
    bandFreqSlider.setBounds(592, dialY, dialW, dialH);

    bandGainLabel.setBounds(676, dialLabelY, dialW, 14);
    bandGainSlider.setBounds(676, dialY, dialW, dialH);

    bandQLabel.setBounds(760, dialLabelY, dialW, 14);
    bandQSlider.setBounds(760, dialY, dialW, dialH);

    // 4. Master Card: y = 404, height = 190
    int masterCardY = 404;

    // --- Section 1: Envelope (Attack & Release) (x = 24..190) ---
    int envDialW = 68;
    int envDialH = 68;
    int envLabelY = masterCardY + 48;
    int envStartY = masterCardY + 68;

    attackLabel.setBounds(34, envLabelY, envDialW, 14);
    attackSlider.setBounds(34, envStartY, envDialW, envDialH);

    releaseLabel.setBounds(112, envLabelY, envDialW, 14);
    releaseSlider.setBounds(112, envStartY, envDialW, envDialH);

    // --- Section 2: Master Hero Amount Knob (x = 205..355) ---
    int knobSize = 116;
    amountSlider.setBounds(224, masterCardY + 44, knobSize, knobSize);

    // --- Section 3: Master Stereo Section (x = 370..620) ---
    int stereoLeftX = 378;
    int stereoRightX = 498;

    // Left Column: mode + link
    stereoModeLabel.setBounds(stereoLeftX, envLabelY, 105, 14);
    stereoModeButton.setBounds(stereoLeftX, envLabelY + 18, 105, 26);

    stereoLinkLabel.setBounds(stereoLeftX, masterCardY + 104, 105, 14);
    stereoLinkSlider.setBounds(stereoLeftX, masterCardY + 122, 105, 26);

    // Right Column: balance + dial + even badge
    stereoBalanceLabel.setBounds(stereoRightX, envLabelY, 105, 14);
    stereoBalanceSlider.setBounds(stereoRightX + 26, envLabelY + 16, 52, 52);
    stereoBalanceValueLabel.setBounds(stereoRightX, masterCardY + 122, 105, 24);

    // --- Section 4: Sharpness & Trim (x = 640..848) ---
    spectralBandsLabel.setBounds(656, envLabelY, envDialW, 14);
    spectralBandsSlider.setBounds(656, envStartY, envDialW, envDialH);

    outputGainLabel.setBounds(748, envLabelY, envDialW, 14);
    outputGainSlider.setBounds(748, envStartY, envDialW, envDialH);
}
