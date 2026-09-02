#!/usr/bin/env python3
"""
AstralEngine - Automated Benchmark Matrix Runner (PR-3 to PR-8)
Runs benchmark matrices across resolutions (720p, 1080p), normal stencils (Central vs Tetrahedron),
spatial acceleration (Grid ON vs Grid OFF), shadow modes, and TAA modes (TAA ON vs TAA OFF).
"""

import sys
import os
import argparse
import subprocess
import json
import csv
from pathlib import Path

PRESETS = {
    "720p": {"width": 1280, "height": 720, "desc": "1280x720 HD"},
    "1080p": {"width": 1920, "height": 1080, "desc": "1920x1080 Full HD"},
    "1440p": {"width": 2560, "height": 1440, "desc": "2560x1440 QHD"},
}

NORMAL_MODES = {
    "central": {"label": "Central (6-tap)", "arg": "central"},
    "tetra": {"label": "Tetrahedron (4-tap)", "arg": "tetra"},
}

def find_binary(custom_path=None):
    if custom_path and Path(custom_path).is_file():
        return Path(custom_path).resolve()

    candidates = [
        Path("build/Sandbox.exe"),
        Path("build-release/Sandbox.exe"),
        Path("build/Debug/Sandbox.exe"),
        Path("build/Release/Sandbox.exe"),
        Path("build-msvc/Debug/Sandbox.exe"),
        Path("build-msvc-release/Release/Sandbox.exe"),
        Path("build/Sandbox"),
    ]

    for cand in candidates:
        if cand.is_file():
            return cand.resolve()

    raise FileNotFoundError(
        "Sandbox binary bulunamadi! Once projeyi derleyin: cmake --build --preset mingw-debug"
    )

def run_single_benchmark(bin_path, preset_name, preset_info, normal_mode, use_grid, opt_shadow, enable_taa, is_stress, frames, out_dir):
    grid_tag = "grid" if use_grid else "nogrid"
    shadow_tag = "optshd" if opt_shadow else "unoptshd"
    taa_tag = "taa" if enable_taa else "notaa"
    stress_tag = "_stress" if is_stress else ""
    tag = f"{preset_name}_{normal_mode}_{grid_tag}_{shadow_tag}_{taa_tag}{stress_tag}"
    csv_file = out_dir / f"bench_{tag}.csv"
    json_file = out_dir / f"bench_{tag}.json"

    cmd = [
        str(bin_path),
        "--bench",
        "--bench-frames", str(frames),
        "--width", str(preset_info["width"]),
        "--height", str(preset_info["height"]),
        "--normal", str(normal_mode),
        "--bench-out", str(csv_file)
    ]

    if use_grid:
        cmd.append("--grid")
    else:
        cmd.append("--no-grid")

    if opt_shadow:
        cmd.append("--opt-shadow")
    else:
        cmd.append("--no-opt-shadow")

    if enable_taa:
        cmd.append("--taa")
    else:
        cmd.append("--no-taa")

    if is_stress:
        cmd.append("--stress")

    grid_label = "Grid ON (Two-Level)" if use_grid else "Grid OFF (Brute Force)"
    shadow_label = "Golge Opt ACIK" if opt_shadow else "Kaba Golge"
    taa_label = "TAA ACIK (Halton+Clamp)" if enable_taa else "TAA KAPALI"
    norm_label = NORMAL_MODES[normal_mode]["label"]
    scene_label = "32 Nesneli Stress" if is_stress else "Standart"

    print(f"\n[BenchRunner] Calistiriliyor: {preset_name} | {scene_label} | {grid_label} | {shadow_label} | {taa_label} | {norm_label} - {frames} kare...")
    result = subprocess.run(cmd, capture_output=True, text=True)

    if result.returncode != 0:
        print(f"[BenchRunner HATA]: {tag} basarisiz oldu! (Hata kodu: {result.returncode})")
        print(result.stderr)
        return None

    if not json_file.exists():
        print(f"[BenchRunner UYARI]: JSON ozet dosyasi bulunamadi: {json_file}")
        return None

    with open(json_file, "r", encoding="utf-8") as f:
        data = json.load(f)

    return {
        "tag": tag,
        "preset": preset_name,
        "normal_mode": normal_mode,
        "normal_label": norm_label,
        "use_grid": use_grid,
        "grid_label": grid_label,
        "opt_shadow": opt_shadow,
        "shadow_label": shadow_label,
        "enable_taa": enable_taa,
        "taa_label": taa_label,
        "is_stress": is_stress,
        "scene_label": scene_label,
        "width": preset_info["width"],
        "height": preset_info["height"],
        "frames": frames,
        "csv_file": str(csv_file),
        "json_file": str(json_file),
        "data": data
    }

def print_comparison_table(results):
    print("\n" + "=" * 135)
    print("               ASTRAL ENGINE MATRIS BENCHMARK KARSILASTIRMASI (PR-8)")
    print("=" * 135)
    print(f"{'Preset':<8} | {'Sahne':<18} | {'Hizlandirma':<20} | {'Golge Modu':<16} | {'Anti-Aliasing':<24} | {'GPU Ort(ms)':<12} | {'GPU p50':<10} | {'CPU Ort':<10}")
    print("-" * 135)

    for r in results:
        m = r["data"]["metrics"]
        gpu_avg = m["gpu_total_ms"]["avg"]
        gpu_p50 = m["gpu_total_ms"]["p50"]
        cpu_avg = m["cpu_frame_ms"]["avg"]

        print(f"{r['preset']:<8} | {r['scene_label']:<18} | {r['grid_label']:<20} | {r['shadow_label']:<16} | {r['taa_label']:<24} | {gpu_avg:<12.3f} | {gpu_p50:<10.3f} | {cpu_avg:<10.3f}")

    print("=" * 135 + "\n")

def save_matrix_summary(results, out_dir):
    summary_json = out_dir / "matrix_summary.json"
    summary_csv = out_dir / "matrix_summary.csv"

    with open(summary_json, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    print(f"[BenchRunner] Konsolide JSON ozeti kaydedildi: {summary_json}")

    with open(summary_csv, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "preset", "is_stress", "use_grid", "opt_shadow", "enable_taa", "normal_mode", "width", "height", "frames",
            "gpu_avg_ms", "gpu_min_ms", "gpu_max_ms", "gpu_p50_ms", "gpu_p95_ms",
            "cpu_avg_ms", "cpu_p50_ms", "cpu_p95_ms"
        ])
        for r in results:
            m = r["data"]["metrics"]
            writer.writerow([
                r["preset"], r["is_stress"], r["use_grid"], r["opt_shadow"], r["enable_taa"], r["normal_mode"], r["width"], r["height"], r["frames"],
                m["gpu_total_ms"]["avg"], m["gpu_total_ms"]["min"], m["gpu_total_ms"]["max"],
                m["gpu_total_ms"]["p50"], m["gpu_total_ms"]["p95"],
                m["cpu_frame_ms"]["avg"], m["cpu_frame_ms"]["p50"], m["cpu_frame_ms"]["p95"]
            ])
    print(f"[BenchRunner] Konsolide CSV ozeti kaydedildi: {summary_csv}")

def main():
    parser = argparse.ArgumentParser(description="AstralEngine Automated Benchmark Runner")
    parser.add_argument("--bin", type=str, default=None, help="Motor calistirilabilir dosya yolu")
    parser.add_argument("--frames", type=int, default=100, help="Test basina kare sayisi (varsayilan: 100)")
    parser.add_argument("--presets", nargs="+", default=["720p"], choices=list(PRESETS.keys()),
                        help="Test edilecek cozunurluk presetleri (720p, 1080p, 1440p)")
    parser.add_argument("--normals", nargs="+", default=["tetra"], choices=list(NORMAL_MODES.keys()),
                        help="Test edilecek normal stencil modlari (central, tetra)")
    parser.add_argument("--grids", nargs="+", default=["grid"], choices=["nogrid", "grid"],
                        help="Test edilecek hizlandirma modlari (nogrid, grid)")
    parser.add_argument("--shadows", nargs="+", default=["opt"], choices=["unopt", "opt"],
                        help="Test edilecek golge modlari (unopt, opt)")
    parser.add_argument("--taas", nargs="+", default=["notaa", "taa"], choices=["notaa", "taa"],
                        help="Test edilecek TAA modlari (notaa, taa)")
    parser.add_argument("--stress", action="store_true", help="32 nesneli stres sahnesi modunda calistir")
    parser.add_argument("--out-dir", type=str, default="artifacts/bench", help="Cikti dizini")
    parser.add_argument("--no-plot", action="store_true", help="Gorsel HTML rapor olusturma adimini atla")

    args = parser.parse_args()

    bin_path = find_binary(args.bin)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"[BenchRunner] Sandbox binary: {bin_path}")
    print(f"[BenchRunner] Hedef presetler: {args.presets} | Normal: {args.normals} | Izgara: {args.grids} | Golge: {args.shadows} | TAA: {args.taas} | Stres: {args.stress}")

    results = []
    for preset in args.presets:
        for normal in args.normals:
            for grid_mod in args.grids:
                use_grid = (grid_mod == "grid")
                for shd_mod in args.shadows:
                    opt_shadow = (shd_mod == "opt")
                    for taa_mod in args.taas:
                        enable_taa = (taa_mod == "taa")
                        res = run_single_benchmark(
                            bin_path, preset, PRESETS[preset], normal,
                            use_grid=use_grid, opt_shadow=opt_shadow, enable_taa=enable_taa, is_stress=args.stress,
                            frames=args.frames, out_dir=out_dir
                        )
                        if res:
                            results.append(res)

    if not results:
        print("[BenchRunner HATA]: Hicbir benchmark basariyla tamamlanamadi.")
        sys.exit(1)

    print_comparison_table(results)
    save_matrix_summary(results, out_dir)

    if not args.no_plot:
        plot_script = Path(__file__).parent / "plot_benchmarks.py"
        if plot_script.exists():
            print("\n[BenchRunner] HTML Raporu olusturuluyor...")
            subprocess.run([sys.executable, str(plot_script), "--input", str(out_dir / "matrix_summary.json"), "--out", str(out_dir / "report.html")])

if __name__ == "__main__":
    main()
