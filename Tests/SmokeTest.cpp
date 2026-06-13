// Headless DSP smoke tests for the TekkChild TA-670 MK2.
//
// These run in CI on every platform and assert the engine-level contract:
// compression engages and releases, the path stays finite under abuse,
// true bypass is bit-exact, and each engine mode reports sensible latency.

#include <cmath>
#include <iostream>
#include <string>

#include <juce_events/juce_events.h>

#include "PluginProcessor.h"

namespace
{

int failures = 0;

void expect (bool condition, const std::string& description)
{
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << description << std::endl;
    if (! condition)
        ++failures;
}

void setParam (TekkChild670Processor& proc, const juce::String& id, float plainValue)
{
    auto* p = proc.apvts.getParameter (id);

    if (p == nullptr)
    {
        std::cout << "[FAIL] missing parameter: " << id << std::endl;
        ++failures;
        return;
    }

    p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;

    return true;
}

struct RunResult
{
    double inRmsDb  = -200.0;
    double outRmsDb = -200.0;
    float  peak     = 0.0f;
    bool   finite   = true;
};

RunResult runSine (TekkChild670Processor& proc, juce::AudioBuffer<float>& buffer,
                   float amplitude, double seconds, double freq, double fs,
                   bool rightSilent = false)
{
    juce::MidiBuffer midi;
    RunResult result;

    const int blockSize = buffer.getNumSamples();
    const int numBlocks = (int) std::ceil (seconds * fs / blockSize);
    const double inc    = juce::MathConstants<double>::twoPi * freq / fs;

    double phase = 0.0, sumIn = 0.0, sumOut = 0.0;
    long count = 0;

    for (int blk = 0; blk < numBlocks; ++blk)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const float s = amplitude * (float) std::sin (phase);
            phase += inc;
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, rightSilent ? 0.0f : s);
            sumIn += (double) s * s;
        }

        proc.processBlock (buffer, midi);
        result.finite = result.finite && allFinite (buffer);

        for (int i = 0; i < blockSize; ++i)
        {
            const float o = buffer.getSample (0, i);
            sumOut += (double) o * o;
            result.peak = std::max (result.peak, std::abs (o));
            ++count;
        }
    }

    result.inRmsDb  = 10.0 * std::log10 (sumIn / (double) count + 1.0e-20);
    result.outRmsDb = 10.0 * std::log10 (sumOut / (double) count + 1.0e-20);
    return result;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    constexpr double fs      = 48000.0;
    constexpr int blockSize  = 512;

    TekkChild670Processor proc;
    proc.setPlayConfigDetails (2, 2, fs, blockSize);
    proc.prepareToPlay (fs, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);

    std::cout << "TekkChild TA-670 MK2 -- DSP smoke tests\n" << std::endl;

    // drive it into compression: +6 dB input, threshold knob at 8 (-26 dBFS)
    setParam (proc, "inputA", 6.0f);
    setParam (proc, "inputB", 6.0f);
    setParam (proc, "thresholdA", 8.0f);
    setParam (proc, "thresholdB", 8.0f);

    {
        const auto r = runSine (proc, buffer, 0.5f, 1.0, 220.0, fs);
        const float gr = proc.getGainReductionDb (0);

        expect (r.finite, "output is finite under compression");
        expect (gr > 2.0f, "compression engages (GR = " + std::to_string (gr) + " dB)");
        expect (gr < 31.0f, "gain reduction stays within the variable-mu range");
        expect (r.outRmsDb < r.inRmsDb - 1.0,
                "output level is reduced (in " + std::to_string (r.inRmsDb)
                    + " dB RMS, out " + std::to_string (r.outRmsDb) + " dB RMS)");
    }

    {
        const auto r = runSine (proc, buffer, 1.0e-5f, 4.0, 220.0, fs);
        const float gr = proc.getGainReductionDb (0);

        expect (r.finite, "output is finite during release");
        expect (gr < 0.5f, "gain reduction releases after the signal stops (GR = "
                               + std::to_string (gr) + " dB)");
    }

    {
        // +12 dBFS programme on top of +6 dB input gain: total abuse
        const auto r = runSine (proc, buffer, 4.0f, 0.5, 220.0, fs);

        expect (r.finite, "output is finite when slammed (+18 dBFS into the core)");
        expect (r.peak < 8.0f, "tube stages bound the output when slammed (peak = "
                                   + std::to_string (r.peak) + ")");
    }

    {
        setParam (proc, "mode", 1.0f); // Lat / Vert
        const auto r = runSine (proc, buffer, 0.5f, 0.25, 220.0, fs, true);

        expect (r.finite, "Lat/Vert (M/S) mode is finite with a one-sided signal");
        setParam (proc, "mode", 2.0f);
    }

    {
        // engine modes and latency reporting
        setParam (proc, "quality", 0.0f); // Eco
        runSine (proc, buffer, 0.1f, 0.05, 220.0, fs);
        const int ecoLatency = proc.getLatencySamples();

        setParam (proc, "quality", 1.0f); // Zero Latency
        runSine (proc, buffer, 0.1f, 0.05, 220.0, fs);
        const int zlLatency = proc.getLatencySamples();

        setParam (proc, "quality", 2.0f); // Studio
        runSine (proc, buffer, 0.1f, 0.05, 220.0, fs);
        const int studioLatency = proc.getLatencySamples();

        expect (ecoLatency == 0, "Eco mode is latency-free");
        expect (zlLatency <= 6, "Zero-Latency mode reports near-zero latency ("
                                    + std::to_string (zlLatency) + " samples)");
        expect (studioLatency > 0, "Studio (linear-phase) mode reports its latency ("
                                       + std::to_string (studioLatency) + " samples)");

        setParam (proc, "quality", 1.0f);
        runSine (proc, buffer, 0.1f, 0.05, 220.0, fs);
    }

    {
        // true bypass must be bit-exact
        setParam (proc, "bypass", 1.0f);

        juce::Random rng (670);
        juce::AudioBuffer<float> reference (2, blockSize);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                reference.setSample (ch, i, rng.nextFloat() * 2.0f - 1.0f);

        buffer.makeCopyOf (reference);

        juce::MidiBuffer midi;
        proc.processBlock (buffer, midi);

        bool identical = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < blockSize; ++i)
                identical = identical
                         && buffer.getSample (ch, i) == reference.getSample (ch, i);

        expect (identical, "true bypass passes the signal bit-exactly");
        setParam (proc, "bypass", 0.0f);
    }

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << std::endl;

    return failures == 0 ? 0 : 1;
}
