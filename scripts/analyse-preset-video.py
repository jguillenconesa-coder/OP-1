#!/usr/bin/env python3
"""Inspect the supplied 2021 demo locally; never publish its media.

Requires ffmpeg, ffprobe, Tesseract, Pillow and NumPy. The crop/highlight model
is specific to this 1280x720 recording. OCR produces candidates, not ground truth.
"""
import argparse
import collections
import concurrent.futures
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import statistics
import subprocess

import numpy as np
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]


def read_json(path):
    return json.loads(path.read_text())


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")


def probe(path, *options):
    return json.loads(subprocess.check_output(
        ["ffprobe", "-v", "error", *options, "-of", "json", str(path)]))


def normalized(text):
    return re.sub("[^a-z0-9]", "", text.lower())


def match_screen(image_path, categories):
    """Match the highlighted row using at least two visible list labels.

    Relative line positions tolerate vertical camera movement. Unreadable selected
    text can be inferred from adjacent labels and alphabetical inventory order;
    that inference is explicitly identified in the output.
    """
    pixels = np.array(Image.open(image_path).convert("RGB"))
    blue = np.median(pixels[0:90, 112:181, 2], axis=1)
    peak = float(blue.max())
    result = {"highlight_peak": peak, "highlight_y": None, "lines": [], "match": None}
    if peak <= 135:
        return result
    lo = hi = int(np.argmax(blue))
    threshold = max(135, peak - 30)
    while lo > 0 and blue[lo - 1] > threshold:
        lo -= 1
    while hi < len(blue) - 1 and blue[hi + 1] > threshold:
        hi += 1
    if hi - lo < 3:
        return result
    center = (lo + hi) / 2
    result["highlight_y"] = center
    image = Image.fromarray(pixels).crop((110, 0, 190, 96)).resize((640, 768))
    image = ImageOps.expand(image, 24, fill="black")
    ocr_path = image_path.with_suffix(".ocr.png")
    image.save(ocr_path)
    process = subprocess.run(
        ["tesseract", str(ocr_path), "stdout", "--psm", "6", "tsv"],
        capture_output=True, text=True, check=True,
        env={**os.environ, "OMP_THREAD_LIMIT": "1"})
    grouped = collections.defaultdict(list)
    for line in process.stdout.splitlines()[1:]:
        cells = line.split("\t")
        if len(cells) == 12 and cells[0] == "5" and cells[-1].strip():
            grouped[tuple(cells[1:5])].append(cells)
    for words in grouped.values():
        text = " ".join(word[11] for word in words)
        top = min(int(word[7]) for word in words)
        bottom = max(int(word[7]) + int(word[9]) for word in words)
        y = ((top + bottom) / 2 - 24) / 8
        relative_row = int(round((y - center) / 13))
        if abs(y - center - relative_row * 13) < 4.8:
            result["lines"].append({"relative_row": relative_row, "y": y,
                                    "text": text, "normalized": normalized(text)})
    candidates = []
    for category, items in categories.items():
        for selected in range(len(items)):
            scores = {}
            for line in result["lines"]:
                row = line["relative_row"]
                index = selected + row
                if 0 <= index < len(items):
                    score = difflib.SequenceMatcher(
                        None, line["normalized"], normalized(items[index]["name"])).ratio()
                    if score >= 0.62:
                        scores[row] = max(score, scores.get(row, 0))
            if len(scores) >= 2:
                candidates.append((sum(scores.values()) + len(scores) * 0.15,
                                   category, selected, scores))
    if not candidates:
        return result
    candidates.sort(reverse=True)
    score, category, selected, scores = candidates[0]
    margin = score - candidates[1][0] if len(candidates) > 1 else score
    mean = sum(scores.values()) / len(scores)
    if mean < 0.72 or margin < 0.15:
        return result
    preset = categories[category][selected]
    result["match"] = {
        "category": category, "name": preset["name"], "inventory_id": preset["id"],
        "method": ("selected-row OCR" if scores.get(0, 0) >= 0.8 else
                   "selected highlight + adjacent OCR / inventory order"),
        "matching_rows": len(scores), "mean_similarity": round(mean, 3),
        "margin": round(margin, 3),
        "selected_text": next((line["text"] for line in result["lines"]
                               if line["relative_row"] == 0), None)}
    return result


def inspect_audio(path):
    pcm = subprocess.check_output([
        "ffmpeg", "-nostdin", "-v", "error", "-threads", "1", "-i", str(path),
        "-map", "0:a:0", "-vn", "-ac", "2", "-ar", "44100", "-f", "f32le", "-"])
    audio = np.frombuffer(pcm, dtype="<f4").reshape(-1, 2)
    peak = np.max(np.abs(audio), axis=0)
    rms = np.sqrt(np.mean(np.square(audio.astype(np.float64)), axis=0))
    db = lambda values: [round(float(20 * np.log10(max(x, 1e-12))), 4) for x in values]
    return {"decoded_sample_frames": len(audio), "sample_rate": 44100, "channels": 2,
            "decoded_pcm_float32le_sha256": hashlib.sha256(pcm).hexdigest(),
            "peak_dbfs": db(peak), "rms_dbfs": db(rms),
            "nonfinite_samples": int(np.count_nonzero(~np.isfinite(audio))),
            "samples_at_or_above_full_scale": int(np.count_nonzero(np.abs(audio) >= 1))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--media-dir", type=Path, required=True,
                        help="Directory containing the supplied MP4s, searched recursively")
    parser.add_argument("--output", type=Path, required=True,
                        help="New private directory outside this checkout")
    parser.add_argument("--manifest", type=Path,
                        default=ROOT / "research/blackfin/preset-video-segments.json")
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--validate-only", action="store_true",
                        help="Verify input file hashes without extracting frames/audio")
    args = parser.parse_args()
    if args.workers < 1 or args.workers > 8:
        parser.error("workers must be between 1 and 8")
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or output.exists():
        parser.error("output must be a new private directory outside the checkout")
    manifest = read_json(args.manifest)
    files = collections.defaultdict(list)
    for path in args.media_dir.rglob("*.mp4"):
        files[path.name].append(path)
    media = []
    for segment in manifest["segments"]:
        matches = files[segment["filename"]]
        if len(matches) != 1:
            parser.error(f"Expected exactly one file: {segment['filename']}")
        path = matches[0]
        if (path.stat().st_size != segment["size_bytes"] or
                hashlib.sha256(path.read_bytes()).hexdigest() != segment["sha256"]):
            parser.error(f"File does not match the supplied batch: {path.name}")
        media.append((segment, path))
    if args.validate_only:
        print(f"Verified {len(media)} supplied file hashes")
        return
    output.mkdir(parents=True)
    versions = {name: subprocess.check_output([name, "-version" if name != "tesseract"
                                               else "--version"], text=True).splitlines()[0]
                for name in ["ffmpeg", "ffprobe", "tesseract"]}
    write_json(output / "tool-versions.json", versions)
    inventory = read_json(ROOT / "research/blackfin/factory-preset-inventory.json")["presets"]
    categories = {}
    for preset in inventory:
        if preset["table"] == "synth_presets":
            categories.setdefault(preset["type"], []).append(preset)
    for items in categories.values():
        items.sort(key=lambda preset: preset["name"].lower())
    previous_packets = {}
    frames = []
    measurements = []
    for segment, path in media:
        number = segment["segment"]
        info = probe(path, "-show_streams", "-show_format")
        video = next(stream for stream in info["streams"] if stream["codec_type"] == "video")
        if (video["width"], video["height"], video["r_frame_rate"]) != (1280, 720, "30/1"):
            raise ValueError(f"Unexpected video geometry/rate in segment {number}")
        packets = probe(path, "-select_streams", "v:0", "-show_packets", "-show_data_hash",
                        "sha256", "-show_entries", "packet=pts_time,data_hash")["packets"]
        current = {packet["data_hash"]: float(packet["pts_time"])
                   for packet in packets if "pts_time" in packet}
        deltas = [previous_packets[key] - value for key, value in current.items()
                  if key in previous_packets]
        offset = 0.0
        if number != 1:
            if len(deltas) < 2 or max(deltas) - min(deltas) >= 0.00001:
                raise ValueError(f"Cannot align segment {number} using shared video packets")
            offset = statistics.median(deltas)
        if abs(offset - segment["global_video_pts_offset_seconds"]) > 0.00001:
            raise ValueError(f"Alignment differs from recorded evidence: segment {number}")
        previous_packets = {key: value + offset for key, value in current.items()}
        directory = output / f"seg{number:02}"
        directory.mkdir()
        subprocess.run([
            "ffmpeg", "-nostdin", "-v", "error", "-threads", "1", "-i", str(path),
            "-vf", r"select=not(mod(n\,60)),crop=200:96:413:184", "-fps_mode", "vfr",
            "-threads", "1", str(directory / "frame-%04d.png")], check=True)
        start = float(video["start_time"])
        for image in sorted(directory.glob("frame-[0-9][0-9][0-9][0-9].png")):
            local = start + (int(image.stem.split("-")[1]) - 1) * 2
            frames.append((number, image, local, offset + local))
        measurements.append({"segment": number, "shared_packet_anchors": len(deltas),
                             "global_video_pts_offset_seconds": offset,
                             "audio_decode": inspect_audio(path)})
        print(f"Prepared segment {number}", flush=True)

    def inspect_frame(job):
        number, image, local, global_time = job
        result = {"segment": number, "frame": str(image),
                  "local_seconds": round(local, 6), "global_seconds": round(global_time, 6),
                  **match_screen(image, categories)}
        write_json(image.with_suffix(".json"), result)
        return result

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        results = list(pool.map(inspect_frame, frames))
    write_json(output / "frames.json", results)
    write_json(output / "measurements.json", measurements)
    print(f"Inspected {len(results)} sampled frames; candidates require visual review")


if __name__ == "__main__":
    main()
