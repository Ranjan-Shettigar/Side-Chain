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

    // Master Controls
    setupRotaryControl(attackSlider, attackLabel, "ATTACK", "");
    attackSlider.setName("Attack");
    attackAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "attack", attackSlider);

    setupRotaryControl(releaseSlider, releaseLabel, "RELEASE", "");
    releaseSlider.setName("Release");
    releaseAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "release", releaseSlider);

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
    bandTitleLabel.setFont(juce::Font(13.0f, juce::Font::bold));
    bandTitleLabel.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurface);
    addAndMakeVisible(bandTitleLabel);

    addAndMakeVisible(bandShapeComboBox);
    addAndMakeVisible(bandSlopeComboBox);
    addAndMakeVisible(bandStereoComboBox);

    setupRotaryControl(bandFreqSlider, bandFreqLabel, "FREQ", "");
    setupRotaryControl(bandGainSlider, bandGainLabel, "GAIN", "");
    setupRotaryControl(bandQSlider, bandQLabel, "Q", "");

    bandSoloButton.setButtonText("SOLO BAND");
    bandSoloButton.setName("solo");
    addAndMakeVisible(bandSoloButton);

    bandBypassButton.setButtonText("BYPASS");
    bandBypassButton.setName("bandBypass");
    addAndMakeVisible(bandBypassButton);

    // Initial Band Selection (Band 1: Cyan)
    updateSelectedBandUI(0);

    // Spacious Dimensions
    setSize(820, 670);
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
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 18);
    slider.setTextValueSuffix(suffix);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(11.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, CustomLookAndFeel::m3OnSurfaceVariant);
    addAndMakeVisible(label);
}

void SideDuckerAudioProcessorEditor::updateSelectedBandUI(int bandIndex)
{
    currentSelectedBand = juce::jlimit(0, 5, bandIndex);

    // Update active pill button toggle states
    for (int i = 0; i < 6; ++i)
        bandSelectButtons[i].setToggleState(i == currentSelectedBand, juce::dontSendNotification);

    // Release existing attachments
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

    // Setup Shape Dropdown
    bandShapeComboBox.clear(juce::dontSendNotification);
    bandShapeComboBox.addItemList({ "Bell", "Low Cut", "High Cut", "Low Shelf", "High Shelf", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt" }, 1);
    selectedShapeAttachment = std::make_unique<ComboBoxAttachment>(apvts, prefix + "shape", bandShapeComboBox);

    // Setup Slope Dropdown
    bandSlopeComboBox.clear(juce::dontSendNotification);
    bandSlopeComboBox.addItemList({ "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct", "Brickwall" }, 1);
    selectedSlopeAttachment = std::make_unique<ComboBoxAttachment>(apvts, prefix + "slope", bandSlopeComboBox);

    // Setup Stereo Placement Dropdown
    bandStereoComboBox.clear(juce::dontSendNotification);
    bandStereoComboBox.addItemList({ "Stereo", "Left", "Right", "Mid", "Side" }, 1);
    selectedStereoAttachment = std::make_unique<ComboBoxAttachment>(apvts, prefix + "stereo", bandStereoComboBox);

    // Attach Freq, Gain, Q, Bypass, Solo
    selectedFreqAttachment = std::make_unique<SliderAttachment>(apvts, prefix + "freq", bandFreqSlider);
    selectedGainAttachment = std::make_unique<SliderAttachment>(apvts, prefix + "gain", bandGainSlider);
    selectedQAttachment    = std::make_unique<SliderAttachment>(apvts, prefix + "q", bandQSlider);
    selectedBypassAttachment = std::make_unique<ButtonAttachment>(apvts, prefix + "bypass", bandBypassButton);
    selectedSoloAttachment   = std::make_unique<ButtonAttachment>(apvts, prefix + "solo", bandSoloButton);

    int curShape = 0;
    if (auto* pS = apvts.getRawParameterValue(prefix + "shape"))
        curShape = static_cast<int>(pS->load());

    // Show/Hide Slope and Gain based on Shape
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

    // 1. Band Control Strip Card
    auto bandArea = juce::Rectangle<int>(16, 290, getWidth() - 32, 142).toFloat();
    g.setColour(CustomLookAndFeel::m3SurfaceContainer);
    g.fillRoundedRectangle(bandArea, 16.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bandArea, 16.0f, 1.0f);

    // 2. Bottom Master Card
    auto bottomArea = juce::Rectangle<int>(16, 444, getWidth() - 32, 176).toFloat();
    g.setColour(CustomLookAndFeel::m3SurfaceContainer);
    g.fillRoundedRectangle(bottomArea, 16.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bottomArea, 16.0f, 1.0f);

    // Dividers in Master Card
    g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.4f));
    g.drawVerticalLine(getWidth() / 2 - 100, 456.0f, 608.0f);
    g.drawVerticalLine(getWidth() / 2 + 100, 456.0f, 608.0f);

    // Section Labels in Master Card
    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.7f));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("ENVELOPE", 32, 452, 140, 16, juce::Justification::centredLeft);
    g.drawText("MASTER CARVE-OUT", getWidth() / 2 - 70, 452, 140, 16, juce::Justification::centred);
    g.drawText("OUTPUT TRIM", getWidth() - 170, 452, 138, 16, juce::Justification::centred);

    // Footer Bar
    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.5f));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    g.drawText("48-Band High Resolution Spectral Engine • Pro-Q 4 Mode", 18, getHeight() - 24, 340, 18, juce::Justification::centredLeft);

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

    // Visualizer Graph Section
    visualizer.setBounds(16, 48, getWidth() - 32, 230);

    // Band Control Strip Card: y = 290, height = 142
    int bandCardY = 290;

    // Row 1 of Band Card (Pills & Title & Toggles)
    int pillStartX = 30;
    int pillY = bandCardY + 12;
    int pillW = 32;
    int pillH = 26;
    for (int i = 0; i < 6; ++i)
    {
        bandSelectButtons[i].setBounds(pillStartX + i * (pillW + 6), pillY, pillW, pillH);
    }

    bandTitleLabel.setBounds(pillStartX + 6 * (pillW + 6) + 12, pillY + 2, 200, 22);

    bandBypassButton.setBounds(getWidth() - 125, pillY, 95, 26);
    bandSoloButton.setBounds(getWidth() - 230, pillY, 95, 26);

    // Row 2 of Band Card (Controls): y = 336
    int ctrlY = bandCardY + 48;
    int startComboX = 30;

    bandShapeComboBox.setBounds(startComboX, ctrlY + 22, 110, 26);
    bandSlopeComboBox.setBounds(startComboX + 118, ctrlY + 22, 100, 26);
    bandStereoComboBox.setBounds(startComboX + (bandSlopeComboBox.isVisible() ? 226 : 118), ctrlY + 22, 85, 26);

    // Dials
    int sliderW = 72;
    int sliderH = 68;
    int startDialX = bandSlopeComboBox.isVisible() ? 440 : 360;

    // Freq
    bandFreqLabel.setBounds(startDialX, ctrlY + 2, sliderW, 14);
    bandFreqSlider.setBounds(startDialX, ctrlY + 18, sliderW, sliderH);

    // Gain
    int gainX = startDialX + 90;
    bandGainLabel.setBounds(gainX, ctrlY + 2, sliderW, 14);
    bandGainSlider.setBounds(gainX, ctrlY + 18, sliderW, sliderH);

    // Q
    int qX = gainX + (bandGainSlider.isVisible() ? 90 : 0);
    bandQLabel.setBounds(qX, ctrlY + 2, sliderW, 14);
    bandQSlider.setBounds(qX, ctrlY + 18, sliderW, sliderH);

    // Master Card: y = 444, height = 176
    int masterCardY = 444;

    // Master Hero Amount Knob (Center)
    int knobSize = 118;
    int centerX = getWidth() / 2;
    amountSlider.setBounds(centerX - knobSize / 2, masterCardY + 44, knobSize, knobSize);

    // Master Envelope (Left)
    int envDialW = 72;
    int envDialH = 68;
    int envStartY = masterCardY + 86;

    attackLabel.setBounds(40, masterCardY + 68, envDialW, 14);
    attackSlider.setBounds(40, envStartY, envDialW, envDialH);

    releaseLabel.setBounds(132, masterCardY + 68, envDialW, 14);
    releaseSlider.setBounds(132, envStartY, envDialW, envDialH);

    // Master Output Trim (Right)
    int trimX = getWidth() - 140;
    outputGainLabel.setBounds(trimX, masterCardY + 68, envDialW, 14);
    outputGainSlider.setBounds(trimX, envStartY, envDialW, envDialH);
}
