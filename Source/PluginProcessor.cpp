#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <memory>
#include <vector>
#include <array>
#include <cmath>

SideDuckerAudioProcessor::SideDuckerAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    amountParam        = apvts.getRawParameterValue("amount");
    attackParam        = apvts.getRawParameterValue("attack");
    releaseParam       = apvts.getRawParameterValue("release");
    outputGainParam    = apvts.getRawParameterValue("outputGain");
    scListenParam      = apvts.getRawParameterValue("scListen");
    bypassParam        = apvts.getRawParameterValue("bypass");
    spectralBandsParam = apvts.getRawParameterValue("spectral_bands");

    stereoModeParam    = apvts.getRawParameterValue("stereo_mode");
    stereoLinkParam    = apvts.getRawParameterValue("stereo_link");
    stereoBalanceParam = apvts.getRawParameterValue("stereo_balance");

    for (int i = 0; i < 6; ++i)
    {
        int bandNum = i + 1;
        juce::String prefix = "b" + juce::String(bandNum) + "_";
        bandParams[i].shapeParam   = apvts.getRawParameterValue(prefix + "shape");
        bandParams[i].slopeParam   = apvts.getRawParameterValue(prefix + "slope");
        bandParams[i].stereoParam  = apvts.getRawParameterValue(prefix + "stereo");
        bandParams[i].freqParam    = apvts.getRawParameterValue(prefix + "freq");
        bandParams[i].gainParam    = apvts.getRawParameterValue(prefix + "gain");
        bandParams[i].qParam       = apvts.getRawParameterValue(prefix + "q");
        bandParams[i].bypassParam  = apvts.getRawParameterValue(prefix + "bypass");
        bandParams[i].soloParam    = apvts.getRawParameterValue(prefix + "solo");
    }
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

    // Toggles & Modes
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("scListen", 1), "SC Listen", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID("bypass", 1), "Bypass", false));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("spectral_bands", 1), "Sharpness", 16, 64, 32
    ));

    // Master Stereo Controls
    const juce::StringArray stereoModeChoices = { "left | right", "mid | side" };
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("stereo_mode", 1), "Stereo Mode", stereoModeChoices, 0
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stereo_link", 1), "Stereo Link",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return juce::String(val, 0) + " %"; }
    ));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stereo_balance", 1), "Stereo Balance",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float val, int) {
            if (std::abs(val) < 0.5f)
                return juce::String("even");
            if (val < 0.0f)
            {
                int rVal = static_cast<int>(100.0f + val);
                return "100 L | " + juce::String(rVal) + " R";
            }
            int lVal = static_cast<int>(100.0f - val);
            return juce::String(lVal) + " L | 100 R";
        }
    ));

    // 6 Parametric Bands
    const juce::StringArray shapeChoices = { "Bell", "Low Cut", "High Cut", "Low Shelf", "High Shelf", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt" };
    const juce::StringArray slopeChoices = { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct", "30 dB/oct", "36 dB/oct", "48 dB/oct", "72 dB/oct", "96 dB/oct", "Brickwall" };
    const juce::StringArray stereoChoices = { "Stereo", "Left", "Right", "Mid", "Side" };

    const std::array<float, 6> defaultFreqs = { 30.0f, 120.0f, 380.0f, 1500.0f, 6000.0f, 18000.0f };
    const std::array<int, 6> defaultShapes = { 1, 0, 0, 0, 0, 2 };

    for (int i = 0; i < 6; ++i)
    {
        int bandNum = i + 1;
        juce::String prefix = "b" + juce::String(bandNum) + "_";
        juce::String bandName = "Band " + juce::String(bandNum) + " ";

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "shape", 1), bandName + "Shape", shapeChoices, defaultShapes[i]
        ));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "slope", 1), bandName + "Slope", slopeChoices, 1
        ));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "stereo", 1), bandName + "Placement", stereoChoices, 0
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
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "solo", 1), bandName + "Solo", false
        ));
    }

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

    // Stereo Section Config
    DuckingProcessor::StereoConfig stereoConfig;
    stereoConfig.mode           = static_cast<DuckingProcessor::StereoMode>(stereoModeParam ? static_cast<int>(stereoModeParam->load()) : 0);
    stereoConfig.linkPercent    = stereoLinkParam ? stereoLinkParam->load() : 100.0f;
    float bal = stereoBalanceParam ? stereoBalanceParam->load() : 0.0f;
    stereoConfig.balancePercent = (bal + 100.0f) * 0.5f;

    // Collect 6 parametric band configs
    std::array<DuckingProcessor::ParametricBandConfig, 6> bandConfigs;

    for (int i = 0; i < 6; ++i)
    {
        bandConfigs[i].shape           = static_cast<DuckingProcessor::FilterShape>(bandParams[i].shapeParam ? static_cast<int>(bandParams[i].shapeParam->load()) : 0);
        bandConfigs[i].slope           = static_cast<DuckingProcessor::FilterSlope>(bandParams[i].slopeParam ? static_cast<int>(bandParams[i].slopeParam->load()) : 1);
        bandConfigs[i].stereoPlacement = static_cast<DuckingProcessor::StereoPlacement>(bandParams[i].stereoParam ? static_cast<int>(bandParams[i].stereoParam->load()) : 0);
        bandConfigs[i].freq            = bandParams[i].freqParam ? bandParams[i].freqParam->load() : 1000.0f;
        bandConfigs[i].gainDb          = bandParams[i].gainParam ? bandParams[i].gainParam->load() : 0.0f;
        bandConfigs[i].q               = bandParams[i].qParam ? bandParams[i].qParam->load() : 1.4f;
        bandConfigs[i].bypassed        = bandParams[i].bypassParam ? bandParams[i].bypassParam->load() > 0.5f : false;
        bandConfigs[i].solo            = bandParams[i].soloParam ? bandParams[i].soloParam->load() > 0.5f : false;
    }

    int numBands = spectralBandsParam ? static_cast<int>(spectralBandsParam->load()) : 32;
    duckingProcessor.setParameters(amount, attack, release, outputGain, scListen, bypass, numBands, bandConfigs, stereoConfig);

    auto mainBus = getBusBuffer(buffer, true, 0);
    auto sidechainBus = getBusBuffer(buffer, true, 1);

    duckingProcessor.process(mainBus, sidechainBus, visualizerFifo, getPlayHead());
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
    if (xml != nullptr)
    {
        copyXmlToBinary(*xml, destData);
    }
}

void SideDuckerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SideDuckerAudioProcessor();
}
