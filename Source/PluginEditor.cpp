#include "PluginEditor.h"

SideDuckerAudioProcessorEditor::SideDuckerAudioProcessorEditor(SideDuckerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef(p),
      visualizer(p.getVisualizerFifo())
{
    setLookAndFeel(&customLookAndFeel);

    // Visualizer
    addAndMakeVisible(visualizer);

    // Hero Amount Knob
    amountSlider.setName("Amount");
    addAndMakeVisible(amountSlider);
    amountAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "amount", amountSlider);

    // Small rotary knobs
    setupRotaryControl(lowCutSlider, lowCutLabel, "LOW CUT", " Hz");
    lowCutSlider.setName("Low Cut");
    lowCutAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "lowCut", lowCutSlider);
    lowCutSlider.onValueChange = [this]() {
        visualizer.setFilterFrequencies(static_cast<float>(lowCutSlider.getValue()),
                                        static_cast<float>(highCutSlider.getValue()));
    };

    setupRotaryControl(highCutSlider, highCutLabel, "HIGH CUT", " Hz");
    highCutSlider.setName("High Cut");
    highCutAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "highCut", highCutSlider);
    highCutSlider.onValueChange = [this]() {
        visualizer.setFilterFrequencies(static_cast<float>(lowCutSlider.getValue()),
                                        static_cast<float>(highCutSlider.getValue()));
    };

    setupRotaryControl(attackSlider, attackLabel, "ATTACK", " ms");
    attackSlider.setName("Attack");
    attackAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "attack", attackSlider);

    setupRotaryControl(releaseSlider, releaseLabel, "RELEASE", " ms");
    releaseSlider.setName("Release");
    releaseAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "release", releaseSlider);

    setupRotaryControl(outputGainSlider, outputGainLabel, "GAIN", " dB");
    outputGainSlider.setName("Output Gain");
    outputGainAttachment = std::make_unique<SliderAttachment>(processorRef.getAPVTS(), "outputGain", outputGainSlider);

    // Toggle Buttons
    scListenButton.setButtonText("SC LISTEN");
    scListenButton.setName("listen");
    addAndMakeVisible(scListenButton);
    scListenAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), "scListen", scListenButton);

    bypassButton.setButtonText("BYPASS");
    bypassButton.setName("bypass");
    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(processorRef.getAPVTS(), "bypass", bypassButton);

    // Set initial filter frequencies on visualizer
    visualizer.setFilterFrequencies(static_cast<float>(lowCutSlider.getValue()),
                                    static_cast<float>(highCutSlider.getValue()));

    // Window dimensions
    setSize(680, 480);
    setResizable(false, false);

    startTimerHz(60); // 60 FPS update for knob telemetry
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
    label.setFont(juce::Font(11.0f, juce::Font::bold));
    addAndMakeVisible(label);
}

void SideDuckerAudioProcessorEditor::timerCallback()
{
    float gr = processorRef.getDuckingProcessor().getCurrentGainReduction();
    amountSlider.setDynamicGainReduction(gr);
}

void SideDuckerAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Background gradient
    juce::ColourGradient bgGradient(CustomLookAndFeel::bgDark, 0.0f, 0.0f,
                                    juce::Colour::fromRGB(9, 11, 15), 0.0f, static_cast<float>(getHeight()), false);
    g.setGradientFill(bgGradient);
    g.fillAll();

    // Header Background Bar
    g.setColour(CustomLookAndFeel::bgCard.withAlpha(0.7f));
    g.fillRect(0, 0, getWidth(), 48);

    g.setColour(CustomLookAndFeel::borderSubtle);
    g.drawHorizontalLine(48, 0.0f, static_cast<float>(getWidth()));

    // Plugin Logo & Title
    g.setColour(CustomLookAndFeel::accentCyan);
    g.fillEllipse(18.0f, 16.0f, 16.0f, 16.0f);
    g.setColour(CustomLookAndFeel::bgDark);
    g.fillEllipse(22.0f, 20.0f, 8.0f, 8.0f);

    g.setColour(CustomLookAndFeel::textPrimary);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("SIDE DUCKER", 42, 12, 140, 24, juce::Justification::centredLeft);

    g.setColour(CustomLookAndFeel::textSecondary);
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    g.drawText("SPECTRAL DYNAMIC DUCKING", 165, 15, 200, 20, juce::Justification::centredLeft);

    // Panel separator cards
    auto bottomArea = juce::Rectangle<int>(14, 260, getWidth() - 28, 175).toFloat();
    g.setColour(CustomLookAndFeel::bgCard.withAlpha(0.6f));
    g.fillRoundedRectangle(bottomArea, 10.0f);
    g.setColour(CustomLookAndFeel::borderSubtle);
    g.drawRoundedRectangle(bottomArea, 10.0f, 1.0f);

    // Section Labels in bottom card
    g.setColour(CustomLookAndFeel::textSecondary.withAlpha(0.5f));
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("TRIGGER FILTER", 30, 268, 140, 14, juce::Justification::centred);
    g.drawText("DUCKING DEPTH", (getWidth() - 140) / 2, 268, 140, 14, juce::Justification::centred);
    g.drawText("ENVELOPE & TRIM", getWidth() - 200, 268, 170, 14, juce::Justification::centred);

    // Subtle vertical panel dividers
    g.setColour(CustomLookAndFeel::borderSubtle.withAlpha(0.4f));
    g.drawVerticalLine(190, 275.0f, 425.0f);
    g.drawVerticalLine(getWidth() - 190, 275.0f, 425.0f);

    // Footer Bar
    g.setColour(CustomLookAndFeel::borderSubtle.withAlpha(0.5f));
    g.drawHorizontalLine(getHeight() - 28, 0.0f, static_cast<float>(getWidth()));

    g.setColour(CustomLookAndFeel::textSecondary.withAlpha(0.6f));
    g.setFont(juce::Font(10.0f, juce::Font::plain));
    g.drawText("SIDECHAIN INPUT: 4-CHANNEL DUAL STEREO BUS", 18, getHeight() - 24, 300, 18, juce::Justification::centredLeft);

    float scLevel = processorRef.getDuckingProcessor().getCurrentSidechainLevel();
    bool scActive = scLevel > 0.005f;

    g.setColour(scActive ? CustomLookAndFeel::accentAmber : juce::Colour::fromRGB(70, 80, 90));
    g.fillEllipse(static_cast<float>(getWidth() - 140), static_cast<float>(getHeight() - 18), 7.0f, 7.0f);

    g.setColour(scActive ? CustomLookAndFeel::textPrimary : CustomLookAndFeel::textSecondary);
    g.drawText(scActive ? "SC ACTIVE" : "SC IDLE", getWidth() - 126, getHeight() - 24, 110, 18, juce::Justification::centredLeft);
}

void SideDuckerAudioProcessorEditor::resized()
{
    // Header Buttons
    bypassButton.setBounds(getWidth() - 105, 10, 90, 28);
    scListenButton.setBounds(getWidth() - 215, 10, 100, 28);

    // Visualizer Area
    visualizer.setBounds(14, 56, getWidth() - 28, 192);

    // Hero Amount Knob (Center)
    int knobSize = 135;
    int centerX = getWidth() / 2;
    amountSlider.setBounds(centerX - knobSize / 2, 288, knobSize, knobSize);

    // Left Panel: Sidechain Filters (Low Cut & High Cut)
    int leftSliderSize = 65;
    int leftY = 300;
    lowCutSlider.setBounds(28, leftY, leftSliderSize, leftSliderSize + 22);
    lowCutLabel.setBounds(28, leftY + leftSliderSize + 18, leftSliderSize, 16);

    highCutSlider.setBounds(108, leftY, leftSliderSize, leftSliderSize + 22);
    highCutLabel.setBounds(108, leftY + leftSliderSize + 18, leftSliderSize, 16);

    // Right Panel: Attack, Release, Gain
    int rightSliderSize = 58;
    int rightY = 300;
    int rightStartX = getWidth() - 182;

    attackSlider.setBounds(rightStartX, rightY, rightSliderSize, rightSliderSize + 20);
    attackLabel.setBounds(rightStartX, rightY + rightSliderSize + 18, rightSliderSize, 16);

    releaseSlider.setBounds(rightStartX + 58, rightY, rightSliderSize, rightSliderSize + 20);
    releaseLabel.setBounds(rightStartX + 58, rightY + rightSliderSize + 18, rightSliderSize, 16);

    outputGainSlider.setBounds(rightStartX + 116, rightY, rightSliderSize, rightSliderSize + 20);
    outputGainLabel.setBounds(rightStartX + 116, rightY + rightSliderSize + 18, rightSliderSize, 16);
}
