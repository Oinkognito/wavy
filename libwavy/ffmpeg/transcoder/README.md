# Audio Sample Sanitization and Artifact Suppression

This README will explain the `Audio Sanitization Job (ASJ)`, that is responsible for cleaning and conditioning raw audio samples decoded from media files using FFmpeg. The process aims to eliminate problematic values (e.g., NaNs or Infs), suppress unwanted high-frequency artifacts, and safely normalize all samples within the expected dynamic range.

## Purpose

When decoding audio from various sources (e.g., corrupted files, imperfect encoders, or aggressive psychoacoustic models like those in `libmp3lame`), the resulting raw samples may contain:
- Invalid values such as `NaN` or `Inf`
- High-frequency noise or "whistling" artifacts caused by quantization or psychoacoustic modeling
- Samples outside the valid range for audio rendering (e.g., beyond -1.0 to 1.0 in floating point)

To ensure clean audio output and compatibility with downstream processing or encoding, this sanitization step corrects these anomalies.

## Core Logic

### 1. Sample Format Handling

The main entry point is:

```cpp
void sanitize_audio_samples(AVFrame* frame);
```

This function detects the audio sample format of the input frame and dispatches it to the appropriate handler using `process_samples<T>()`, a templated function that works across various sample types:

- `AV_SAMPLE_FMT_FLT`, `AV_SAMPLE_FMT_FLTP` — float (interleaved/planar)
- `AV_SAMPLE_FMT_DBL`, `AV_SAMPLE_FMT_DBLP` — double (interleaved/planar)
- `AV_SAMPLE_FMT_S32`, `AV_SAMPLE_FMT_S32P` — 32-bit signed integer
- `AV_SAMPLE_FMT_S16`, `AV_SAMPLE_FMT_S16P` — 16-bit signed integer

Each format is converted to `float` for internal processing, then converted back.

### 2. Templated Processing

```cpp
template <typename T, typename F, typename R>
void process_samples(AVFrame* frame, F convert_to_float, R convert_from_float);
```

This templated function:
- Converts native audio samples to `float`
- Scans and modifies samples to remove invalid or extreme values
- Detects high-pitch artifacts based on amplitude and temporal change
- Applies soft clipping (`tanh`) for gentle compression of outliers
- Converts the cleaned samples back to the native format

### 3. High-Pitch Detection Heuristic

To suppress high-pitch or whistling artifacts, the module uses a simple heuristic:
- Samples with high absolute amplitude (greater than `0.85`)
- Combined with rapid fluctuations in a short window (`window_size = 5`)
- If average sample-to-sample change exceeds `0.5` in the window, it's flagged

Flagged samples are zeroed out to mute these audible artifacts.

Absolutely — tuning the parameters like **high amplitude threshold**, **window size**, and **average change threshold** will directly affect the sensitivity and behavior of your **high-pitch artifact detection**. Here's a detailed breakdown of what happens when each is adjusted:

## Parameter Behavior Explained

### a. **High Amplitude Threshold (`threshold = 0.85f`)**

This defines the **minimum amplitude** a sample must reach (in normalized `[-1.0, 1.0]` range) to be considered suspicious **before** checking for rapid changes.

#### If You **Lower** This Threshold (e.g., 0.7):
- **Effect**: More samples qualify for high-pitch checking.
- **Result**: The detection becomes **more sensitive**. Even moderately loud transients may be silenced.
- **Use case**: Noisier audio, or overcompressed sources with exaggerated highs.

#### If You **Raise** This Threshold (e.g., 0.95):
- **Effect**: Fewer samples are examined for rapid changes.
- **Result**: Detection becomes **less sensitive**. Some harsh high-pitched artifacts may slip through.
- **Use case**: Clean, dynamic sources where occasional spikes are natural.

### b. **Window Size (`window_size = 5`)**

This defines how many previous samples are considered when computing **temporal change** for high-pitch detection.

#### If You **Decrease** the Window (e.g., 3):
- **Effect**: Smaller snapshot of temporal behavior.
- **Result**: Responds to very brief high-frequency glitches, but may be **less stable** and **more prone to false positives**.
- **Use case**: Aggressive filtering of transient spikes or when data is extremely noisy.

#### If You **Increase** the Window (e.g., 10 or 20):
- **Effect**: Looks over a longer temporal span.
- **Result**: Only sustained rapid changes will trigger detection. Short noise bursts may **go undetected**.
- **Use case**: Music or speech where short bursts of high amplitude are intentional (e.g., percussion or fricatives in vocals).

### c. **Average Change Threshold (`change_threshold = 0.5f`)**

This defines how much **sample-to-sample amplitude difference** (on average) is required within the window to consider the change "rapid."

#### If You **Lower** the Threshold (e.g., 0.3):
- **Effect**: Smaller changes are flagged.
- **Result**: **Higher sensitivity** to rapid shifts, more aggressive silencing.
- **Risk**: May mute legitimate audio content with normal dynamics.

#### If You **Raise** the Threshold (e.g., 0.7 or higher):
- **Effect**: Only extreme and fast shifts are considered.
- **Result**: **More tolerant**, misses mild pitch or quantization noise.
- **Use case**: High-fidelity content where small shifts are natural and shouldn't be removed.

## Interaction Between Parameters

All three parameters work together. Here's how combinations behave:

| Threshold | Window Size | Change Threshold | Behavior Summary |
|-----------|-------------|------------------|------------------|
| Low       | Small       | Low              | Extremely sensitive, likely to catch and mute all high-pitched or transient noise — **risk of over-silencing** |
| High      | Large       | High             | Very relaxed — detects only long, loud, erratic oscillations — **risk of missing artifacts** |
| Moderate  | Moderate    | Moderate         | Balanced detection suitable for most content |
| Low       | Large       | Low              | Detects **sustained**, noisy oscillations — avoids false positives from short bursts |

### 4. Soft Clipping

```cpp
inline float soft_clip(float x)  { return std::tanh(x); }
inline double soft_clip(double x) { return std::tanh(x); }
```

Rather than hard clipping, the sanitization uses `tanh()` to gently compress samples that exceed the range `[-1.0, 1.0]`, preserving dynamics while avoiding distortion.

## Constants

To ensure clear and consistent conversions:

```cpp
constexpr float SCALE_FACTOR_32B = 1.0f / 2147483648.0f;
constexpr float SCALE_FACTOR_16B = 1.0f / 32768.0f;
constexpr float FLOAT_TO_INT32   = 2147483648.0f;
constexpr float FLOAT_TO_INT16   = 32768.0f;
```

These constants convert between integer audio formats and normalized floating-point representations.

## Logging and Diagnostics

When issues are detected, the module logs warnings:

- **Invalid Samples:** Samples with `NaN` or `Inf` are replaced with silence (`0.0`)
- **High-Pitch Artifacts:** Samples showing high-frequency noise are zeroed
- Each incident is logged with the format and sample index for debugging

## Use Case

This sanitization should be run **after decoding** and **before resampling or encoding**, to ensure:
- Stable and valid audio data
- Cleaner audio output
- Avoidance of encoder crashes or glitches due to invalid input
