#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <mutex>
#include <atomic>

/**
 * 48-Point Custom Envelope / LFO Curve Engine with Host BPM Synchronization
 */
class CustomEnvelope
{
public:
    static constexpr int MaxPoints = 48;

    enum class Rate
    {
        Rate1_2 = 0,
        Rate1_4,
        Rate1_8,
        Rate1_16,
        Rate1_32,
        Rate1_2T,
        Rate1_4T,
        Rate1_8T,
        Rate1_16T,
        Rate1_32T,
        Rate1_2D,
        Rate1_4D,
        Rate1_8D,
        Rate1_16D
    };

    struct Point
    {
        float x = 0.0f;       // 0.0 to 1.0 (time within cycle)
        float y = 0.0f;       // 0.0 (no ducking) to 1.0 (full ducking)
        float tension = 0.0f; // -1.0 (concave / slow start) to +1.0 (convex / fast start), 0.0 = linear
    };

    CustomEnvelope();
    ~CustomEnvelope() = default;

    void reset();
    void setSampleRate(double newSampleRate);
    void setRate(Rate newRate);

    // Point Management (Thread-safe)
    int getNumPoints() const;
    Point getPoint(int index) const;
    std::vector<Point> getAllPoints() const;
    void setPoints(const std::vector<Point>& newPoints);
    bool addPoint(float x, float y, float tension = 0.0f);
    bool removePoint(int index);
    bool updatePoint(int index, float newX, float newY, float newTension = 0.0f);

    // Presets
    void loadPresetClassicPump();
    void loadPresetKickDip();
    void loadPresetGate16th();
    void loadPresetSawRamp();
    void loadPresetSmoothSwell();

    // DSP processing
    float getNextSample(juce::AudioPlayHead* playHead);
    float evaluateAt(float phase) const;

    // Real-time playhead telemetry for UI (0.0 to 1.0)
    float getCurrentPhase() const noexcept { return currentPhase.load(); }

    // XML Serialization
    juce::String toXmlString() const;
    void fromXmlString(const juce::String& xmlString);

    static float getBeatsForRate(Rate rate);

private:
    double sampleRate = 44100.0;
    Rate currentRate = Rate::Rate1_4;

    std::vector<Point> points;
    mutable std::mutex pointMutex;

    double internalPhase = 0.0;
    std::atomic<float> currentPhase { 0.0f };

    void sortAndClampPoints();
    void ensureEndpoints();
};
