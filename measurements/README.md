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
  soft and the ratio increases progressively with level (≈16 dB of gain
  reduction by +6 dBFS at THRESHOLD 5), as expected from a feedback variable-mu
  topology.
- **Harmonic profile.** THD rises smoothly with level (≈0.03% at −36 dBFS to
  ≈11% at +6 dBFS at 1 kHz). The distortion is **low-frequency weighted**:
  ≈2.6% at 20–30 Hz versus ≈1.3% above ~200 Hz at −6 dBFS, because the
  transformer core saturates the low band first.
- **Frequency response.** Flat across the audio band with a gentle ~6 Hz
  high-pass roll-off (transformer open-circuit inductance / DC blocking);
  −3 dB near 13 Hz, essentially flat from 50 Hz to Nyquist.

> These are the model as it stands. The per-stage drive levels and the
> CV-to-gain-reduction law in `Source/DSP/` are the calibration points: when
> reference-hardware curves are available, tune those constants and re-run the
> harness to compare.
