#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/CustomEnvelope.h"

class SideDuckerAudioProcessor;

/**
 * FabFilter / VolumeShaper Inspired Interactive 48-Point Envelope Curve Editor
 */
class EnvelopeEditor : public juce::Component, public juce::Timer
{
public:
    explicit EnvelopeEditor(SideDuckerAudioProcessor& processor);
    ~EnvelopeEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void syncWithProcessor();

private:
    SideDuckerAudioProcessor& processorRef;
    CustomEnvelope& envelopeRef;

    // Controls
    juce::ComboBox rateComboBox;
    juce::ComboBox snapComboBox;
    juce::ComboBox presetComboBox;
    juce::TextButton clearButton;

    juce::Label rateLabel;
    juce::Label snapLabel;
    juce::Label presetLabel;

    int draggedPointIndex = -1;
    int hoveredPointIndex = -1;
    int hoveredSegmentIndex = -1;

    juce::Point<float> pointToScreen(const CustomEnvelope::Point& p, const juce::Rectangle<float>& area) const;
    CustomEnvelope::Point screenToPoint(juce::Point<float> pos, const juce::Rectangle<float>& area) const;
    int findPointAtPosition(juce::Point<float> pos, const juce::Rectangle<float>& area) const;
    int findSegmentAtPosition(juce::Point<float> pos, const juce::Rectangle<float>& area) const;

    float applySnap(float x) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeEditor)
};
