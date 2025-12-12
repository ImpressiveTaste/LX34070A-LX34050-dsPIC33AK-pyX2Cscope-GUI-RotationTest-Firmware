# LX34070A-LX34050-dsPIC33AK-pyX2Cscope-GUI

**Last updated:** December 12, 2025

This repository contains an MPLAB X project (`X2C-Scope-Blinky-dspic33AK128MC106-Curiosity.X`) and a companion Digital Twin App Python GUI (`ResolverEncoder.py`) that turn dsPIC33AK ADC inputs into a software quadrature encoder. The firmware runs on Microchip's dsPIC33AK128MC106 ([EV02G02A](https://www.microchip.com/en-us/development-tool/EV02G02A)) using the Curiosity board ([EV74H48A](https://www.microchip.com/en-us/development-tool/ev74h48a)). LX34050 or LX34070A (Absolute Inductive Position Sensor IC) sine/cosine signals are sampled, calibrated, converted to angle with `atan2f`, and exposed to both X2Cscope and the GUI so you can tune offsets/gains and counts-per-rev without reflashing.

Advantages of this sensor: no need of a magnet as a target—just a metallic target like a copper disk or a PCB!
https://www.microchip.com/en-us/products/sensors-and-motor-drive/inductive-position-sensors

## Repository Layout

```
LX34070A-LX34050-dsPIC33AK-pyX2Cscope-GUI/
├── README.md                  <-- you are here
├── X2C-Scope-Blinky-dspic33AK128MC106-Curiosity.X/
│   ├── main.c                 <-- resolver/encoder data path + X2Cscope hooks
│   ├── nbproject/             <-- MPLAB X configuration
│   ├── mcc_generated_files/   <-- Melody-generated drivers (Timer1, ADC2, UART2)
│   └── ...
├── ResolverEncoder.py         <-- PyQt5 GUI for calibration and plotting
├── app/                       <-- auxiliary GUI assets/config (if any)
├── spinning_logo_rpm_red.ico  <-- GUI icon
└── pyx2cscope.x2cscope.log    <-- X2Cscope session log (optional)
```

Everything you need to rebuild, modify, and run the resolver-to-encoder demo lives in `X2C-Scope-Blinky-dspic33AK128MC106-Curiosity.X` plus the Python GUI at the repo root.

## Video walkthrough

[![Resolver Encoder Demo](https://img.youtube.com/vi/ZiPm2cT6u0s/maxresdefault.jpg)](https://youtu.be/ZiPm2cT6u0s)

## Requirements

- **Hardware:** dsPIC33AK128MC106 on the Curiosity board ([EV02G02A](https://www.microchip.com/en-us/development-tool/EV02G02A) + [EV74H48A](https://www.microchip.com/en-us/development-tool/ev74h48a)), LX34050 or LX34070A, USB-to-serial (pkob4 on-board).
- **Toolchain:**
  - MPLAB X IDE v6.25 or later
  - XC-DSC compiler v3.21 or newer
  - dsPIC33AK-MC_DFP v1.2.125 (already referenced by the project)
- **Host tooling:**
  - X2Cscope for Windows/Linux ([x2cscope.github.io](https://x2cscope.github.io/))
  - Python 3.9+ with `pyx2cscope` (install via `pip install pyx2cscope`)

## Setup Instructions

1. **Clone or download** this repository to your development machine.
2. **Open the MPLAB X project:**
   - Launch MPLAB X IDE.
   - *File → Open Project...* and select `X2C-Scope-Blinky-dspic33AK128MC106-Curiosity.X`.
3. **Check toolchain paths:**
   - Ensure XC-DSC and the dsPIC33AK DFP are detected (`Project Properties → XC-DSC`).
4. **Build the firmware:**
   - Press `F11` (*Build Project*) to generate `dist/default/production/X2C-Scope-Blinky-dspic33AK128MC106-Curiosity.X.production.hex`.
5. **Flash the board:**
   - Connect the Curiosity board over USB and use *Make and Program Device*.
6. **Start a host tool:**
   - Use X2Cscope or run `python ResolverEncoder.py` to connect, load the ELF, and view the exported variables.

## Using the Python GUI (`ResolverEncoder.py`)

- **Install deps:** `pip install pyx2cscope`.
- **Run it:** `python ResolverEncoder.py`, choose the ELF (e.g. `dist/default/production/...production.elf`), select the COM port, and connect.
- **Calibrate:** Click **Calibrate** to auto-populate offsets/gains based on the live resolver waveform. When running autocalibration, please spin the rotor/target until calibration is completed.
- **Plotting:** The **Waveforms** tab shows sine, cosine, and angle/pi with adjustable trigger and windowing. **Counts/rev** lets you sweep resolutions without reflashing.

## Connecting X2Cscope (more difficult than just using the GUI app provided)

1. Launch X2Cscope and set the COM port for UART2 (115200-8-N-1).
2. Import `dist/default/production/X2C-Scope-Blinky-dspic33AK128MC106-Curiosity.X.production.elf` so symbols are visible.
3. Add key signals to watch/plot:
   - `sin_raw`, `cos_raw`
   - `sin_offset`, `cos_offset`, `sin_amplitude`, `cos_amplitude`
   - `sin_calibrated`, `cos_calibrated`, `resolver_position`
   - `encoder_A`, `encoder_B`, `encoder_Z`
   - `counts_per_rev`, `sample_counter`
4. Press *Run* to stream live data; edits to offsets/gains/`counts_per_rev` take effect immediately.

## How It Works (firmware)

- Timer1 runs at 1 kHz and triggers ADC2 Channel0 (AN1) and Channel1 (AN4).
- ADC ISRs deposit raw samples into `sin_raw` / `cos_raw`.
- `process_samples()` applies offsets/gains, sanitises gains to avoid divide-by-zero, computes `resolver_position = atan2f(sin_calibrated, cos_calibrated)`, and derives A/B/Z from `counts_per_rev`.
- `X2CScope_Update()` publishes all globals each tick so X2Cscope/GUI can read and write them in real time.

## Customizing / Extending

- Change `counts_per_rev` live to match your mechanical resolution or to test how the A/B/Z logic behaves at different grid sizes.
- Add GPIO drive for A/B/Z if you need physical quadrature outputs; the software states are already available.
- Swap the 1 kHz cadence or ADC channels in Melody to match your hardware; the rest of the data path is cadence-agnostic.
- Log waveforms in the GUI for offline analysis or feed them into other Python tooling.

## Troubleshooting

| Issue | Fix |
|-------|-----|
| No data in X2Cscope / GUI | Confirm UART2 COM port and 115200-8-N-1. Ensure `SYSTEM_Initialize()` enables UART2 and ADC2. |
| Angle jumps or drifts | Re-run **Calibrate** to refresh offsets/gains; verify resolver amplitude is within ADC range. |
| Counts per rev feels wrong | Adjust `counts_per_rev` live; values are clamped to sane ranges to prevent divide-by-zero. |
| Build fails on dependencies | Verify XC-DSC path and dsPIC33AK DFP in `Project Properties`; regenerate Melody drivers if hardware pins change. |

## References & Useful Links

- [X2Cscope documentation](https://x2cscope.github.io/)
- [pyx2cscope on PyPI](https://pypi.org/project/pyx2cscope/)
- [XC-DSC Compiler User’s Guide](https://ww1.microchip.com/downloads/en/DeviceDoc/50002441E.pdf)
- [dsPIC33AK128MC106 Curiosity board](https://ww1.microchip.com/downloads/en/DeviceDoc/DS70005475A.pdf)

Feel free to fork the repo, add hardware A/B/Z drive, and share results—contributions are welcome!
