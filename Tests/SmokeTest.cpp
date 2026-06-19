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

        expect (proc.getNumPrograms() >= 28,
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

    // -- Easter egg ----------------------------------------------------------
    {
        // Triggering it decodes the embedded clip and mixes it into the output.
        setParam (proc, "bypass", 0.0f);
        proc.triggerEasterEgg();

        juce::MidiBuffer midi;
        float peak = 0.0f;
        for (int blk = 0; blk < 120; ++blk) // ~1.3 s of silent input
        {
            buffer.clear();
            proc.processBlock (buffer, midi);
            peak = std::max (peak, buffer.getMagnitude (0, 0, blockSize));
        }

        expect (peak > 0.001f,
                "easter egg plays the embedded clip into silent output (peak "
                    + std::to_string (peak) + ")");
    }

    // -- Drive / saturation --------------------------------------------------
    {
        // More Drive = harder tube/iron saturation, which clamps a hot peak more.
        setParam (proc, "bypass", 0.0f);
        setParam (proc, "mode", 0.0f);
        setParam (proc, "compinA", 0.0f); // colour only, isolate saturation
        setParam (proc, "compinB", 0.0f);
        setParam (proc, "inputA", 0.0f);
        setParam (proc, "outputA", 0.0f);

        auto peakAtDrive = [&] (float drive)
        {
            setParam (proc, "drive", drive);
            runSine (proc, buffer, 1.2f, 0.30, 110.0, fs);            // settle character
            return runSine (proc, buffer, 1.2f, 0.10, 110.0, fs).peak; // measure
        };

        const float lo = peakAtDrive (0.0f);
        const float hi = peakAtDrive (100.0f);

        expect (std::isfinite (lo) && std::isfinite (hi) && lo > 0.1f,
                "Drive output is finite and present");
        expect (hi < lo,
                "higher Drive saturates harder (peak " + std::to_string (lo)
                    + " -> " + std::to_string (hi) + ")");

        setParam (proc, "drive", 50.0f);
    }

    // -- Tube roll: type, bias, voltage --------------------------------------
    {
        setParam (proc, "bypass", 0.0f);
        setParam (proc, "mode", 0.0f);
        setParam (proc, "compinA", 0.0f);
        setParam (proc, "compinB", 0.0f);
        setParam (proc, "inputA", 0.0f);
        setParam (proc, "outputA", 0.0f);
        setParam (proc, "drive", 75.0f);
        setParam (proc, "tubevolt", 250.0f);
        setParam (proc, "tubebias", 0.0f);

        auto peakForType = [&] (float type)
        {
            setParam (proc, "tubetype", type);
            runSine (proc, buffer, 1.1f, 0.30, 110.0, fs);            // settle the glide
            return runSine (proc, buffer, 1.1f, 0.10, 110.0, fs).peak; // measure
        };

        const float pHi    = peakForType (1.0f); // 12AX7 hi-gain
        const float pClean = peakForType (2.0f); // 12AU7 clean
        expect (std::isfinite (pHi) && std::isfinite (pClean),
                "tube types produce finite output");
        expect (pHi < pClean,
                "hi-gain valve saturates harder than the clean valve (" + std::to_string (pHi)
                    + " vs " + std::to_string (pClean) + ")");

        // Biasing toward cutoff lowers the stage gain (and adds 2nd harmonic).
        auto peakForBias = [&] (float bias)
        {
            setParam (proc, "tubetype", 0.0f);
            setParam (proc, "tubebias", bias);
            runSine (proc, buffer, 0.5f, 0.30, 110.0, fs);
            return runSine (proc, buffer, 0.5f, 0.10, 110.0, fs).peak;
        };

        const float b0   = peakForBias (0.0f);
        const float bBig = peakForBias (100.0f);
        expect (bBig < b0,
                "Bias shifts the operating point toward cutoff, lowering gain ("
                    + std::to_string (b0) + " -> " + std::to_string (bBig) + ")");

        setParam (proc, "tubetype", 0.0f);
        setParam (proc, "tubebias", 0.0f);
        setParam (proc, "tubevolt", 250.0f);
        setParam (proc, "drive", 50.0f);
    }

    // -- Output tools: auto makeup + safety clip -----------------------------
    {
        // Auto Gain adds the average gain reduction back, raising the output.
        setParam (proc, "bypass", 0.0f);
        setParam (proc, "mode", 0.0f);
        setParam (proc, "quality", 0.0f);
        setParam (proc, "safety", 0.0f);
        setParam (proc, "compinA", 1.0f);
        setParam (proc, "inputA", 6.0f);
        setParam (proc, "thresholdA", 8.0f); // heavy, steady gain reduction

        auto outRmsWithMakeup = [&] (float on)
        {
            setParam (proc, "automakeup", on);
            proc.reset();
            runSine (proc, buffer, 0.5f, 3.0, 220.0, fs);             // build the slow makeup
            return runSine (proc, buffer, 0.5f, 0.5, 220.0, fs).outRmsDb;
        };

        const float mkOff = outRmsWithMakeup (0.0f);
        const float mkOn  = outRmsWithMakeup (1.0f);
        expect (mkOn > mkOff + 3.0f,
                "Auto Gain restores level (" + std::to_string (mkOff) + " -> "
                    + std::to_string (mkOn) + " dB)");

        setParam (proc, "automakeup", 0.0f);
        setParam (proc, "compinA", 0.0f);
        setParam (proc, "inputA", 0.0f);
    }

    {
        // Safety clip holds the output at/under ~0 dBFS even when driven over.
        setParam (proc, "automakeup", 0.0f);
        setParam (proc, "compinA", 0.0f);
        setParam (proc, "inputA", 0.0f);
        setParam (proc, "drive", 50.0f);
        setParam (proc, "outputA", 12.0f); // push the output well over 0 dBFS

        setParam (proc, "safety", 0.0f);
        const float pkOff = runSine (proc, buffer, 0.5f, 0.2, 220.0, fs).peak;

        setParam (proc, "safety", 1.0f);
        runSine (proc, buffer, 0.5f, 0.1, 220.0, fs);
        const float pkOn = runSine (proc, buffer, 0.5f, 0.2, 220.0, fs).peak;

        expect (pkOff > 1.0f, "output exceeds 0 dBFS without Safety (peak "
                                  + std::to_string (pkOff) + ")");
        expect (pkOn <= 1.0f, "Safety holds the output at/under 0 dBFS (peak "
                                  + std::to_string (pkOn) + ")");

        setParam (proc, "safety", 0.0f);
        setParam (proc, "outputA", 0.0f);
    }

    // -- Iron Drive (transformer saturation) ---------------------------------
    {
        setParam (proc, "bypass", 0.0f);
        setParam (proc, "mode", 0.0f);
        setParam (proc, "quality", 0.0f);
        setParam (proc, "compinA", 0.0f);
        setParam (proc, "compinB", 0.0f);
        setParam (proc, "inputA", 0.0f);

        setParam (proc, "irondrive", 10.0f);
        const auto rLow = runSine (proc, buffer, 1.2f, 0.25, 110.0, fs);
        proc.reset();
        setParam (proc, "irondrive", 190.0f);
        const auto rHigh = runSine (proc, buffer, 1.2f, 0.25, 110.0, fs);

        expect (rLow.finite && rHigh.finite, "output is finite across iron drive range");
        expect (rHigh.peak < 6.0f, "output stays bounded at max iron drive (peak "
                                       + std::to_string (rHigh.peak) + ")");
        setParam (proc, "irondrive", 100.0f);
        setParam (proc, "compinA", 1.0f);
        setParam (proc, "compinB", 1.0f);
    }

    // -- Valve hiss ----------------------------------------------------------
    {
        proc.reset(); // make sure no easter-egg playback bleeds into the measurement
        setParam (proc, "quality", 0.0f);
        setParam (proc, "compinA", 0.0f);
        setParam (proc, "inputA", 0.0f);
        setParam (proc, "noisefloor", 1.0f);

        juce::MidiBuffer midi;
        double noisePow = 0.0;
        for (int blk = 0; blk < 100; ++blk)
        {
            buffer.clear();
            proc.processBlock (buffer, midi);
            for (int i = 0; i < blockSize; ++i)
            {
                const float o = buffer.getSample (0, i);
                noisePow += (double) o * o;
            }
        }
        const float noiseRms = (float) std::sqrt (noisePow / (100.0 * blockSize));

        expect (noiseRms > 1.0e-7f, "valve hiss adds noise when enabled (rms "
                                        + std::to_string (noiseRms) + ")");
        expect (noiseRms < 5.0e-4f, "valve hiss is at a gentle calibrated level");
        setParam (proc, "noisefloor", 0.0f);
        setParam (proc, "compinA", 1.0f);
    }

    // -- Dry Gain ------------------------------------------------------------
    {
        setParam (proc, "quality", 0.0f);
        setParam (proc, "mode", 0.0f);
        setParam (proc, "compinA", 0.0f);
        setParam (proc, "mixA", 50.0f);

        setParam (proc, "drygainA", 6.0f);
        const auto rBoost = runSine (proc, buffer, 0.5f, 0.25, 220.0, fs);
        proc.reset();
        setParam (proc, "drygainA", -6.0f);
        const auto rCut = runSine (proc, buffer, 0.5f, 0.25, 220.0, fs);

        expect (rBoost.outRmsDb > rCut.outRmsDb + 3.0,
                "dry gain +6 dB raises output; -6 dB lowers it ("
                    + std::to_string (rCut.outRmsDb) + " -> " + std::to_string (rBoost.outRmsDb) + ")");
        setParam (proc, "drygainA", 0.0f);
        setParam (proc, "mixA", 100.0f);
        setParam (proc, "compinA", 1.0f);
    }

    // -- Stereo link amount --------------------------------------------------
    {
        setParam (proc, "quality", 0.0f);
        setParam (proc, "mode", 2.0f);   // L/R Linked
        setParam (proc, "inputA", 6.0f); // A loud, B quiet
        setParam (proc, "inputB", 0.0f);
        setParam (proc, "thresholdA", 8.0f);
        setParam (proc, "thresholdB", 8.0f);

        setParam (proc, "linkamount", 100.0f);
        runSine (proc, buffer, 0.5f, 0.6, 220.0, fs, true);
        const float grHardA = proc.getGainReductionDb (0);
        const float grHardB = proc.getGainReductionDb (1);

        proc.reset();
        setParam (proc, "linkamount", 0.0f);
        runSine (proc, buffer, 0.5f, 0.6, 220.0, fs, true);
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

    // -- reset() clears the level meters -------------------------------------
    {
        setParam (proc, "mode", 0.0f);
        setParam (proc, "quality", 0.0f);
        setParam (proc, "compinA", 1.0f);
        runSine (proc, buffer, 0.5f, 0.2, 220.0, fs);
        proc.reset();

        expect (proc.getInputLevelDb (0) <= -99.0f, "reset clears the input meter");
        expect (proc.getOutputLevelDb (0) <= -99.0f, "reset clears the output meter");
        expect (proc.getGainReductionDb (0) == 0.0f, "reset clears the GR meter");
    }

    // -- Tape Brain (series / parallel routing) ------------------------------
    {
        proc.reset();
        setParam (proc, "bypass", 0.0f);
        setParam (proc, "mode", 0.0f);
        setParam (proc, "quality", 0.0f);
        setParam (proc, "purist", 0.0f);
        setParam (proc, "compinA", 0.0f); // colour only, no compression
        setParam (proc, "inputA", 0.0f);

        setParam (proc, "tapeon", 0.0f);
        const auto dry = runSine (proc, buffer, 0.5f, 0.2, 220.0, fs);

        setParam (proc, "tapeon", 1.0f);
        setParam (proc, "tapepar", 0.0f);   // series
        setParam (proc, "tapedrive", 9.0f);
        setParam (proc, "tapesat", 9.0f);
        const auto series = runSine (proc, buffer, 0.5f, 0.3, 220.0, fs);

        setParam (proc, "tapepar", 1.0f);   // parallel
        const auto parallel = runSine (proc, buffer, 0.5f, 0.3, 220.0, fs);

        expect (series.finite && parallel.finite, "Tape output is finite (series & parallel)");
        expect (series.peak < 4.0f && parallel.peak < 4.0f, "Tape output stays bounded");
        expect (series.peak < dry.peak,
                "Tape (series) saturates the peak (" + std::to_string (dry.peak)
                    + " -> " + std::to_string (series.peak) + ")");
        expect (parallel.outRmsDb > series.outRmsDb,
                "Parallel route sums the tape on top of the compressor output");

        // wow & flutter modulates a steady tone away from a clean sine
        setParam (proc, "tapedrive", 0.0f);
        setParam (proc, "tapesat", 0.0f);
        setParam (proc, "tapepar", 0.0f);
        setParam (proc, "tapewow", 10.0f);
        const auto wow = runSine (proc, buffer, 0.5f, 0.5, 220.0, fs);
        expect (wow.finite, "Wow & Flutter output is finite");

        setParam (proc, "tapeon", 0.0f);
        setParam (proc, "tapewow", 0.0f);
        setParam (proc, "tapesat", 3.0f);
        setParam (proc, "compinA", 1.0f);
    }

    std::cout << "\n" << (failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED")
              << std::endl;

    return failures == 0 ? 0 : 1;
}
