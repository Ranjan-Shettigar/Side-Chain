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
    setupRotaryControl(attackSlider, attackLabel, "ATTACK", " ms");
    attackSlider.setName("Attack");
    attackAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "attack", attackSlider);

    setupRotaryControl(releaseSlider, releaseLabel, "RELEASE", " ms");
    releaseSlider.setName("Release");
    releaseAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "release", releaseSlider);

    setupRotaryControl(outputGainSlider, outputGainLabel, "TRIM", " dB");
    outputGainSlider.setName("Output Gain");
    outputGainAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "outputGain", outputGainSlider);

    // Master Toggles (M3 Pill Chips)
    scListenButton.setButtonText("SC LISTEN");
    scListenButton.setName("listen");
    addAndMakeVisible(scListenButton);
    scListenAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), "scListen", scListenButton);

    bypassButton.setButtonText("BYPASS");
    bypassButton.setName("bypass");
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), "bypass", bypassButton);

    // Band Selector Pill Buttons (M3 Filter Chips 1 to 6)
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

    addAndMakeVisible(bandTypeComboBox);

    setupRotaryControl(bandFreqSlider, bandFreqLabel, "FREQ", " Hz");
    setupRotaryControl(bandGainSlider, bandGainLabel, "SENSITIVITY", " dB");
    setupRotaryControl(bandQSlider, bandQLabel, "Q / SLOPE", "");

    bandBypassButton.setButtonText("BYPASS BAND");
    bandBypassButton.setName("bandBypass");
    addAndMakeVisible(bandBypassButton);

    // Initial Band Selection (Band 2: Yellow GP1)
    updateSelectedBandUI(1);

    // Dimensions
    setSize(740, 570);
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
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 18);
    slider.setTextValueSuffix(suffix);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(10.0f, juce::Font::bold));
    addAndMakeVisible(label);
}

void SideDuckerAudioProcessorEditor::updateSelectedBandUI(int bandIndex)
{
    currentSelectedBand = juce::jlimit(0, 5, bandIndex);

    // Update active pill button toggle states
    for (int i = 0; i < 6; ++i)
        bandSelectButtons[i].setToggleState(i == currentSelectedBand, juce::dontSendNotification);

    // Release existing attachments
    selectedTypeAttachment.reset();
    selectedFreqAttachment.reset();
    selectedGainAttachment.reset();
    selectedQAttachment.reset();
    selectedBypassAttachment.reset();

    auto& apvts = processorRef.getAPVTS();
    bandTypeComboBox.clear(juce::dontSendNotification);

    if (currentSelectedBand == 0) // Band 1: Low Cut
    {
        bandTitleLabel.setText("BAND 1: LOW CUT", juce::dontSendNotification);
        bandTitleLabel.setColour(juce::Label::textColourId, DuckingVisualizer::bandColours[0]);

        bandTypeComboBox.addItemList({ "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct" }, 1);
        selectedTypeAttachment = std::make_unique<ComboBoxAttachment>(apvts, "b1_slope", bandTypeComboBox);
        selectedFreqAttachment = std::make_unique<SliderAttachment>(apvts, "b1_freq", bandFreqSlider);
        selectedBypassAttachment = std::make_unique<ButtonAttachment>(apvts, "b1_bypass", bandBypassButton);

        bandGainSlider.setVisible(false);
        bandGainLabel.setVisible(false);
        bandQSlider.setVisible(false);
        bandQLabel.setVisible(false);
    }
    else if (currentSelectedBand == 5) // Band 6: High Cut
    {
        bandTitleLabel.setText("BAND 6: HIGH CUT", juce::dontSendNotification);
        bandTitleLabel.setColour(juce::Label::textColourId, DuckingVisualizer::bandColours[5]);

        bandTypeComboBox.addItemList({ "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct" }, 1);
        selectedTypeAttachment = std::make_unique<ComboBoxAttachment>(apvts, "b6_slope", bandTypeComboBox);
        selectedFreqAttachment = std::make_unique<SliderAttachment>(apvts, "b6_freq", bandFreqSlider);
        selectedBypassAttachment = std::make_unique<ButtonAttachment>(apvts, "b6_bypass", bandBypassButton);

        bandGainSlider.setVisible(false);
        bandGainLabel.setVisible(false);
        bandQSlider.setVisible(false);
        bandQLabel.setVisible(false);
    }
    else // Bands 2-5: General Purpose
    {
        const std::array<juce::String, 4> colorNames = { "AMBER (GP 1)", "PEACH (GP 2)", "ROSE (GP 3)", "LAVENDER (GP 4)" };
        int gpIdx = currentSelectedBand - 1;
        int bNum = currentSelectedBand + 1;
        juce::String prefix = "b" + juce::String(bNum) + "_";

        bandTitleLabel.setText("BAND " + juce::String(bNum) + ": " + colorNames[gpIdx], juce::dontSendNotification);
        bandTitleLabel.setColour(juce::Label::textColourId, DuckingVisualizer::bandColours[currentSelectedBand]);

        bandTypeComboBox.addItemList({ "Bell", "Low Shelf", "High Shelf", "Band Shelf", "Notch", "Tilt" }, 1);
        selectedTypeAttachment = std::make_unique<ComboBoxAttachment>(apvts, prefix + "type", bandTypeComboBox);
        selectedFreqAttachment = std::make_unique<SliderAttachment>(apvts, prefix + "freq", bandFreqSlider);
        selectedGainAttachment = std::make_unique<SliderAttachment>(apvts, prefix + "gain", bandGainSlider);
        selectedQAttachment    = std::make_unique<SliderAttachment>(apvts, prefix + "q", bandQSlider);
        selectedBypassAttachment = std::make_unique<ButtonAttachment>(apvts, prefix + "bypass", bandBypassButton);

        bandGainSlider.setVisible(true);
        bandGainLabel.setVisible(true);
        bandQSlider.setVisible(true);
        bandQLabel.setVisible(true);
    }

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

    // M3 App Bar (Header)
    g.setColour(CustomLookAndFeel::m3Surface);
    g.fillRect(0, 0, getWidth(), 46);

    // M3 Logo & Title
    g.setColour(CustomLookAndFeel::m3Primary);
    g.fillEllipse(18.0f, 15.0f, 16.0f, 16.0f);
    g.setColour(CustomLookAndFeel::m3Surface);
    g.fillEllipse(22.0f, 19.0f, 8.0f, 8.0f);

    g.setColour(CustomLookAndFeel::m3OnSurface);
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawText("SideDucker", 42, 11, 120, 24, juce::Justification::centredLeft);

    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.7f));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    g.drawText("48-BAND DYNAMIC SPECTRAL DUCKER", 132, 13, 230, 20, juce::Justification::centredLeft);

    // 1. Band Control Strip Card (M3 Surface Container with 16px radius)
    auto bandArea = juce::Rectangle<int>(14, 280, getWidth() - 28, 106).toFloat();
    g.setColour(CustomLookAndFeel::m3SurfaceContainer);
    g.fillRoundedRectangle(bandArea, 16.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bandArea, 16.0f, 1.0f);

    // 2. Bottom Master Card (M3 Surface Container with 16px radius)
    auto bottomArea = juce::Rectangle<int>(14, 394, getWidth() - 28, 140).toFloat();
    g.setColour(CustomLookAndFeel::m3SurfaceContainer);
    g.fillRoundedRectangle(bottomArea, 16.0f);
    g.setColour(CustomLookAndFeel::m3OutlineVariant);
    g.drawRoundedRectangle(bottomArea, 16.0f, 1.0f);

    // Section Labels in Master Card (M3 Title Medium style)
    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.7f));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("ENVELOPE", 30, 402, 140, 14, juce::Justification::centredLeft);
    g.drawText("MASTER CARVE-OUT", getWidth() / 2 - 70, 402, 140, 14, juce::Justification::centred);
    g.drawText("OUTPUT TRIM", getWidth() - 175, 402, 145, 14, juce::Justification::centred);

    // Dividers in Master Card
    g.setColour(CustomLookAndFeel::m3OutlineVariant.withAlpha(0.4f));
    g.drawVerticalLine(getWidth() / 2 - 80, 408.0f, 520.0f);
    g.drawVerticalLine(getWidth() / 2 + 80, 408.0f, 520.0f);

    // Footer Bar (M3 Minimalist Caption)
    g.setColour(CustomLookAndFeel::m3OnSurfaceVariant.withAlpha(0.6f));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    g.drawText("48-Band High Resolution Spectral Engine", 18, getHeight() - 22, 300, 16, juce::Justification::centredLeft);

    float scLevel = processorRef.getDuckingProcessor().getCurrentSidechainLevel();
    bool scActive = scLevel > 0.005f;

    g.setColour(scActive ? CustomLookAndFeel::m3Secondary : CustomLookAndFeel::m3Outline);
    g.fillEllipse(static_cast<float>(getWidth() - 120), static_cast<float>(getHeight() - 16), 6.0f, 6.0f);

    g.setColour(scActive ? CustomLookAndFeel::m3OnSurface : CustomLookAndFeel::m3OnSurfaceVariant);
    g.drawText(scActive ? "SC Active" : "SC Idle", getWidth() - 108, getHeight() - 22, 90, 16, juce::Justification::centredLeft);
}

void SideDuckerAudioProcessorEditor::resized()
{
    // Header Action Chips (Pill shaped)
    bypassButton.setBounds(getWidth() - 96, 9, 82, 28);
    scListenButton.setBounds(getWidth() - 198, 9, 94, 28);

    // Visualizer Graph Section (M3 16px rounded canvas)
    visualizer.setBounds(14, 50, getWidth() - 28, 222);

    // Band Selector Pill Chips (1 to 6)
    int pillStartX = 24;
    int pillY = 290;
    int pillW = 28;
    int pillH = 24;
    for (int i = 0; i < 6; ++i)
    {
        bandSelectButtons[i].setBounds(pillStartX + i * (pillW + 4), pillY, pillW, pillH);
    }

    // Band Title Label
    bandTitleLabel.setBounds(pillStartX + 6 * (pillW + 4) + 12, pillY + 2, 220, 20);

    // Band Bypass Toggle
    bandBypassButton.setBounds(getWidth() - 135, pillY, 110, 24);

    // Band Control Dials
    int ctrlY = 320;
    bandTypeComboBox.setBounds(24, ctrlY + 12, 110, 24);

    int dialSize = 52;
    int dialSpacing = 72;
    int startDialX = 160;

    bandFreqSlider.setBounds(startDialX, ctrlY, dialSize, dialSize + 18);
    bandFreqLabel.setBounds(startDialX - 4, ctrlY + dialSize + 16, dialSize + 8, 14);

    bandGainSlider.setBounds(startDialX + dialSpacing, ctrlY, dialSize, dialSize + 18);
    bandGainLabel.setBounds(startDialX + dialSpacing - 10, ctrlY + dialSize + 16, dialSize + 20, 14);

    bandQSlider.setBounds(startDialX + dialSpacing * 2, ctrlY, dialSize, dialSize + 18);
    bandQLabel.setBounds(startDialX + dialSpacing * 2 - 8, ctrlY + dialSize + 16, dialSize + 16, 14);

    // Master Hero Amount Knob (Center)
    int knobSize = 118;
    int centerX = getWidth() / 2;
    amountSlider.setBounds(centerX - knobSize / 2, 408, knobSize, knobSize);

    // Master Envelope (Left)
    int envSize = 54;
    attackSlider.setBounds(36, 424, envSize, envSize + 20);
    attackLabel.setBounds(36, 424 + envSize + 18, envSize, 14);

    releaseSlider.setBounds(116, 424, envSize, envSize + 20);
    releaseLabel.setBounds(116, 424 + envSize + 18, envSize, 14);

    // Master Output Gain (Right)
    outputGainSlider.setBounds(getWidth() - 130, 424, envSize, envSize + 20);
    outputGainLabel.setBounds(getWidth() - 130, 424 + envSize + 18, envSize, 14);
}
