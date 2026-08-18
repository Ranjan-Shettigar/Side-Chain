#include "CustomEnvelope.h"
#include <algorithm>
#include <cmath>

CustomEnvelope::CustomEnvelope()
{
    loadPresetClassicPump();
}

void CustomEnvelope::reset()
{
    internalPhase = 0.0;
    currentPhase.store(0.0f);
}

void CustomEnvelope::setSampleRate(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
}

void CustomEnvelope::setRate(Rate newRate)
{
    currentRate = newRate;
}

float CustomEnvelope::getBeatsForRate(Rate rate)
{
    switch (rate)
    {
        case Rate::Rate1_2:   return 2.0f;
        case Rate::Rate1_4:   return 1.0f;
        case Rate::Rate1_8:   return 0.5f;
        case Rate::Rate1_16:  return 0.25f;
        case Rate::Rate1_32:  return 0.125f;

        case Rate::Rate1_2T:  return 2.0f * (2.0f / 3.0f);
        case Rate::Rate1_4T:  return 1.0f * (2.0f / 3.0f);
        case Rate::Rate1_8T:  return 0.5f * (2.0f / 3.0f);
        case Rate::Rate1_16T: return 0.25f * (2.0f / 3.0f);
        case Rate::Rate1_32T: return 0.125f * (2.0f / 3.0f);

        case Rate::Rate1_2D:  return 2.0f * 1.5f;
        case Rate::Rate1_4D:  return 1.0f * 1.5f;
        case Rate::Rate1_8D:  return 0.5f * 1.5f;
        case Rate::Rate1_16D: return 0.25f * 1.5f;
    }
    return 1.0f;
}

void CustomEnvelope::ensureEndpoints()
{
    if (points.empty())
    {
        points.push_back({ 0.0f, 0.0f, 0.0f });
        points.push_back({ 1.0f, 1.0f, 0.0f });
        return;
    }

    if (points.front().x > 0.001f)
        points.insert(points.begin(), { 0.0f, points.front().y, 0.0f });
    else
        points.front().x = 0.0f;

    if (points.back().x < 0.999f)
        points.push_back({ 1.0f, points.back().y, 0.0f });
    else
        points.back().x = 1.0f;
}

void CustomEnvelope::sortAndClampPoints()
{
    for (auto& p : points)
    {
        p.x = juce::jlimit(0.0f, 1.0f, p.x);
        p.y = juce::jlimit(0.0f, 1.0f, p.y);
        p.tension = juce::jlimit(-1.0f, 1.0f, p.tension);
    }

    std::sort(points.begin(), points.end(), [](const Point& a, const Point& b) {
        return a.x < b.x;
    });

    ensureEndpoints();

    if (static_cast<int>(points.size()) > MaxPoints)
        points.resize(MaxPoints);
}

int CustomEnvelope::getNumPoints() const
{
    std::lock_guard<std::mutex> lock(pointMutex);
    return static_cast<int>(points.size());
}

CustomEnvelope::Point CustomEnvelope::getPoint(int index) const
{
    std::lock_guard<std::mutex> lock(pointMutex);
    if (index >= 0 && index < static_cast<int>(points.size()))
        return points[index];
    return {};
}

std::vector<CustomEnvelope::Point> CustomEnvelope::getAllPoints() const
{
    std::lock_guard<std::mutex> lock(pointMutex);
    return points;
}

void CustomEnvelope::setPoints(const std::vector<Point>& newPoints)
{
    std::lock_guard<std::mutex> lock(pointMutex);
    points = newPoints;
    sortAndClampPoints();
}

bool CustomEnvelope::addPoint(float x, float y, float tension)
{
    std::lock_guard<std::mutex> lock(pointMutex);
    if (static_cast<int>(points.size()) >= MaxPoints)
        return false;

    points.push_back({ x, y, tension });
    sortAndClampPoints();
    return true;
}

bool CustomEnvelope::removePoint(int index)
{
    std::lock_guard<std::mutex> lock(pointMutex);
    if (index > 0 && index < static_cast<int>(points.size()) - 1)
    {
        points.erase(points.begin() + index);
        return true;
    }
    return false;
}

bool CustomEnvelope::updatePoint(int index, float newX, float newY, float newTension)
{
    std::lock_guard<std::mutex> lock(pointMutex);
    if (index < 0 || index >= static_cast<int>(points.size()))
        return false;

    if (index == 0)
        newX = 0.0f;
    else if (index == static_cast<int>(points.size()) - 1)
        newX = 1.0f;

    points[index].x = juce::jlimit(0.0f, 1.0f, newX);
    points[index].y = juce::jlimit(0.0f, 1.0f, newY);
    points[index].tension = juce::jlimit(-1.0f, 1.0f, newTension);

    sortAndClampPoints();
    return true;
}

// VolumeShaper / Kickstart / LFO Tool Standard (y = 0: silence/ducked at kick impact; y = 1: full volume)
void CustomEnvelope::loadPresetClassicPump()
{
    std::lock_guard<std::mutex> lock(pointMutex);
    points = {
        { 0.0f, 0.0f, 0.45f },
        { 0.12f, 0.15f, 0.50f },
        { 0.48f, 0.92f, -0.25f },
        { 1.0f, 1.0f, 0.0f }
    };
    sortAndClampPoints();
}

void CustomEnvelope::loadPresetKickDip()
{
    std::lock_guard<std::mutex> lock(pointMutex);
    points = {
        { 0.0f, 0.0f, 0.65f },
        { 0.22f, 1.0f, -0.15f },
        { 1.0f, 1.0f, 0.0f }
    };
    sortAndClampPoints();
}

void CustomEnvelope::loadPresetGate16th()
{
    std::lock_guard<std::mutex> lock(pointMutex);
    points = {
        { 0.0f, 0.0f, 0.0f },
        { 0.24f, 0.0f, 0.0f },
        { 0.25f, 1.0f, 0.0f },
        { 0.49f, 1.0f, 0.0f },
        { 0.50f, 0.0f, 0.0f },
        { 0.74f, 0.0f, 0.0f },
        { 0.75f, 1.0f, 0.0f },
        { 1.0f, 1.0f, 0.0f }
    };
    sortAndClampPoints();
}

void CustomEnvelope::loadPresetSawRamp()
{
    std::lock_guard<std::mutex> lock(pointMutex);
    points = {
        { 0.0f, 0.0f, -0.3f },
        { 1.0f, 1.0f, 0.0f }
    };
    sortAndClampPoints();
}

void CustomEnvelope::loadPresetSmoothSwell()
{
    std::lock_guard<std::mutex> lock(pointMutex);
    points = {
        { 0.0f, 0.0f, 0.3f },
        { 0.5f, 1.0f, -0.3f },
        { 1.0f, 0.0f, 0.0f }
    };
    sortAndClampPoints();
}

float CustomEnvelope::evaluateAt(float phase) const
{
    phase = juce::jlimit(0.0f, 1.0f, phase);

    std::lock_guard<std::mutex> lock(pointMutex);
    if (points.empty())
        return 1.0f;

    if (points.size() == 1)
        return points[0].y;

    for (size_t i = 0; i < points.size() - 1; ++i)
    {
        const auto& p0 = points[i];
        const auto& p1 = points[i + 1];

        if (phase >= p0.x && phase <= p1.x)
        {
            float dx = p1.x - p0.x;
            if (dx <= 1e-6f)
                return p1.y;

            float u = (phase - p0.x) / dx;
            float exponent = std::pow(2.0f, -p0.tension * 3.0f);
            float v = std::pow(juce::jlimit(0.0f, 1.0f, u), exponent);

            return p0.y + v * (p1.y - p0.y);
        }
    }

    return points.back().y;
}

float CustomEnvelope::getNextSample(juce::AudioPlayHead* playHead)
{
    double beatsPerCycle = static_cast<double>(getBeatsForRate(currentRate));
    double bpm = 120.0;
    bool isHostPlaying = false;

    if (playHead != nullptr)
    {
        if (auto posOpt = playHead->getPosition())
        {
            auto pos = *posOpt;
            if (pos.getBpm().hasValue())
                bpm = *pos.getBpm();

            if (pos.getIsPlaying())
            {
                isHostPlaying = true;
                if (pos.getPpqPosition().hasValue())
                {
                    double ppq = *pos.getPpqPosition();
                    double beatInCycle = std::fmod(ppq, beatsPerCycle);
                    if (beatInCycle < 0.0)
                        beatInCycle += beatsPerCycle;
                    internalPhase = beatInCycle / beatsPerCycle;
                }
            }
        }
    }

    if (!isHostPlaying)
    {
        double secondsPerBeat = 60.0 / bpm;
        double secondsPerCycle = secondsPerBeat * beatsPerCycle;
        double samplesPerCycle = secondsPerCycle * sampleRate;

        if (samplesPerCycle > 0.0)
        {
            internalPhase += 1.0 / samplesPerCycle;
            if (internalPhase >= 1.0)
                internalPhase -= std::floor(internalPhase);
        }
    }

    float normPhase = static_cast<float>(juce::jlimit(0.0, 1.0, internalPhase));
    currentPhase.store(normPhase);

    return evaluateAt(normPhase);
}

juce::String CustomEnvelope::toXmlString() const
{
    std::lock_guard<std::mutex> lock(pointMutex);
    juce::XmlElement xml("CUSTOM_ENVELOPE");
    xml.setAttribute("rate", static_cast<int>(currentRate));

    for (const auto& p : points)
    {
        auto* ptElem = xml.createNewChildElement("POINT");
        ptElem->setAttribute("x", p.x);
        ptElem->setAttribute("y", p.y);
        ptElem->setAttribute("tension", p.tension);
    }

    return xml.toString(juce::XmlElement::TextFormat().singleLine());
}

void CustomEnvelope::fromXmlString(const juce::String& xmlString)
{
    auto xml = juce::XmlDocument::parse(xmlString);
    if (xml != nullptr && xml->hasTagName("CUSTOM_ENVELOPE"))
    {
        std::lock_guard<std::mutex> lock(pointMutex);
        currentRate = static_cast<Rate>(xml->getIntAttribute("rate", 1));

        std::vector<Point> newPoints;
        for (auto* ptElem : xml->getChildIterator())
        {
            if (ptElem->hasTagName("POINT"))
            {
                Point p;
                p.x = static_cast<float>(ptElem->getDoubleAttribute("x", 0.0));
                p.y = static_cast<float>(ptElem->getDoubleAttribute("y", 0.0));
                p.tension = static_cast<float>(ptElem->getDoubleAttribute("tension", 0.0));
                newPoints.push_back(p);
            }
        }

        if (!newPoints.empty())
        {
            points = newPoints;
            sortAndClampPoints();
        }
    }
}
