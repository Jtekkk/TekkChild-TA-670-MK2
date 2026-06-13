# Characterisation measurements

These plots are produced by the offline harness (`Tools/Measure.cpp`,
target `TekkChild670Measure`). They characterise the **current model** by
running the real processing chain at 96 kHz and analysing the output, so the
plugin's behaviour can be checked against reference-hardware measurements
rather than asserted.

Regenerate them with:

```sh
cmake --build build --target TekkChild670Measure
./build/TekkChild670Measure_artefacts/Release/TekkChild670Measure measurements
```

Each measurement is written as both an `.svg` (for viewing) and a `.csv` (for
analysis / comparison against hardware captures).

| File | What it shows |
| --- | --- |
| `static_curve` | Steady-state output vs input level at THRESHOLD 3/5/7 (time-constant position 1). The soft knee and progressive variable-mu ratio. |
| `thd_vs_level` | THD vs input level at 1 kHz with the compressor out — the tube + transformer colour in isolation. |
| `thd_vs_frequency` | THD vs frequency at −6 dBFS, compressor out. Tests the "iron saturates low frequencies first" claim. |
| `frequency_response` | Magnitude vs frequency at −30 dBFS (linear regime) — the band edges from the transformer models and DC blockers. |

## Current model — headline numbers

- **Compression curve.** Below threshold the curve tracks unity; the knee is
  soft and the ratio increases progressively with level rather than holding a
  fixed slope (≈13 dB of gain reduction by +6 dBFS at THRESHOLD 5, more at
  higher settings as the loop approaches its ~22 dB ceiling), as expected from
  a feedback variable-mu topology that limits rather than clips.
- **Harmonic profile.** THD is low at nominal and rises smoothly when the unit
  is pushed (≈0.1% at −20 dBFS, ≈0.3–1% around nominal, ≈3.7% at 0 dBFS, ≈11%
  by +6 dBFS at 1 kHz). The distortion is **low-frequency weighted** — ≈4% at
  20 Hz versus ≈1% at 1 kHz at −6 dBFS — because the transformer core saturates
  the low band first. With the compressor in circuit it thickens further as
  gain reduction increases, since the tube is biased harder toward cutoff.
- **Frequency response.** Essentially flat across the audio band with gentle
  roll-offs at both extremes: a ~6 Hz high-pass at the bottom (transformer
  open-circuit inductance / DC blocking; −3 dB near 13 Hz) and a soft top-end
  roll-off from the output-transformer bandwidth (≈−2 dB at 20 kHz).

> These are best-estimate characterisations of the model, grounded in the
> documented operation of a Fairchild 670 (variable-mu feedback gain control,
> LF-weighted iron saturation, program-dependent time constants) rather than a
> bench capture of a specific unit. The per-stage drive levels and the
> gain-reduction law in `Source/DSP/` remain the calibration points should
> reference-hardware curves become available.
