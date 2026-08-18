#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

/**
 * Lock-free FIFO structure for streaming audio frames (Input, Output, Sidechain, GR)
 * from the DSP audio thread to the UI thread for FFT Spectrum Analysis.
 */
class AudioVisualizerFifo
{
public:
    static constexpr int FifoCapacity = 32768;

    struct VisualizerSample
    {
        float inSample = 0.0f;
        float outSample = 0.0f;
        float scSample = 0.0f;
        float gainReduction = 1.0f;
    };

    AudioVisualizerFifo()
        : abstractFifo(FifoCapacity)
    {
        buffer.resize(FifoCapacity);
    }

    void pushSample(float input, float output, float sidechain, float gr)
    {
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
    }

private:
    juce::AbstractFifo abstractFifo;
    std::vector<VisualizerSample> buffer;
};
