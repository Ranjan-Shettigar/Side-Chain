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
    lowCutParam     = apvts.getRawParameterValue("lowCut");
    highCutParam    = apvts.getRawParameterValue("highCut");
    outputGainParam = apvts.getRawParameterValue("outputGain");
    scListenParam   = apvts.getRawParameterValue("scListen");
    bypassParam     = apvts.getRawParameterValue("bypass");
}

SideDuckerAudioProcessor::~SideDuckerAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SideDuckerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Amount: 0 to 100%
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("amount", 1),
        "Amount",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        25.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return juce::String(val, 1) + " %"; }
    ));

    // Attack: 0.1ms to 100ms (exponential skew)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1),
        "Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.35f),
        10.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return juce::String(val, 1) + " ms"; }
    ));

    // Release: 5ms to 1000ms (exponential skew)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1),
        "Release",
        juce::NormalisableRange<float>(5.0f, 1000.0f, 1.0f, 0.35f),
        100.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return juce::String(val, 0) + " ms"; }
    ));

    // Low Cut: 20Hz to 2000Hz (exponential skew)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lowCut", 1),
        "Low Cut",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.3f),
        20.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return juce::String(val, 0) + " Hz"; }
    ));

    // High Cut: 500Hz to 20000Hz (exponential skew)
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("highCut", 1),
        "High Cut",
        juce::NormalisableRange<float>(500.0f, 20000.0f, 10.0f, 0.3f),
        20000.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return val >= 1000.0f ? juce::String(val / 1000.0f, 1) + " kHz" : juce::String(val, 0) + " Hz"; }
    ));

    // Output Gain: -12dB to +12dB
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputGain", 1),
        "Output Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f),
        0.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float val, int) { return (val > 0 ? "+" : "") + juce::String(val, 1) + " dB"; }
    ));

    // Sidechain Listen toggle
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("scListen", 1),
        "SC Listen",
        false
    ));

    // Bypass toggle
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("bypass", 1),
        "Bypass",
        false
    ));

    return { params.begin(), params.end() };
}

const juce::String SideDuckerAudioProcessor::getName() const
{
    return "SideDucker";
}

bool SideDuckerAudioProcessor::acceptsMidi() const
{
    return false;
}

bool SideDuckerAudioProcessor::producesMidi() const
{
    return false;
}

bool SideDuckerAudioProcessor::isMidiEffect() const
{
    return false;
}

double SideDuckerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SideDuckerAudioProcessor::getNumPrograms()
{
    return 1;
}

int SideDuckerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SideDuckerAudioProcessor::setCurrentProgram(int)
{
}

const juce::String SideDuckerAudioProcessor::getProgramName(int)
{
    return {};
}

void SideDuckerAudioProcessor::changeProgramName(int, const juce::String&)
{
}

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
    // Main input and output must match (mono/mono or stereo/stereo)
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    // Sidechain input can be disabled, mono, or stereo
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

    // Clear unused output channels
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Get parameters
    float amount     = amountParam     ? amountParam->load()     : 25.0f;
    float attack     = attackParam     ? attackParam->load()     : 10.0f;
    float release    = releaseParam    ? releaseParam->load()    : 100.0f;
    float lowCut     = lowCutParam     ? lowCutParam->load()     : 20.0f;
    float highCut    = highCutParam    ? highCutParam->load()    : 20000.0f;
    float outputGain = outputGainParam ? outputGainParam->load() : 0.0f;
    bool  scListen   = scListenParam   ? scListenParam->load() > 0.5f : false;
    bool  bypass     = bypassParam     ? bypassParam->load() > 0.5f   : false;

    duckingProcessor.setParameters(amount, attack, release, lowCut, highCut, outputGain, scListen, bypass);

    // Extract main audio buffer and sidechain audio buffer
    auto mainBus = getBusBuffer(buffer, true, 0);
    auto sidechainBus = getBusBuffer(buffer, true, 1);

    duckingProcessor.process(mainBus, sidechainBus, visualizerFifo);
}

bool SideDuckerAudioProcessor::hasEditor() const
{
    return true;
}

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
