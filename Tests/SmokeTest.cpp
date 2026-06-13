// Headless DSP smoke tests for the TekkChild TA-670 MK2.
//
// These run in CI on every platform and assert the engine-level contract:
// compression engages and releases, the path stays finite under abuse,
// true bypass is bit-exact, and each engine mode reports sensible latency.

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

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

struct ContinuityResult
{
    float peak     = 0.0f; // |y| peak over the measured region
    float maxDelta = 0.0f; // largest |y[i] - y[i-1]| over the measured region
    bool  finite   = true;
};

// Feeds a steady sine and tracks the biggest sample-to-sample jump in the
// output. perBlock(blockIndex) may move a parameter before each block; deltas
// are only accumulated from measureFrom onward so the initial attack settle
// doesn't pollute the baseline, but continuity is tracked across every block
// boundary. A click introduced by a parameter change shows up as a delta far
// larger than the tone's natural slope.
template <typename PerBlock>
ContinuityResult runContinuity (TekkChild670Processor& proc, juce::AudioBuffer<float>& buffer,
                                float amplitude, int numBlocks, int measureFrom,
                                double freq, double fs, PerBlock perBlock)
{
    juce::MidiBuffer midi;
    ContinuityResult r;

    const int blockSize = buffer.getNumSamples();
    const double inc    = juce::MathConstants<double>::twoPi * freq / fs;

    double phase = 0.0;
    float prev   = 0.0f;
    bool havePrev = false;

    for (int blk = 0; blk < numBlocks; ++blk)
    {
        perBlock (blk);

        for (int i = 0; i < blockSize; ++i)
        {
            const float s = amplitude * (float) std::sin (phase);
            phase += inc;
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s);
        }

        proc.processBlock (buffer, midi);
        r.finite = r.finite && allFinite (buffer);

        for (int i = 0; i < blockSize; ++i)
        {
            const float y = buffer.getSample (0, i);

            if (blk >= measureFrom)
            {
                r.peak = std::max (r.peak, std::abs (y));
                if (havePrev)
                    r.maxDelta = std::max (r.maxDelta, std::abs (y - prev));
            }

            prev = y;
            havePrev = true;
        }
    }

    return r;
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

    // Latency is reported from the message thread, so pump the dispatch loop
    // before reading it back (a host's message thread does this for us).
    auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (40); };

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
        // Engine modes and latency reporting. Latency is reported from the
        // message thread (so setLatencySamples never runs on the audio thread),
        // hence the pump() before each read-back.
        setParam (proc, "quality", 0.0f); // Eco
        runSine (proc, buffer, 0.1f, 0.05, 220.0, fs);
        pump();
        const int ecoLatency = proc.getLatencySamples();

        setParam (proc, "quality", 1.0f); // Zero Latency
        runSine (proc, buffer, 0.1f, 0.05, 220.0, fs);
        pump();
        const int zlLatency = proc.getLatencySamples();

        setParam (proc, "quality", 2.0f); // Studio
        runSine (proc, buffer, 0.1f, 0.05, 220.0, fs);
        pump();
        const int studioLatency = proc.getLatencySamples();

        expect (ecoLatency == 0, "Eco mode is latency-free");
        expect (zlLatency <= 6, "Zero-Latency mode reports near-zero latency ("
                                    + std::to_string (zlLatency) + " samples)");
        expect (studioLatency > 0, "Studio (linear-phase) mode reports its latency ("
                                       + std::to_string (studioLatency) + " samples)");
    }

    // -- Anti-click / zipper hardening (continuity of the audio path) --------
    setParam (proc, "mode", 0.0f);     // Left/Right, so channel 0 stands alone
    setParam (proc, "quality", 0.0f);  // Eco: observe the core at base rate

    {
        // Automate Threshold continuously across many blocks. Without per-sample
        // smoothing this steps the detector every block boundary (zipper).
        setParam (proc, "timeconstA", 2.0f); // a steady mid release
        const auto r = runContinuity (proc, buffer, 0.35f, 60, 8, 60.0, fs,
            [&proc] (int blk)
            {
                const float knob = juce::jmap ((float) blk, 0.0f, 59.0f, 3.0f, 9.0f);
                setParam (proc, "thresholdA", knob);
            });

        expect (r.finite, "Threshold automation stays finite");
        expect (r.maxDelta < 0.06f * r.peak,
                "Threshold automation does not zipper (max step "
                    + std::to_string (r.maxDelta / std::max (r.peak, 1.0e-6f))
                    + " x peak)");
    }

    {
        // Flip the Time Constant switch mid-tone. Branch seeding keeps the
        // control voltage continuous, so the switch must not click.
        setParam (proc, "thresholdA", 8.0f);
        const auto r = runContinuity (proc, buffer, 0.35f, 40, 18, 60.0, fs,
            [&proc] (int blk)
            {
                if (blk == 20) setParam (proc, "timeconstA", 5.0f); // 1 -> 3 branches
                if (blk == 28) setParam (proc, "timeconstA", 0.0f); // 3 -> 1 branch
            });

        expect (r.finite, "Time Constant switching stays finite");
        expect (r.maxDelta < 0.06f * r.peak,
                "Time Constant switching does not click (max step "
                    + std::to_string (r.maxDelta / std::max (r.peak, 1.0e-6f))
                    + " x peak)");
    }

    {
        // True bypass in a zero-latency engine: bit-exact passthrough.
        setParam (proc, "quality", 0.0f); // Eco, 0 latency
        runSine (proc, buffer, 0.05f, 0.02, 220.0, fs);
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

        expect (identical, "true bypass (zero-latency engine) is bit-exact");
        setParam (proc, "bypass", 0.0f);
    }

    {
        // True bypass in an oversampled engine: the dry signal must come out
        // delayed by exactly the reported latency, so the host's delay
        // compensation keeps it aligned.
        setParam (proc, "quality", 2.0f); // Studio (linear phase, large latency)
        runSine (proc, buffer, 0.05f, 0.02, 220.0, fs);
        pump();
        const int latency = proc.getLatencySamples();
        setParam (proc, "bypass", 1.0f);

        juce::Random rng (671);
        const int totalLen = blockSize * 4;
        std::vector<float> in ((size_t) totalLen), out ((size_t) totalLen);

        juce::MidiBuffer midi;
        int written = 0;
        for (int blk = 0; blk < 4; ++blk)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const float s = rng.nextFloat() * 2.0f - 1.0f;
                in[(size_t) (written + i)] = s;
                buffer.setSample (0, i, s);
                buffer.setSample (1, i, s);
            }
            proc.processBlock (buffer, midi);
            for (int i = 0; i < blockSize; ++i)
                out[(size_t) (written + i)] = buffer.getSample (0, i);
            written += blockSize;
        }

        bool aligned = latency > 0;
        for (int i = latency; i < totalLen && aligned; ++i)
            aligned = std::abs (out[(size_t) i] - in[(size_t) (i - latency)]) < 1.0e-6f;

        expect (aligned, "true bypass is latency-aligned in an oversampled engine ("
                             + std::to_string (latency) + "-sample delay)");
        setParam (proc, "bypass", 0.0f);
        setParam (proc, "quality", 1.0f);
    }

    // -- Factory presets -----------------------------------------------------
    {
        auto programIndex = [&proc] (const juce::String& name)
        {
            for (int i = 0; i < proc.getNumPrograms(); ++i)
                if (proc.getProgramName (i) == name)
                    return i;
            return -1;
        };

        expect (proc.getNumPrograms() >= 8,
                "factory presets are exposed (" + std::to_string (proc.getNumPrograms()) + ")");

        const int master = programIndex ("Master Bus");
        expect (master >= 0, "Master Bus preset is present");

        proc.setCurrentProgram (master);
        expect (proc.getCurrentProgram() == master, "setCurrentProgram updates the current program");
        expect ((int) *proc.apvts.getRawParameterValue ("mode") == 2,
                "Master Bus selects Linked mode");
        expect ((int) *proc.apvts.getRawParameterValue ("quality") == 2,
                "Master Bus selects the Studio engine");
        expect ((int) *proc.apvts.getRawParameterValue ("timeconstA") == 5,
                "Master Bus selects an auto-release time constant");

        // Determinism: a preset fully defines the patch regardless of prior edits.
        const int smash = programIndex ("Drum Bus Smash");
        proc.setCurrentProgram (smash);
        const float ref = proc.apvts.getRawParameterValue ("thresholdA")->load();

        setParam (proc, "thresholdA", 1.0f);          // dirty the state
        proc.setCurrentProgram (0);                    // Init
        proc.setCurrentProgram (smash);                // reload
        const float reloaded = proc.apvts.getRawParameterValue ("thresholdA")->load();

        expect (std::abs (reloaded - ref) < 1.0e-4f, "reloading a preset is deterministic");

        proc.setCurrentProgram (0);
        setParam (proc, "bypass", 0.0f);
    }

    // -- Metering ------------------------------------------------------------
    {
        setParam (proc, "quality", 0.0f); // Eco, no latency
        setParam (proc, "mode", 0.0f);    // independent channels
        setParam (proc, "inputA", 0.0f);
        setParam (proc, "inputB", 0.0f);
        setParam (proc, "compinA", 0.0f); // colour only, so output tracks input
        setParam (proc, "compinB", 0.0f);

        runSine (proc, buffer, 0.5f, 0.2, 220.0, fs); // 0.5 peak = -6.02 dBFS

        const float inDb  = proc.getInputLevelDb (0);
        const float outDb = proc.getOutputLevelDb (0);

        expect (inDb > -7.0f && inDb < -5.0f,
                "input meter reads the input level (~-6 dBFS, got " + std::to_string (inDb) + ")");
        expect (std::isfinite (outDb) && outDb > -24.0f,
                "output meter is active (" + std::to_string (outDb) + " dBFS)");

        setParam (proc, "bypass", 1.0f);
        runSine (proc, buffer, 0.25f, 0.05, 220.0, fs); // -12 dBFS through bypass
        const float bypIn  = proc.getInputLevelDb (0);
        const float bypOut = proc.getOutputLevelDb (0);
        expect (std::abs (bypIn - bypOut) < 0.5f && bypIn > -16.0f,
                "meters track the signal through true bypass");
        setParam (proc, "bypass", 0.0f);
    }

    // -- Stereo link amount --------------------------------------------------
    {
        setParam (proc, "mode", 2.0f);    // Linked
        setParam (proc, "quality", 0.0f); // Eco

        // Hard link (100%): both channels should see identical GR.
        setParam (proc, "inputA",     6.0f);
        setParam (proc, "inputB",     0.0f);  // channel B is quiet
        setParam (proc, "thresholdA", 8.0f);
        setParam (proc, "thresholdB", 8.0f);
        setParam (proc, "linkamount", 100.0f);
        runSine (proc, buffer, 0.5f, 1.0, 220.0, fs);
        const float grHardA = proc.getGainReductionDb (0);
        const float grHardB = proc.getGainReductionDb (1);

        // Soft link (0%): channels are independent, B should show little GR.
        setParam (proc, "linkamount", 0.0f);
        proc.reset();
        runSine (proc, buffer, 0.5f, 1.0, 220.0, fs);
        const float grSoftA = proc.getGainReductionDb (0);
        const float grSoftB = proc.getGainReductionDb (1);

        expect (std::abs (grHardA - grHardB) < 1.0f,
                "hard link (100%) makes both channels track the louder one");
        expect (grSoftB < grSoftA * 0.5f,
                "soft link (0%) lets the quiet channel compress independently");

        setParam (proc, "linkamount", 100.0f);
        setParam (proc, "inputA", 0.0f);
        setParam (proc, "inputB", 0.0f);
    }

    // -- reset() clears level meters -----------------------------------------
    {
        setParam (proc, "mode", 0.0f);
        setParam (proc, "quality", 0.0f);
        setParam (proc, "compinA", 1.0f);
        setParam (proc, "compinB", 1.0f);
        runSine (proc, buffer, 0.5f, 0.1, 220.0, fs);

        proc.reset();

        const float inAfterReset  = proc.getInputLevelDb (0);
        const float outAfterReset = proc.getOutputLevelDb (0);
        const float grAfterReset  = proc.getGainReductionDb (0);

        expect (inAfterReset  <= -99.0f, "reset() clears input meter");
        expect (outAfterReset <= -99.0f, "reset() clears output meter");
        expect (grAfterReset  == 0.0f,   "reset() clears GR meter");
    }

    // -- State restoration of discrete parameters ----------------------------
    {
        // A bool/choice parameter keeps an un-snapped raw value; if a prior
        // value snapped to the same bin as the restored one, a naive
        // replaceState leaves it stale. This reproduces that pluginval failure.
        auto* comp   = proc.apvts.getParameter ("compinA");
        auto* purist = proc.apvts.getParameter ("purist");

        comp->setValueNotifyingHost (1.0f);   // AGC In A = true
        purist->setValueNotifyingHost (0.0f); // Purist  = false

        juce::MemoryBlock state;
        proc.getStateInformation (state);

        comp->setValueNotifyingHost (0.62f);   // snaps to true,  raw 0.62
        purist->setValueNotifyingHost (0.41f); // snaps to false, raw 0.41

        proc.setStateInformation (state.getData(), (int) state.getSize());

        expect (std::abs (comp->getValue() - 1.0f) < 0.05f,
                "discrete parameter restores exactly (AGC In, got "
                    + std::to_string (comp->getValue()) + ")");
        expect (std::abs (purist->getValue() - 0.0f) < 0.05f,
                "discrete parameter restores exactly (Purist, got "
                    + std::to_string (purist->getValue()) + ")");
    }

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << std::endl;

    return failures == 0 ? 0 : 1;
}
