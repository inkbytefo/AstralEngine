"""Turn VisualQualityGpuTests PPM captures into lossless PNGs and a frame-review page.

Usage: python tools/visual_quality_report.py artifacts/a4/captures
Requires Pillow. Original PPM captures remain the comparison source.
"""
from pathlib import Path
import json
import sys
from PIL import Image, ImageDraw

root = Path(sys.argv[1]).resolve()
sequences = {}
for folder in sorted(root.iterdir()):
    if not folder.is_dir():
        continue
    frames = []
    for source in sorted(folder.glob("*.ppm")):
        target = source.with_suffix(".png")
        with Image.open(source) as im:
            im.save(target)
            frames.append(im.copy())
    if not frames:
        continue
    sequences[folder.name] = [str(p.relative_to(root)).replace("\\", "/") for p in sorted(folder.glob("*.png"))]
    # GIF is for convenient preview; PNG/PPM retain original unquantized RGB values.
    frames[0].save(root / f"{folder.name}.gif", save_all=True, append_images=frames[1:], duration=33, loop=0)
    selected = [0, 8, 15, 16, 24, 31]
    sheet = Image.new("RGB", (384 * 3, 210 * 2), "#181b21")
    draw = ImageDraw.Draw(sheet)
    for index, frame in enumerate(selected):
        image = frames[frame].resize((frames[frame].width, 180), Image.Resampling.NEAREST)
        x, y = (index % 3) * 384, (index // 3) * 210
        sheet.paste(image, (x, y + 25))
        draw.text((x + 5, y + 5), f"{folder.name} / frame {frame}", fill="white")
    sheet.save(root / f"{folder.name}_contact.png")

page = """<!doctype html><meta charset="utf-8"><title>Astral A4 Frame Review</title>
<style>body{background:#151820;color:#e4e8ed;font:16px system-ui;margin:30px}img{width:640px;max-width:90vw;image-rendering:auto}button,select,input{margin:10px;padding:8px}figure{display:inline-block;margin:8px}small{display:block;color:#adb6c6}</style>
<h1>Astral A4 · Frame Review</h1><p>Source: 60 Hz deterministic simulation. Playback: 30 fps (half speed). PNG frames preserve the captured RGB values.</p>
<select id="scene"></select><button id="play">Play / Pause</button><input id="frame" type="range" min="0" max="31" value="0"><span id="label"></span><br>
<figure><figcaption>Forward — fixed light, shadow, AO</figcaption><img id="forward"></figure>
<figure><figcaption>Deferred — PBR + imported IBL, no shadow</figcaption><img id="deferred"></figure>
<small>These rendering paths have different lighting features. Their frame times are not equivalent-quality comparisons. Resize frames retain their native dimensions.</small>
<script>const sequences=DATA;let active=false;const names=['pan','thin','metal','fast','disocclusion','resize'];
scene.innerHTML=names.map(n=>`<option>${n}</option>`).join('');
function show(){for(const path of ['forward','deferred'])document.getElementById(path).src=sequences[path+'_'+scene.value][+frame.value];label.textContent='Frame '+frame.value}
scene.onchange=show;frame.oninput=show;play.onclick=()=>active=!active;setInterval(()=>{if(active){frame.value=(+frame.value+1)%32;show()}},1000/30);show();</script>"""
(root / "index.html").write_text(page.replace("DATA", json.dumps(sequences)), encoding="utf-8")
print(f"Wrote {len(sequences)} sequences: {root / 'index.html'}")
