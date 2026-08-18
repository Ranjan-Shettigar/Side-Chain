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
    stereoLinkSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 24);
    stereoLinkSlider.setTextValueSuffix("");
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

    addAndMakeVisible(bandShapeComboBox);
    addAndMakeVisible(bandSlopeComboBox);
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

    // Initial Band Selection (Band 1: Cyan)
    updateSelectedBandUI(0);
    updateStereoSectionUI();

    // Dimensions: 880 x 640
    setSize(880, 640);
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
    bandGainSlider.setVisible(!isCut);
    bandGainLabel.setVisible(!isCut);

    bandShapeComboBox.onChange = [this]() {
        int shape = bandShapeComboBox.getSelectedId() - 1;
        bool cut = (shape == 1 || shape == 2);
        bandSlopeComboBox.setVisible(cut);
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
    g.drawText("SideDucker Pro", 42, 11, 140, 24, juce::Justification::centredLeft);

    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.65f));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    g.drawText("48-BAND DYNAMIC SPECTRAL DUCKER", 152, 13, 240, 20, juce::Justification::centredLeft);

    // 1. Compact Band Control Strip Card (106px height)
    auto bandArea = juce::Rectangle<int>(16, 290, getWidth() - 32, 106).toFloat();
    g.setColour(CustomLookAndFeel::m3SurfaceContainer);
    g.fillRoundedRectangle(bandArea, 14.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bandArea, 14.0f, 1.0f);

    // 2. Bottom Master Card (190px height)
    auto bottomArea = juce::Rectangle<int>(16, 404, getWidth() - 32, 190).toFloat();
    g.setColour(CustomLookAndFeel::m3SurfaceContainer);
    g.fillRoundedRectangle(bottomArea, 16.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bottomArea, 16.0f, 1.0f);

    // Dividers in Master Card
    g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.4f));
    g.drawVerticalLine(205, 416.0f, 584.0f);
    g.drawVerticalLine(380, 416.0f, 584.0f);
    g.drawVerticalLine(680, 416.0f, 584.0f);

    // Section Labels in Master Card
    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.7f));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("ENVELOPE", 32, 412, 140, 16, juce::Justification::centredLeft);
    g.drawText("MASTER CARVE-OUT", 220, 412, 140, 16, juce::Justification::centred);

    // Stereo Header
    bool isStereo = processorRef.getDuckingProcessor().isStereoInstance();
    g.setColour(isStereo ? CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.7f) : CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.3f));
    g.drawText("STEREO", 400, 412, 70, 16, juce::Justification::centredLeft);
    g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(isStereo ? 0.4f : 0.15f));
    g.drawHorizontalLine(420, 456.0f, 665.0f);

    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.7f));
    g.drawText("OUTPUT TRIM", 700, 412, 138, 16, juce::Justification::centred);

    // Footer Bar
    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.5f));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    g.drawText("48-Band High Resolution Spectral Engine | Pro-Q 4 Mode", 18, getHeight() - 24, 340, 18, juce::Justification::centredLeft);

    float scLevel = processorRef.getDuckingProcessor().getCurrentSidechainLevel();
    bool scActive = scLevel > 0.005f;

    g.setColour(scActive ? CustomLookAndFeel::m3Secondary : CustomLookAndFeel::m3Outline);
    g.fillEllipse(static_cast<float>(getWidth() - 120), static_cast<float>(getHeight() - 17), 6.0f, 6.0f);

    g.setColour(scActive ? CustomLookAndFeel::m3OnSurface : CustomLookAndFeel::m3OnSurfaceVariant);
    g.drawText(scActive ? "SC Active" : "SC Idle", getWidth() - 108, getHeight() - 24, 90, 18, juce::Justification::centredLeft);
}

void SideDuckerAudioProcessorEditor::resized()
{
    // Header Action Chips
    bypassButton.setBounds(getWidth() - 106, 9, 90, 28);
    scListenButton.setBounds(getWidth() - 212, 9, 98, 28);

    // Visualizer Graph Section (Expanded height: 236px)
    visualizer.setBounds(16, 48, getWidth() - 32, 236);

    // Compact Band Control Strip Card: y = 290, height = 106
    int bandCardY = 290;

    // Row 1 of Band Card (Pills & Title & Toggles: y = 298, height = 22)
    int pillStartX = 28;
    int pillY = bandCardY + 8;
    int pillW = 28;
    int pillH = 22;
    for (int i = 0; i < 6; ++i)
    {
        bandSelectButtons[i].setBounds(pillStartX + i * (pillW + 4), pillY, pillW, pillH);
    }

    bandTitleLabel.setBounds(pillStartX + 6 * (pillW + 4) + 8, pillY, 180, 22);

    bandBypassButton.setBounds(getWidth() - 110, pillY, 82, 22);
    bandSoloButton.setBounds(getWidth() - 198, pillY, 82, 22);

    // Row 2 of Band Card (Controls: y = 330, height = 58)
    int ctrlY = bandCardY + 36;
    int startComboX = 28;

    bandShapeComboBox.setBounds(startComboX, ctrlY + 16, 105, 26);
    bandSlopeComboBox.setBounds(startComboX + 112, ctrlY + 16, 88, 26);
    bandStereoComboBox.setBounds(startComboX + (bandSlopeComboBox.isVisible() ? 206 : 112), ctrlY + 16, 80, 26);

    // Compact Rotary Dials (No overlap, clean hierarchy)
    int sliderW = 58;
    int sliderH = 52;
    int startDialX = bandSlopeComboBox.isVisible() ? 440 : 350;
    int dialSpacing = 85;

    bandFreqLabel.setBounds(startDialX, ctrlY + 2, sliderW, 12);
    bandFreqSlider.setBounds(startDialX, ctrlY + 14, sliderW, sliderH);

    int gainX = startDialX + dialSpacing;
    bandGainLabel.setBounds(gainX, ctrlY + 2, sliderW, 12);
    bandGainSlider.setBounds(gainX, ctrlY + 14, sliderW, sliderH);

    int qX = gainX + (bandGainSlider.isVisible() ? dialSpacing : 0);
    bandQLabel.setBounds(qX, ctrlY + 2, sliderW, 12);
    bandQSlider.setBounds(qX, ctrlY + 14, sliderW, sliderH);

    // Master Card: y = 404, height = 190
    int masterCardY = 404;

    // 1. Envelope Section (Left: x = 24..190)
    int envDialW = 72;
    int envDialH = 68;
    int envStartY = masterCardY + 74;

    attackLabel.setBounds(34, masterCardY + 54, envDialW, 14);
    attackSlider.setBounds(34, envStartY, envDialW, envDialH);

    releaseLabel.setBounds(116, masterCardY + 54, envDialW, 14);
    releaseSlider.setBounds(116, envStartY, envDialW, envDialH);

    // 2. Master Hero Amount Knob (Center-Left: x = 210..370)
    int knobSize = 120;
    amountSlider.setBounds(230, masterCardY + 44, knobSize, knobSize);

    // 3. Master Stereo Section (Center-Right: x = 395..670)
    int stereoLeftX = 395;
    int stereoRightX = 525;

    // Left Column: mode + link
    stereoModeLabel.setBounds(stereoLeftX, masterCardY + 54, 110, 14);
    stereoModeButton.setBounds(stereoLeftX, masterCardY + 72, 110, 26);

    stereoLinkLabel.setBounds(stereoLeftX, masterCardY + 106, 110, 14);
    stereoLinkSlider.setBounds(stereoLeftX + 5, masterCardY + 124, 100, 26);

    // Right Column: balance + dial + even badge
    stereoBalanceLabel.setBounds(stereoRightX, masterCardY + 52, 120, 14);
    stereoBalanceSlider.setBounds(stereoRightX + 34, masterCardY + 70, 52, 52);
    stereoBalanceValueLabel.setBounds(stereoRightX + 5, masterCardY + 126, 110, 24);

    // 4. Master Output Trim (Right: x = 700..840)
    int trimX = 725;
    outputGainLabel.setBounds(trimX, masterCardY + 54, envDialW, 14);
    outputGainSlider.setBounds(trimX, envStartY, envDialW, envDialH);
}
