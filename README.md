# AudioToMMP

*Replaces BasslineAnalyser, which was the simple first iteration.*

**AudioToMMP** is a WIP audio analysis and transcription tool designed to bridge the gap between raw audio files and LMMS. It uses digital signal processing (DSP) to break down mixed audio, detect pitches and rhythms, and translate them directly into actionable pattern files (`.xpt`), and full multi-track LMMS projects (`.mmp`) for LMMS. 

### Core Features

* **Multi-Band Project Mode (Audio to `.mmp`):** Define specific frequency bands (e.g., Bass, Mids, Highs) and let the app scan the entire song. It will automatically transcribe the detected notes for each band and generate a complete, multi-track LMMS project file ready for editing.
* **Pattern Mode:** Click and drag a selection box over a specific part of the spectrogram to isolate an instrument, melody, or bassline. The app detects the exact MIDI notes or chords (using algorithms like YIN or Harmonic Product Spectrum) and maps them onto a 16-step or 32-step pattern grid based on the BPM. This grid can be exported as an LMMS `.xpt` pattern.
* **DSP Source Separation:** A dedicated module to isolate or remove specific elements (like basslines or vocals) from the mix using cascaded, steep Biquad filters. You can play back the separated audio or save it to disk.
* **Spectral Sampling:** Drag an area on the spectrogram (time × frequency) to apply a bandpass filter, isolating that exact sound, and export it as a ready-to-use `.wav` sample.
* **Visual Data:** Features detailed spectrogram and waveform visualisers to help you see exactly what you are extracting.

### Current State
Functional note detection, offset/BPM detection, and full `.mmp` generation is implemented. Chord detection (Template Matching and Peak Picking) is currently active but needs further refinement.

---

### Third-Party Libraries
This project relies on the following open-source libraries:

#### QCustomPlot
* **Description:** Used for plotting the interactive audio waveforms, spectrogram maps, and visual grid selections.
* **Author:** Emanuel Eichhammer
* **License:** GPLv3
* **Source:** https://www.qcustomplot.com/

#### KissFFT
* **Description:** A simple, lightweight Fast Fourier Transform (FFT) library used to generate the spectrogram visuals and power the frequency-based pitch/chord detection algorithms.
* **Author:** Mark Borgerding
* **License:** BSD-3-Clause 
* **Source:** https://github.com/mborgerding/kissfft

#### miniaudio
* **Description:** A lightweight, single-file C library used for decoding and loading audio files (WAV, MP3, FLAC) into raw PCM float data for DSP analysis, as well as handling audio playback and exporting processed `.wav` files.
* **Author:** David Reid (mackron)
* **License:** Public Domain (Unlicense) / MIT-0
* **Source:** https://github.com/mackron/miniaudio
