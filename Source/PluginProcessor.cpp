#include "PluginProcessor.h"
#include "PluginEditor.h"

SideDuckerAudioProcessor::SideDuckerAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    amountParam     = apvts.getRawParameterValue("amount");
    attackParam     = apvts.getRawParameterValue("attack");
    releaseParam    = apvts.getRawParameterValue("release");
    outputGainParam = apvts.getRawParameterValue("outputGain");
    scListenParam   = apvts.getRawParameterValue("scListen");
    bypassParam     = apvts.getRawParameterValue("bypass");

    b1_freqParam    = apvts.getRawParameterValue("b1_freq");
    b1_slopeParam   = apvts.getRawParameterValue("b1_slope");
    b1_bypassParam  = apvts.getRawParameterValue("b1_bypass");

    for (int i = 0; i < 4; ++i)
    {
        int bandNum = i + 2;
        juce::String prefix = "b" + juce::String(bandNum) + "_";
        gpBandParams[i].typeParam   = apvts.getRawParameterValue(prefix + "type");
        gpBandParams[i].freqParam   = apvts.getRawParameterValue(prefix + "freq");
        gpBandParams[i].gainParam   = apvts.getRawParameterValue(prefix + "gain");
        gpBandParams[i].qParam      = apvts.getRawParameterValue(prefix + "q");
        gpBandParams[i].bypassParam = apvts.getRawParameterValue(prefix + "bypass");
    }

    b6_freqParam    = apvts.getRawParameterValue("b6_freq");
    b6_slopeParam   = apvts.getRawParameterValue("b6_slope");
    b6_bypassParam  = apvts.getRawParameterValue("b6_bypass");
}

SideDuckerAudioProcessor::~SideDuckerAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SideDuckerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Master Amount: 0 to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("amount", 1), "Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 35.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return juce::String(val, 1) + " %"; }
    ));

    // Attack: 0.1ms to 100ms
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1), "Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.35f), 10.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return juce::String(val, 1) + " ms"; }
    ));

    // Release: 5ms to 1000ms
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1), "Release",
        juce::NormalisableRange<float>(5.0f, 1000.0f, 1.0f, 0.35f), 100.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return juce::String(val, 0) + " ms"; }
    ));

    // Output Gain: -12dB to +12dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputGain", 1), "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return (val > 0 ? "+" : "") + juce::String(val, 1) + " dB"; }
    ));

    // Toggles
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("scListen", 1), "SC Listen", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass", 1), "Bypass", false));

    const juce::StringArray slopeChoices = { "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct" };
    const juce::StringArray typeChoices = { "Bell", "Low Shelf", "High Shelf", "Band Shelf", "Notch", "Tilt" };

    // Band 1 (Low Cut)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("b1_freq", 1), "B1 Low Cut Freq",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 1.0f, 0.3f), 30.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return juce::String(val, 0) + " Hz"; }
    ));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("b1_slope", 1), "B1 Slope", slopeChoices, 1 // 12 dB/oct
    ));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("b1_bypass", 1), "B1 Bypass", false
    ));

    // Bands 2 - 5 (General Purpose)
    const std::array<float, 4> defaultFreqs = { 120.0f, 380.0f, 1500.0f, 6000.0f };
    for (int i = 0; i < 4; ++i)
    {
        int bandNum = i + 2;
        juce::String prefix = "b" + juce::String(bandNum) + "_";
        juce::String bandName = "Band " + juce::String(bandNum) + " ";

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "type", 1), bandName + "Type", typeChoices, 0 // Bell
        ));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "freq", 1), bandName + "Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.25f), defaultFreqs[i],
            juce::String(), juce::AudioProcessorParameter::genericParameter,
            [](float val, int) { return val >= 1000.0f ? juce::String(val / 1000.0f, 2) + " kHz" : juce::String(val, 0) + " Hz"; }
        ));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "gain", 1), bandName + "Gain",
            juce::NormalisableRange<float>(-18.0f, 18.0f, 0.1f), 0.0f,
            juce::String(), juce::AudioProcessorParameter::genericParameter,
            [](float val, int) { return (val > 0 ? "+" : "") + juce::String(val, 1) + " dB"; }
        ));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "q", 1), bandName + "Q",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.35f), 1.4f,
            juce::String(), juce::AudioProcessorParameter::genericParameter,
            [](float val, int) { return juce::String(val, 2); }
        ));
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "bypass", 1), bandName + "Bypass", false
        ));
    }

    // Band 6 (High Cut)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("b6_freq", 1), "B6 High Cut Freq",
        juce::NormalisableRange<float>(500.0f, 20000.0f, 1.0f, 0.3f), 18000.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return val >= 1000.0f ? juce::String(val / 1000.0f, 2) + " kHz" : juce::String(val, 0) + " Hz"; }
    ));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("b6_slope", 1), "B6 Slope", slopeChoices, 1 // 12 dB/oct
    ));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("b6_bypass", 1), "B6 Bypass", false
    ));

    return { params.begin(), params.end() };
}

const juce::String SideDuckerAudioProcessor::getName() const { return "SideDucker"; }
bool SideDuckerAudioProcessor::acceptsMidi() const { return false; }
bool SideDuckerAudioProcessor::producesMidi() const { return false; }
bool SideDuckerAudioProcessor::isMidiEffect() const { return false; }
double SideDuckerAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int SideDuckerAudioProcessor::getNumPrograms() { return 1; }
int SideDuckerAudioProcessor::getCurrentProgram() { return 0; }
void SideDuckerAudioProcessor::setCurrentProgram(int) {}
const juce::String SideDuckerAudioProcessor::getProgramName(int) { return {}; }
void SideDuckerAudioProcessor::changeProgramName(int, const juce::String&) {}

void SideDuckerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    duckingProcessor.prepare(spec);
    visualizerFifo.reset();
}

void SideDuckerAudioProcessor::releaseResources()
{
    duckingProcessor.reset();
    visualizerFifo.reset();
}

bool SideDuckerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    const auto& sidechain = layouts.getChannelSet(true, 1);
    if (!sidechain.isDisabled()
        && sidechain != juce::AudioChannelSet::mono()
        && sidechain != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void SideDuckerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    float amount     = amountParam     ? amountParam->load()     : 35.0f;
    float attack     = attackParam     ? attackParam->load()     : 10.0f;
    float release    = releaseParam    ? releaseParam->load()    : 100.0f;
    float outputGain = outputGainParam ? outputGainParam->load() : 0.0f;
    bool  scListen   = scListenParam   ? scListenParam->load() > 0.5f : false;
    bool  bypass     = bypassParam     ? bypassParam->load() > 0.5f   : false;

    // Collect 6 parametric band configs
    std::array<DuckingProcessor::ParametricBandConfig, 6> bandConfigs;

    // Band 1 (Low Cut)
    bandConfigs[0].freq     = b1_freqParam ? b1_freqParam->load() : 30.0f;
    bandConfigs[0].slope    = static_cast<DuckingProcessor::SlopeType>(b1_slopeParam ? static_cast<int>(b1_slopeParam->load()) : 1);
    bandConfigs[0].bypassed = b1_bypassParam ? b1_bypassParam->load() > 0.5f : false;

    // Bands 2-5 (GP)
    for (int i = 0; i < 4; ++i)
    {
        int bandIdx = i + 1;
        bandConfigs[bandIdx].type     = static_cast<DuckingProcessor::BandType>(gpBandParams[i].typeParam ? static_cast<int>(gpBandParams[i].typeParam->load()) : 0);
        bandConfigs[bandIdx].freq     = gpBandParams[i].freqParam ? gpBandParams[i].freqParam->load() : 1000.0f;
        bandConfigs[bandIdx].gainDb   = gpBandParams[i].gainParam ? gpBandParams[i].gainParam->load() : 0.0f;
        bandConfigs[bandIdx].q        = gpBandParams[i].qParam ? gpBandParams[i].qParam->load() : 1.4f;
        bandConfigs[bandIdx].bypassed = gpBandParams[i].bypassParam ? gpBandParams[i].bypassParam->load() > 0.5f : false;
    }

    // Band 6 (High Cut)
    bandConfigs[5].freq     = b6_freqParam ? b6_freqParam->load() : 18000.0f;
    bandConfigs[5].slope    = static_cast<DuckingProcessor::SlopeType>(b6_slopeParam ? static_cast<int>(b6_slopeParam->load()) : 1);
    bandConfigs[5].bypassed = b6_bypassParam ? b6_bypassParam->load() > 0.5f : false;

    duckingProcessor.setParameters(amount, attack, release, outputGain, scListen, bypass, bandConfigs);

    auto mainBus = getBusBuffer(buffer, true, 0);
    auto sidechainBus = getBusBuffer(buffer, true, 1);

    duckingProcessor.process(mainBus, sidechainBus, visualizerFifo);
}

bool SideDuckerAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* SideDuckerAudioProcessor::createEditor()
{
    return new SideDuckerAudioProcessorEditor(*this);
}

void SideDuckerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SideDuckerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SideDuckerAudioProcessor();
}
