# Resolver Encoder + X2Cscope

Resolver-style sine/cosine inputs on the dsPIC33AK128MC106 are sampled with ADC2, normalised, and converted to an angle via `atan2f`. Quadrature A/B/Z states are generated in software for the GUI (no GPIO drive yet). Everything stays visible over X2Cscope so the Python GUI can calibrate offsets/gains, adjust counts per revolution, and visualise live waveforms in real time.

## Firmware overview (`X2C-Scope-Blinky-dspic33AK128MC106-Curiosity.X/main.c`)

- Timer1 runs at 1 kHz. Each tick triggers ADC2 Channel0 (AN1) and Channel1 (AN4); raw samples land in `sin_raw` / `cos_raw` via ADC interrupts.
- Calibration applies `sin_offset` / `cos_offset` and `sin_amplitude` / `cos_amplitude`, with gain sanitisation to avoid divide-by-zero if the GUI writes 0.0.
- Angle `resolver_position` comes from `atan2f(sin_calibrated, cos_calibrated)`; A/B/Z states come from `counts_per_rev` in software only (no GPIO drive yet).
- X2Cscope: `process_samples()` calls `X2CScope_Update()` after each fresh pair so the GUI can read/write all exposed globals.

Key X2Cscope-visible symbols: `sin_raw`, `cos_raw`, `sin_offset`, `cos_offset`, `sin_amplitude`, `cos_amplitude`, `sin_calibrated`, `cos_calibrated`, `resolver_position`, `encoder_A/B/Z`, `counts_per_rev`, `sample_counter`.

## Python GUI (`ResolverEncoder.py`)

- Targets **PyQt5** for broader compatibility. Dependencies: `PyQt5`, `pyqtgraph`, `pyx2cscope`, `pyserial` (install with `pip install PyQt5 pyqtgraph pyx2cscope pyserial`).
- Usage: run `python ResolverEncoder.py`, pick the project ELF (e.g. `dist/default/production/X2C-Scope-Blinky-dspic33AK128MC106-Curiosity.X.production.elf`), choose the COM port, connect, and press **Calibrate** to auto-fill offsets/gains.
- The **Waveforms** window plots sine, cosine, and angle/pi with adjustable trigger and windowing. **Counts/rev** writes `counts_per_rev` live so you can sweep resolutions without reflashing.

## Build / flash

1. Open `X2C-Scope-Blinky-dspic33AK128MC106-Curiosity.X` in MPLAB X with the dsPIC33AK-MC DFP and XC-DSC toolchain configured.
2. Build and program the Curiosity board (`F11` / *Make and Program Device*); UART2 at 115200 baud is already wired for X2Cscope.
3. Launch the Python GUI or X2Cscope, load the ELF, and interact with the exported variables.

## What changed (and why)

- Replaced the FFT benchmark with a resolver-to-encoder data path that mirrors `ResolverEncoder.py` (angle computation, calibrated waveforms, ABZ generation in software, X2Cscope-friendly globals).
- Added guards against common field failures: gain sanitisation and counts-per-rev clamping. Angle uses standard `atan2f` for accuracy while still timing the trig path.
- Fixed the X2Cscope UART shim to include `uart2.h` (the active UART instance for this project).
- Ported the GUI to PyQt5 to avoid Qt6 deployment friction and kept all X2Cscope variable hookups intact. Timing readouts in the GUI refresh every ~3 s for readability.
