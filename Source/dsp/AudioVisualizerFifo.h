#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Lock-free FIFO structure for passing audio waveform and ducking telemetry
 * safely from the DSP thread to the UI visualizer thread.
 */
class AudioVisualizerFifo
{
public:
    static constexpr int FifoCapacity = 16384;
    static constexpr int SubsampleFactor = 32; // Decimate samples to provide smooth long-window scrolling

    struct VisualizerSample
    {
        float inputSample = 0.0f;
        float outputSample = 0.0f;
        float sidechainSample = 0.0f;
        float gainReduction = 1.0f; // 1.0 = no reduction, 0.0 = full reduction
    };

    AudioVisualizerFifo()
        : abstractFifo(FifoCapacity)
    {
        buffer.resize(FifoCapacity);
    }

    void pushSample(float input, float output, float sidechain, float gr)
    {
        // Simple subsampling / decimation counter to save CPU & bandwidth
        if (++sampleCounter >= SubsampleFactor)
        {
            sampleCounter = 0;

            int start1, size1, start2, size2;
            abstractFifo.prepareToWrite(1, start1, size1, start2, size2);

            if (size1 > 0)
            {
                buffer[start1] = { input, output, sidechain, gr };
            }
            else if (size2 > 0)
            {
                buffer[start2] = { input, output, sidechain, gr };
            }

            abstractFifo.finishedWrite(size1 + size2);
        }
    }

    int readAvailable(VisualizerSample* destination, int maxSamplesToRead)
    {
        int numReady = abstractFifo.getNumReady();
        int toRead = juce::jmin(numReady, maxSamplesToRead);

        if (toRead <= 0)
            return 0;

        int start1, size1, start2, size2;
        abstractFifo.prepareToRead(toRead, start1, size1, start2, size2);

        if (size1 > 0)
        {
            std::memcpy(destination, &buffer[start1], sizeof(VisualizerSample) * size1);
        }
        if (size2 > 0)
        {
            std::memcpy(destination + size1, &buffer[start2], sizeof(VisualizerSample) * size2);
        }

        abstractFifo.finishedRead(size1 + size2);
        return size1 + size2;
    }

    void reset()
    {
        abstractFifo.reset();
        sampleCounter = 0;
    }

private:
    juce::AbstractFifo abstractFifo;
    std::vector<VisualizerSample> buffer;
    int sampleCounter = 0;
};
