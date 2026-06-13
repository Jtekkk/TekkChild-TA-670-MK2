# TekkChild TA-670 MK2

The TekkChild TA-670 MK2 is our revolutionary VST plugin that brings the most
iconic and finest sounding tube compressor in history to your DAW.
Painstakingly modeled by our small team of DSP engineers, the TekkChild is a
recreation of a Fairchild 670 with added modern features for today's
workflows.

While developing this plugin we had two main objectives: first and foremost,
the audio processing had to remain as close as possible to the original
hardware unit, and secondly the compression curve had to be identical to the
original. With our proprietary tube modeling algorithms, transformer
saturation algorithms, and state-of-the-art oversampling, the TekkChild 670
delivers the tone, character, compression curves and frequency response of an
original Fairchild compressor at a fraction of the cost.

**Fully automatable, with a zero latency mode available for mixing.**

## The Tube Modeling

The single most important part of the circuit -- and the heart of any
Fairchild comp -- are the peculiar compression curves that can only be
achieved with its tube complement (sixteen 6BA6 and two 12AX7 in the
hardware). Its peculiar time constants are what make the 670 so unique. The
gain stage is modeled with nonlinear processing and a feedback detector
topology to ensure the smooth, programme-dependent compression that responds
just like the hardware.

## The Transformer Modeling

Custom algorithms replicate the behavior of the original UTC A26 input
transformer as well as an XL Sowter output transformer. The transformer
saturation stages add the subtle, low-frequency-weighted harmonic distortion
that makes the original so special.

## The Circuit and Controls

The TA-670 adds several modern features for modern-day productions.
DC Threshold, sidechain High Pass Filter, Dry/Wet blend control and Output
trim are available on each channel, alongside a true bypass switch and a
Purist mode that takes the modern blend controls out of circuit entirely.

## Processing Efficiency

Advanced oversampling is used only where needed -- around the nonlinear core
-- and the engine offers three modes so CPU is spent where it matters:

| Engine mode | Oversampling | Latency |
| --- | --- | --- |
| Eco | none | 0 samples |
| Zero Latency | 4x minimum-phase IIR | ~0 samples (tracking & mixing) |
| Studio | 4x linear-phase FIR | reported to the host |

---

## Controls

Per channel (A = Left/Lateral, B = Right/Vertical):

| Control | Range | Function |
| --- | --- | --- |
| Input | -12..+24 dB | Drive into the unit; raises both colour and compression depth |
| Threshold | 0..10 | Detector sensitivity, hardware-style taper |
| Time Constant | 1..6 | The six original attack/release networks (5 and 6 are program-dependent) |
| DC Threshold | -10..+10 | Rectifier bias: earlier/softer knee anticlockwise, later/harder clockwise |
| SC Filter | Off..300 Hz | Sidechain high-pass; keeps bass from pumping the unit |
| Mix | 0..100% | Latency-aligned parallel compression blend |
| Output | -24..+12 dB | Clean output trim |
| AGC In | on/off | Takes the gain reduction in or out while keeping the tube/iron colour |

Global: Channel Mode (Left/Right, Lat/Vert (M/S), L/R Linked), Engine
(Eco / Zero Latency / Studio), Purist Mode, True Bypass.

Each channel strip shows a VU gain-reduction needle flanked by input and
output level meters (dBFS, with peak hold).

## Presets

Ten factory presets are exposed through the host's program interface (so they
appear in the DAW's preset menu) and via the preset bar in the UI: Init,
Vocal Leveler, Vocal Parallel, Drum Bus Smash, Drum Parallel Punch, Mix Glue,
Master Bus, Bass DI, Brick Limiter and Clean Color. Loading a preset resets
every sound parameter to its default and then applies the preset, so each one
is a complete, deterministic starting point. They are defined in
`Source/Presets.h`.

## Signal path

```
            +----------------------- per channel ------------------------+
            |                                                            |
 in --o-----+--> UTC A26 input   --> variable-mu     --> push-pull   --> Sowter output --+--> mix --> output trim --> out
      |     |    transformer model   gain stage          tube stage      transformer    |    ^
      |     |                          ^                     |                           |    |
      |     |                          | control voltage     | (feedback sidechain)     |    |
      |     |                  RC time-constant network <-- rectifier <-- SC high-pass <-+    |
      |     +------------------------------------------------------------+                   |
      +------------------------------- dry, latency-aligned --------------------------------->
```

The detector follows the hardware's feedback topology: it listens to the
*output* of the gain stage, which is what gives the 670 its soft knee and its
progressive ratio. The six positions of the time-constant switch are modeled
as one-, two- and three-branch capacitor networks, so positions 5 and 6
exhibit the famous "auto" release: fast recovery from single peaks, slow
recovery from dense programme.

## Building from source

Requirements: CMake 3.22+, a C++17 compiler. JUCE 8 is fetched automatically
at configure time.

Linux additionally needs the usual JUCE development packages:

```sh
sudo apt-get install -y libasound2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev libxinerama-dev \
  libxrandr-dev libxrender-dev
```

Then:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# headless DSP smoke tests
./build/TekkChild670Tests_artefacts/Release/TekkChild670Tests
```

The VST3 lands in `build/TekkChild670_artefacts/Release/VST3/`. Standalone is
built alongside; AU is produced on macOS.

## Project layout

```
CMakeLists.txt              JUCE 8 CMake project (VST3 / AU / Standalone + test app)
Source/
  PluginProcessor.*         parameter layout, routing, oversampling, dry/wet, presets
  PluginEditor.*            hardware-styled UI: GR needles, I/O meters, preset bar
  Parameters.h              parameter IDs
  Presets.h                 factory preset definitions
  DSP/
    TimeConstants.h         the six attack/release networks
    ControlVoltageNetwork.h multi-branch CV integrator (program-dependent release)
    Channel670.h            one full channel: iron -> mu stage -> tubes -> iron
    TubeStage.h             push-pull tube nonlinearity
    TransformerModel.h      LF-weighted core saturation, band edges
    SidechainFilter.h       TPT high-pass on the detector
Tests/
  SmokeTest.cpp             engine + anti-click contract tests, run in CI
.github/workflows/build.yml CI: Linux/macOS/Windows build + tests + pluginval + artifacts
```

## Validation & robustness

The plugin passes [pluginval](https://github.com/Tracktion/pluginval) at
strictness level 10 (real-time safety, parameter and state threading, bus
layouts, automation and parameter fuzzing), and CI runs it on every push.

Hardening that the headless tests lock in:

- **Click-free automation.** Threshold and DC Threshold are smoothed at the
  detector rate, so automating them glides instead of stepping per block.
- **Click-free switching.** Flipping the Time Constant selector seeds the
  capacitor branches to the held control voltage (the branch weights sum to 1
  at every position), so the gain reduction is continuous across the switch.
  Toggling AGC In ramps over ~15 ms rather than stepping.
- **Latency-aligned true bypass.** Bypass passes the signal unprocessed but
  delayed to match the engine's reported latency, so a host's delay
  compensation stays aligned (bit-exact in the zero-latency engine).
- **No host calls on the audio thread.** Engine-mode latency changes are
  reported from the message thread via an async update.

## Measurement & characterisation

`Tools/Measure.cpp` (target `TekkChild670Measure`) is an offline harness that
runs the real processing chain and writes CSV + SVG plots of the static
compression curve, THD vs level, THD vs frequency, and frequency response.
It needs no external plotting dependency and runs in CI.

```sh
cmake --build build --target TekkChild670Measure
./build/TekkChild670Measure_artefacts/Release/TekkChild670Measure measurements
```

See [`measurements/`](measurements/) for the plots and the current model's
headline numbers. These curves are the data you calibrate the DSP constants
against; today they confirm the intended behaviour (soft-knee progressive
ratio, low-frequency-weighted harmonics, flat response with a ~6 Hz roll-off).

## Engineering status

This repository contains the full plugin architecture: complete parameter
set, the variable-mu feedback engine with all six time-constant networks,
tube and transformer saturation stages, Lat/Vert (M/S) and linked operation,
three oversampling engine modes with correct latency reporting, a
latency-aligned blend, state save/recall and the UI. The per-stage drive and
CV-law constants in `Source/DSP/` are the calibration points for matching
measurements taken from reference hardware units.

## Licensing

The TekkChild TA-670 MK2 is a commercial product of Tekk Audio Engineering.
This source builds against [JUCE](https://juce.com), which is dual-licensed
(AGPLv3 / commercial) -- distribution of binaries must comply with the JUCE
license held by the company.

---

*If you don't love your TekkChild TA-670 MK2, we'll refund you -- every
purchase comes with a 30-day money back guarantee.*
