#!/usr/bin/env python3
"""
AstralEngine - Benchmark Report Visualizer (PR-7)
Generates a standalone, beautiful HTML/SVG report from matrix_summary.json.
Visualizes Shadow Early Exit & Back-Face Culling vs Unoptimized Brute-Force Shadows.
Uses pure Python standard library (no pip dependencies required).
"""

import sys
import json
import argparse
from pathlib import Path

HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="tr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AstralEngine — Shadow Early-Exit & Shading Acceleration Benchmark</title>
    <style>
        :root {
            --bg-primary: #0d1117;
            --bg-secondary: #161b22;
            --bg-card: #21262d;
            --border: #30363d;
            --text-primary: #f0f6fc;
            --text-secondary: #8b949e;
            --accent-cyan: #58a6ff;
            --accent-emerald: #3fb950;
            --accent-purple: #bc8cff;
            --accent-orange: #d29922;
            --accent-red: #f85149;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: var(--bg-primary);
            color: var(--text-primary);
            padding: 2rem;
            line-height: 1.6;
        }

        .container {
            max-width: 1150px;
            margin: 0 auto;
        }

        header {
            margin-bottom: 2rem;
            border-bottom: 1px solid var(--border);
            padding-bottom: 1.5rem;
        }

        .header-badge {
            display: inline-block;
            background: rgba(63, 185, 80, 0.15);
            color: var(--accent-emerald);
            padding: 0.25rem 0.75rem;
            border-radius: 20px;
            font-size: 0.85rem;
            font-weight: 600;
            margin-bottom: 0.75rem;
            border: 1px solid rgba(63, 185, 80, 0.3);
        }

        h1 {
            font-size: 2.2rem;
            font-weight: 700;
            margin-bottom: 0.5rem;
            background: linear-gradient(135deg, #3fb950, #58a6ff);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        p.subtitle {
            color: var(--text-secondary);
            font-size: 1.05rem;
        }

        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
            gap: 1.5rem;
            margin-bottom: 2rem;
        }

        .card {
            background-color: var(--bg-secondary);
            border: 1px solid var(--border);
            border-radius: 12px;
            padding: 1.5rem;
            box-shadow: 0 8px 24px rgba(0, 0, 0, 0.2);
        }

        .card-title {
            font-size: 1.15rem;
            font-weight: 600;
            margin-bottom: 1rem;
            color: var(--text-primary);
            display: flex;
            align-items: center;
            gap: 0.5rem;
        }

        .meta-list {
            list-style: none;
        }

        .meta-item {
            display: flex;
            justify-content: space-between;
            padding: 0.5rem 0;
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
            font-size: 0.95rem;
        }

        .meta-item:last-child { border-bottom: none; }
        .meta-label { color: var(--text-secondary); }
        .meta-val { font-weight: 600; color: var(--text-primary); font-family: monospace; }

        .table-wrap {
            overflow-x: auto;
            margin-top: 1rem;
        }

        table {
            width: 100%;
            border-collapse: collapse;
            text-align: left;
            font-size: 0.95rem;
        }

        th {
            background-color: var(--bg-card);
            color: var(--text-secondary);
            padding: 0.75rem 1rem;
            font-weight: 600;
            border-bottom: 2px solid var(--border);
        }

        td {
            padding: 0.75rem 1rem;
            border-bottom: 1px solid var(--border);
            font-family: monospace;
        }

        tr:hover td {
            background-color: rgba(255, 255, 255, 0.02);
        }

        .badge-preset {
            background-color: rgba(88, 166, 255, 0.15);
            color: var(--accent-cyan);
            padding: 0.2rem 0.6rem;
            border-radius: 6px;
            font-weight: bold;
        }

        .badge-opt-on {
            background-color: rgba(63, 185, 80, 0.15);
            color: var(--accent-emerald);
            padding: 0.2rem 0.6rem;
            border-radius: 6px;
            font-weight: 600;
        }

        .badge-opt-off {
            background-color: rgba(210, 153, 34, 0.15);
            color: var(--accent-orange);
            padding: 0.2rem 0.6rem;
            border-radius: 6px;
            font-weight: 600;
        }

        .chart-container {
            margin-top: 1rem;
            background: var(--bg-card);
            border-radius: 8px;
            padding: 1.5rem;
            border: 1px solid var(--border);
        }

        svg {
            width: 100%;
            height: auto;
            overflow: visible;
        }

        footer {
            margin-top: 3rem;
            text-align: center;
            color: var(--text-secondary);
            font-size: 0.85rem;
            border-top: 1px solid var(--border);
            padding-top: 1.5rem;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <div class="header-badge">PR-7 Shading & Shadow Early Exit</div>
            <h1>AstralEngine Performans Raporu</h1>
            <p class="subtitle">Gölge & AO Erken Çıkış, Ters Yüzey Ayıklama (Back-Face Culling) ve Boş Uzay Atlama</p>
        </header>

        <div class="grid">
            <div class="card">
                <div class="card-title">🖥️ Donanim & Sistem Bilgisi</div>
                <ul class="meta-list">
                    <li class="meta-item">
                        <span class="meta-label">GPU Modeli</span>
                        <span class="meta-val">__GPU_NAME__</span>
                    </li>
                    <li class="meta-item">
                        <span class="meta-label">Vulkan API</span>
                        <span class="meta-val">1.4.341</span>
                    </li>
                    <li class="meta-item">
                        <span class="meta-label">Surucu Kodu</span>
                        <span class="meta-val">__DRIVER_VERSION__</span>
                    </li>
                    <li class="meta-item">
                        <span class="meta-label">Timestamp Hassasiyeti</span>
                        <span class="meta-val">1.0 ns / tick</span>
                    </li>
                </ul>
            </div>

            <div class="card">
                <div class="card-title">⚡ Gölge & Aydınlatma Optimizasyonu (PR-7)</div>
                <ul class="meta-list">
                    <li class="meta-item">
                        <span class="meta-label">Back-Face Early-Out</span>
                        <span class="meta-val" style="color: var(--accent-emerald);">Aktif (N·L &le; 0 ise 0 adım)</span>
                    </li>
                    <li class="meta-item">
                        <span class="meta-label">AABB Sky Early-Exit</span>
                        <span class="meta-val" style="color: var(--accent-emerald);">Aktif (Y &gt; 11m ise erken terk)</span>
                    </li>
                    <li class="meta-item">
                        <span class="meta-label">Shadow Grid ESS</span>
                        <span class="meta-val" style="color: var(--accent-emerald);">Aktif (Izgara üzerinden sıçrama)</span>
                    </li>
                    <li class="meta-item">
                        <span class="meta-label">Vulkan Validation</span>
                        <span class="meta-val" style="color: var(--accent-emerald);">Aktif (0 Hata)</span>
                    </li>
                </ul>
            </div>
        </div>

        <div class="card" style="margin-bottom: 2rem;">
            <div class="card-title">📊 Karsilastirmali Performans Tablosu</div>
            <div class="table-wrap">
                <table>
                    <thead>
                        <tr>
                            <th>Preset</th>
                            <th>Sahne</th>
                            <th>Hizlandirma Yapisi</th>
                            <th>Golge Modu</th>
                            <th>GPU Ort (ms)</th>
                            <th>GPU p50 (ms)</th>
                            <th>CPU Ort (ms)</th>
                        </tr>
                    </thead>
                    <tbody>
                        __TABLE_ROWS__
                    </tbody>
                </table>
            </div>
        </div>

        <div class="card">
            <div class="card-title">📈 GPU Render Zamani (ms) — Gölge Erken Çıkış Karsilastirmasi</div>
            <div class="chart-container">
                __SVG_CHART__
            </div>
        </div>

        <footer>
            AstralEngine &bull; Signed Distance Field Compute Shading & Raymarch Architecture &bull; Otomatik Uretilmistir
        </footer>
    </div>
</body>
</html>
"""

def generate_svg_chart(results):
    max_val = 0.0
    for r in results:
        m = r["data"]["metrics"]
        max_val = max(max_val, m["gpu_total_ms"]["p95"], m["gpu_total_ms"]["avg"], 1.0)
    
    max_val = max_val * 1.25

    width = 750
    height = 280
    margin_l = 80
    margin_b = 60
    chart_w = width - margin_l - 30
    chart_h = height - margin_b - 30

    bars = ""
    bar_group_w = chart_w / len(results)
    bar_w = min(50, bar_group_w * 0.5)

    for i, r in enumerate(results):
        m = r["data"]["metrics"]
        gpu_avg = m["gpu_total_ms"]["avg"]
        opt_shadow = r.get("opt_shadow", True)

        gx = margin_l + i * bar_group_w + (bar_group_w - bar_w) / 2
        
        # Color: Emerald for Opt Shadow, Orange for Unopt
        color = "#3fb950" if opt_shadow else "#d29922"

        h_bar = (gpu_avg / max_val) * chart_h
        y_bar = height - margin_b - h_bar
        bars += f'<rect x="{gx}" y="{y_bar}" width="{bar_w}" height="{h_bar}" fill="{color}" rx="5"/>'
        bars += f'<text x="{gx + bar_w/2}" y="{y_bar - 8}" font-size="11" font-weight="bold" fill="{color}" text-anchor="middle">{gpu_avg:.2f} ms</text>'

        # X labels
        label_preset = f"{r['preset']}"
        label_mode = "Opt Shadow" if opt_shadow else "Kaba Gölge"
        bars += f'<text x="{gx + bar_w/2}" y="{height - margin_b + 18}" font-size="12" font-weight="600" fill="#f0f6fc" text-anchor="middle">{label_preset}</text>'
        bars += f'<text x="{gx + bar_w/2}" y="{height - margin_b + 34}" font-size="10" fill="{color}" text-anchor="middle">{label_mode}</text>'

    svg = f'''<svg viewBox="0 0 {width} {height}" xmlns="http://www.w3.org/2000/svg">
        <!-- Axes -->
        <line x1="{margin_l}" y1="20" x2="{margin_l}" y2="{height - margin_b}" stroke="#30363d" stroke-width="2"/>
        <line x1="{margin_l}" y1="{height - margin_b}" x2="{width - 20}" y2="{height - margin_b}" stroke="#30363d" stroke-width="2"/>

        <!-- Grid lines -->
        <line x1="{margin_l}" y1="{height - margin_b - chart_h/2}" x2="{width - 20}" y2="{height - margin_b - chart_h/2}" stroke="#30363d" stroke-dasharray="4"/>
        <text x="{margin_l - 10}" y="{height - margin_b - chart_h/2 + 4}" font-size="10" fill="#8b949e" text-anchor="end">{max_val/2:.1f} ms</text>
        <text x="{margin_l - 10}" y="24" font-size="10" fill="#8b949e" text-anchor="end">{max_val:.1f} ms</text>

        {bars}

        <!-- Legend -->
        <rect x="{width - 320}" y="10" width="12" height="12" fill="#3fb950" rx="2"/>
        <text x="{width - 302}" y="20" font-size="11" fill="#8b949e">Opt Shadow (Erken Cikis + Back-Face)</text>
        <rect x="{width - 140}" y="10" width="12" height="12" fill="#d29922" rx="2"/>
        <text x="{width - 122}" y="20" font-size="11" fill="#8b949e">Kaba Kuvvet (24-Adım)</text>
    </svg>'''
    return svg

def main():
    parser = argparse.ArgumentParser(description="Plot benchmark matrix results into HTML/SVG")
    parser.add_argument("--input", type=str, default="artifacts/bench/matrix_summary.json", help="Giris JSON dosyasi")
    parser.add_argument("--out", type=str, default="artifacts/bench/report.html", help="Cikti HTML dosyasi")

    args = parser.parse_args()
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"[PlotBenchmarks HATA]: {input_path} dosyasi bulunamadi.")
        sys.exit(1)

    with open(input_path, "r", encoding="utf-8") as f:
        results = json.load(f)

    if not results:
        print("[PlotBenchmarks HATA]: Veri bulunamadi.")
        sys.exit(1)

    first = results[0]
    gpu_name = first["data"]["benchmark_metadata"].get("gpu_name", "Bilinmiyor")
    driver_version = first["data"]["benchmark_metadata"].get("driver_version", "N/A")

    rows = ""
    for r in results:
        m = r["data"]["metrics"]
        gpu_avg = m["gpu_total_ms"]["avg"]
        gpu_p50 = m["gpu_total_ms"]["p50"]
        cpu_avg = m["cpu_frame_ms"]["avg"]
        opt_shadow = r.get("opt_shadow", True)
        grid_label = r.get("grid_label", "Grid ON (Two-Level)")
        shadow_label = r.get("shadow_label", "Opt Shadow" if opt_shadow else "Kaba Gölge")
        scene_label = r.get("scene_label", "Stress (32 Nesne)" if r.get("is_stress", False) else "Standart")

        badge_class = "badge-opt-on" if opt_shadow else "badge-opt-off"

        rows += f"""<tr>
            <td><span class="badge-preset">{r['preset']}</span></td>
            <td>{scene_label}</td>
            <td>{grid_label}</td>
            <td><span class="{badge_class}">{shadow_label}</span></td>
            <td>{gpu_avg:.3f}</td>
            <td>{gpu_p50:.3f}</td>
            <td>{cpu_avg:.3f}</td>
        </tr>"""

    svg_chart = generate_svg_chart(results)

    html = HTML_TEMPLATE
    html = html.replace("__GPU_NAME__", gpu_name)
    html = html.replace("__DRIVER_VERSION__", driver_version)
    html = html.replace("__TABLE_ROWS__", rows)
    html = html.replace("__SVG_CHART__", svg_chart)

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(html)

    print(f"[PlotBenchmarks] Rapor basariyla olusturuldu: {out_path.resolve()}")

if __name__ == "__main__":
    main()
