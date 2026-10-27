#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
"""Conditional spectral pilots against private demo excerpts, not fidelity scores.

Requires NumPy, SciPy, Matplotlib, ffmpeg and preset_study_render. Media and
rendered WAVs stay in a new private output directory outside the checkout.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import subprocess

import numpy as np
from scipy.io import wavfile
from scipy.signal import find_peaks

ROOT = Path(__file__).resolve().parents[1]
RATE = 44100


def spectral_features(signal, rate, nominal, harmonics):
    """Measure an explicit pitch hypothesis; reject unsuitable harmonic grids."""
    x = np.asarray(signal, dtype=np.float64)
    if len(x) < rate // 8 or not np.all(np.isfinite(x)):
        raise ValueError("Need at least 125 ms of finite audio")
    x = x - np.mean(x)
    if np.sqrt(np.mean(x * x)) < 1e-10:
        raise ValueError("Silent signal cannot identify a pitch or harmonic grid")
    window = np.hanning(len(x))
    fft_size = 1 << int(math.ceil(math.log2(len(x) * 16)))
    power = np.abs(np.fft.rfft(x * window, n=fft_size)) ** 2
    frequency = np.fft.rfftfreq(fft_size, 1 / rate)
    search = np.flatnonzero((frequency >= nominal * 2 ** (-35 / 1200)) &
                            (frequency <= nominal * 2 ** (35 / 1200)))
    peak = int(search[np.argmax(power[search])])
    log_power = np.log(np.maximum(power, 1e-60))
    denominator = log_power[peak - 1] - 2 * log_power[peak] + log_power[peak + 1]
    fractional = (0.5 * (log_power[peak - 1] - log_power[peak + 1]) / denominator
                  if abs(denominator) > 1e-20 else 0)
    f0 = float((peak + np.clip(fractional, -0.5, 0.5)) * rate / fft_size)
    half_width = max(3.0, rate / len(x) * 2.5)
    mask = np.zeros(len(power), dtype=bool)
    harmonic_power = []
    for harmonic in range(1, harmonics + 1):
        region = np.abs(frequency - f0 * harmonic) <= half_width
        harmonic_power.append(float(np.sum(power[region])))
        mask |= region
    band = ((frequency >= nominal / 2) &
            (frequency <= min((harmonics + 0.5) * nominal, 10000)))
    total = float(np.sum(power[band]))
    residual = float(np.sum(power[band & ~mask]) / max(total, 1e-60))
    fundamental_db = float(10 * np.log10(max(power[peak], 1e-60) /
                                       max(float(np.max(power[band])), 1e-60)))
    levels = 10 * np.log10(np.maximum(harmonic_power, 1e-60) /
                           max(harmonic_power[0], 1e-60))
    peaks, _ = find_peaks(power)
    peaks = [p for p in peaks if 40 <= frequency[p] <= 10000]
    strongest = sorted(peaks, key=lambda p: -power[p])[:8]
    return {
        "f0_hypothesis_hz": round(f0, 6),
        "cents_from_hypothesized_equal_tempered_note": round(1200 * math.log2(f0 / nominal), 4),
        "harmonic_levels_db_relative_to_first": np.round(levels, 4).tolist(),
        "nonharmonic_band_energy_fraction": round(residual, 6),
        "fundamental_relative_to_strongest_peak_db": round(fundamental_db, 4),
        "harmonic_grid_compatible": bool(residual <= 0.15 and fundamental_db >= -12),
        "grid_gate_scope": "Heuristic eligibility gate, not proof of one voice or a recovered MIDI note",
        "harmonic_band_half_width_hz": round(half_width, 6),
        "rms_dbfs": round(float(20 * np.log10(max(np.sqrt(np.mean(x * x)), 1e-30))), 4),
        "strongest_peaks": [{"hz": round(float(frequency[p]), 4),
                              "db_relative_to_peak": round(float(10 * np.log10(
                                  power[p] / power[strongest[0]])), 4)} for p in strongest]}


def self_test():
    t = np.arange(RATE) / RATE
    tone = np.sin(2 * np.pi * 110 * t) + 0.3 * np.sin(2 * np.pi * 220 * t)
    measured = spectral_features(tone, RATE, 110, 12)
    assert measured["harmonic_grid_compatible"]
    assert abs(measured["cents_from_hypothesized_equal_tempered_note"]) < 0.1
    assert abs(measured["harmonic_levels_db_relative_to_first"][1] - 20 * math.log10(0.3)) < 0.1
    scaled = spectral_features(tone * 0.017, RATE, 110, 12)
    assert np.allclose(measured["harmonic_levels_db_relative_to_first"][:2],
                       scaled["harmonic_levels_db_relative_to_first"][:2], atol=0.001)
    chord = np.sin(2 * np.pi * 110 * t) + np.sin(2 * np.pi * 137.5 * t)
    assert not spectral_features(chord, RATE, 110, 12)["harmonic_grid_compatible"]
    try:
        spectral_features(np.zeros(RATE), RATE, 110, 12)
    except ValueError:
        pass
    else:
        raise AssertionError("Silence accepted as a pitch")
    print("PASS: analytic pitch, harmonic amplitude, gain invariance, chord and silence rejection")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--media-dir", type=Path)
    parser.add_argument("--renderer", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not all((args.media_dir, args.renderer, args.output)):
        parser.error("media-dir, renderer and output are required")
    output = args.output.resolve()
    if output.exists() or output.is_relative_to(ROOT):
        parser.error("output must be new and outside the checkout")
    cases_path = ROOT / "research/blackfin/preset-audio-cases.json"
    manifest_path = ROOT / "research/blackfin/preset-video-segments.json"
    cases = json.loads(cases_path.read_text())["cases"]
    manifest = {s["segment"]: s for s in json.loads(manifest_path.read_text())["segments"]}
    media = {}
    for number in sorted({case["segment"] for case in cases}):
        segment = manifest[number]
        files = list(args.media_dir.rglob(segment["filename"]))
        if len(files) != 1 or hashlib.sha256(files[0].read_bytes()).hexdigest() != segment["sha256"]:
            parser.error(f"Missing, ambiguous or different segment {number}")
        pcm = subprocess.check_output([
            "ffmpeg", "-nostdin", "-v", "error", "-i", str(files[0]),
            "-map", "0:a:0", "-vn", "-ac", "2", "-ar", str(RATE), "-f", "f32le", "-"])
        media[number] = np.frombuffer(pcm, dtype="<f4").reshape(-1, 2)
    output.mkdir(parents=True)
    results = []
    for case in cases:
        offset = manifest[case["segment"]]["global_video_pts_offset_seconds"]
        begin = int((case["video_start_seconds"] - offset) * RATE)
        end = int((case["video_end_seconds"] - offset) * RATE)
        stereo = media[case["segment"]][begin:end]
        if len(stereo) != end - begin:
            raise ValueError("Reference window outside decoded audio")
        reference = stereo.mean(axis=1)
        path = output / (case["id"] + ".wav")
        subprocess.run([str(args.renderer.resolve()), str(path), str(RATE), case["family"],
                        str(case["preset_index"]), str(case["hypothesized_midi_note"]),
                        str(case["render_velocity"]), str(int(case["render_gate_seconds"] * RATE)),
                        str(int(case["render_duration_seconds"] * RATE))], check=True)
        rate, rendered = wavfile.read(path)
        if rate != RATE or rendered.ndim != 1 or rendered.dtype != np.float32:
            raise ValueError("Unexpected renderer WAV format")
        start = int(case["render_window_start_seconds"] * rate)
        rendered = rendered[start:start + len(reference)]
        nominal = 440 * 2 ** ((case["hypothesized_midi_note"] - 69) / 12)
        ref = spectral_features(reference, RATE, nominal, case["harmonics"])
        synth = spectral_features(rendered, RATE, nominal, case["harmonics"])
        eligible = ref["harmonic_grid_compatible"] and synth["harmonic_grid_compatible"]
        if case.get("expect_spectral_rejection") and eligible:
            raise AssertionError("Polyphonic negative control was incorrectly accepted")
        delta = np.array(synth["harmonic_levels_db_relative_to_first"]) - np.array(
            ref["harmonic_levels_db_relative_to_first"])
        results.append({"case": case, "reference": ref, "core_render": synth,
                        "stereo_channel_correlation": round(float(np.corrcoef(stereo.T)[0, 1]), 8),
                        "render_wav_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                        "spectral_comparison_eligible": bool(eligible),
                        "render_minus_reference_harmonics_db": np.round(delta, 4).tolist() if eligible else None})
    report = {"schema": 1, "scope": "Conditional harmonic measurements, not a fidelity percentage or MIDI mapping validation",
              "render_scope": "Engine + stored oscillator/common ADSR + own output/rate adapter; all FX/LFO and PluginProcessor master/EQ/tape omitted",
              "runtime_dsp_changed": False, "envelope_note_on_off_timing_validated": False,
              "reference_media_redistributed": False,
              "cases_sha256": hashlib.sha256(cases_path.read_bytes()).hexdigest(),
              "manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
              "renderer_source_sha256": hashlib.sha256((ROOT / "Tools/PresetStudyRender.cpp").read_bytes()).hexdigest(),
              "analysis_source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "sample_rate": RATE, "results": results}
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    os.environ.setdefault("MPLCONFIGDIR", str(output / "matplotlib-cache"))
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    accepted = [result for result in results if result["spectral_comparison_eligible"]]
    if not accepted:
        print(json.dumps({"cases": len(results), "eligible": 0, "output": str(output)}))
        return
    figure, axes = plt.subplots(len(accepted), 1, figsize=(10, 4 * len(accepted)), squeeze=False)
    for axis, result in zip(axes[:, 0], accepted):
        case = result["case"]
        harmonics = np.arange(1, case["harmonics"] + 1)
        axis.plot(harmonics, result["reference"]["harmonic_levels_db_relative_to_first"], "o-", label="Video reference")
        axis.plot(harmonics, result["core_render"]["harmonic_levels_db_relative_to_first"], "s-", label="Current CORE render")
        axis.set(title=f"{case['family']} / {case['name']} — conditional note hypothesis {case['hypothesized_midi_note']}",
                 xlabel="Harmonic number", ylabel="dB relative to first harmonic", ylim=(-65, 8))
        axis.grid(alpha=0.25)
        axis.legend()
    figure.suptitle("Spectral pilot: unknown reference chain / incomplete patch routes; no DSP retuning", fontsize=11)
    figure.tight_layout(rect=(0, 0, 1, 0.96))
    figure.savefig(output / "comparison.svg")
    figure.savefig(output / "comparison.png", dpi=140)
    plt.close(figure)
    print(json.dumps({"cases": len(results), "eligible": len(accepted), "output": str(output)}))


if __name__ == "__main__":
    main()
