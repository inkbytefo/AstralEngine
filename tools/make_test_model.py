#!/usr/bin/env python3
"""Generates a procedural, textured test model (crate.glb) for the model viewer and manual tests.

    python3 tools/make_test_model.py out/crate.glb

Wooden crate: plank base colour, groove normal map, metallic-roughness map with metal corner bands.
Everything is generated here (no third-party art), so the output can be shared freely.
Needs numpy and Pillow.
"""
import io
import json
import struct
import sys

import numpy as np
from PIL import Image

SIZE = 512


def noise(shape, scale, seed):
    """Smooth value noise: random grid upsampled bicubically."""
    rng = np.random.default_rng(seed)
    small = rng.random((max(2, shape[0] // scale), max(2, shape[1] // scale))).astype(np.float32)
    return np.asarray(Image.fromarray((small * 255).astype(np.uint8)).resize(shape[::-1], Image.BICUBIC), np.float32) / 255.0


def crate_textures():
    y, x = np.mgrid[0:SIZE, 0:SIZE].astype(np.float32) / SIZE
    planks = 5
    plank = np.floor(y * planks)
    v = (y * planks) % 1.0
    border = 0.07  # metal frame width (uv)
    frame = (x < border) | (x > 1 - border) | (y < border) | (y > 1 - border)
    grain = noise((SIZE, SIZE * 8), 6, 1)[:, ::8] * 0.6 + noise((SIZE, SIZE), 3, 2) * 0.4
    streaks = 0.5 + 0.5 * np.sin((x * 40 + grain * 6 + plank * 1.7) * np.pi)
    tone = 0.75 + 0.25 * ((plank * 0.37) % 1.0)
    wood = np.stack([0.55, 0.36, 0.20])[None, None, :] * (0.65 + 0.35 * streaks[..., None]) * tone[..., None]
    metal = np.stack([0.32, 0.33, 0.35])[None, None, :] * (0.8 + 0.2 * noise((SIZE, SIZE), 16, 3))[..., None]
    groove = np.minimum(v, 1 - v) < 0.02
    wood = np.where(groove[..., None], wood * 0.35, wood)
    base = np.where(frame[..., None], metal, wood)

    # Height: grooves sink, frame stands out with rivet bumps.
    height = np.where(groove, -1.0, 0.0) + np.where(frame, 0.6, 0.0)
    rivets = ((np.abs(((x * 8) % 1.0) - 0.5) < 0.08) & (np.abs(((y * 8) % 1.0) - 0.5) < 0.08) & frame)
    height = height + rivets * 0.6 + grain * 0.15
    gy, gx = np.gradient(height)
    strength = 6.0
    n = np.stack([-gx * strength, gy * strength, np.ones_like(gx)], axis=-1)  # +y up in tangent space (glTF)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)

    rough = np.where(frame, 0.35 + 0.2 * noise((SIZE, SIZE), 8, 4), 0.75 + 0.2 * grain)
    metallic = np.where(frame, 1.0, 0.0)
    mr = np.stack([np.zeros_like(rough), rough, metallic], axis=-1)  # glTF: G roughness, B metallic

    def png(arr, srgb):
        a = np.clip(arr, 0, 1)
        if srgb:
            a = np.where(a <= 0.0031308, a * 12.92, 1.055 * np.power(a, 1 / 2.4) - 0.055)
        img = Image.fromarray((a * 255 + 0.5).astype(np.uint8), "RGB")
        buf = io.BytesIO()
        img.save(buf, "PNG")
        return buf.getvalue()

    return png(base, True), png(n * 0.5 + 0.5, False), png(mr, False)


def cube():
    """24 vertices (per-face normals and full 0..1 uvs), 36 indices."""
    faces = [((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (-1, 0, 0), (0, 1, 0)),
             ((1, 0, 0), (0, 0, -1), (0, 1, 0)), ((-1, 0, 0), (0, 0, 1), (0, 1, 0)),
             ((0, 1, 0), (1, 0, 0), (0, 0, -1)), ((0, -1, 0), (1, 0, 0), (0, 0, 1))]
    pos, nrm, uv, idx = [], [], [], []
    for n, u, v in faces:
        base = len(pos)
        for cu, cv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            pos.append([0.5 * (n[i] + cu * u[i] + cv * v[i]) for i in range(3)])
            nrm.append(list(n))
            uv.append([(cu + 1) * 0.5, (1 - cv) * 0.5])
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return pos, nrm, uv, idx


def build(path):
    color, normal, mr = crate_textures()
    pos, nrm, uv, idx = cube()
    blob = bytearray()
    views = []

    def add(data, target=None):
        while len(blob) % 4:
            blob.append(0)
        view = {"buffer": 0, "byteOffset": len(blob), "byteLength": len(data)}
        if target:
            view["target"] = target
        blob.extend(data)
        views.append(view)
        return len(views) - 1

    p = add(struct.pack(f"<{len(pos) * 3}f", *sum(pos, [])), 34962)
    n = add(struct.pack(f"<{len(nrm) * 3}f", *sum(nrm, [])), 34962)
    t = add(struct.pack(f"<{len(uv) * 2}f", *sum(uv, [])), 34962)
    i = add(struct.pack(f"<{len(idx)}H", *idx), 34963)
    images = [add(color), add(normal), add(mr)]
    gltf = {
        "asset": {"version": "2.0", "generator": "AstralEngine tools/make_test_model.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [
            {"name": "crate", "mesh": 0, "children": [1]},
            {"name": "small_crate", "mesh": 0, "translation": [0.0, 0.8, 0.0], "scale": [0.6, 0.6, 0.6],
             "rotation": [0.0, 0.2588190, 0.0, 0.9659258]},
        ],
        "meshes": [{"name": "crate", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
                                                     "indices": 3, "material": 0}]}],
        "materials": [{"name": "crate_wood", "normalTexture": {"index": 1},
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                "metallicRoughnessTexture": {"index": 2}}}],
        "textures": [{"source": 0}, {"source": 1}, {"source": 2}],
        "images": [{"bufferView": v, "mimeType": "image/png"} for v in images],
        "accessors": [
            {"bufferView": p, "componentType": 5126, "count": len(pos), "type": "VEC3",
             "min": [-0.5, -0.5, -0.5], "max": [0.5, 0.5, 0.5]},
            {"bufferView": n, "componentType": 5126, "count": len(nrm), "type": "VEC3"},
            {"bufferView": t, "componentType": 5126, "count": len(uv), "type": "VEC2"},
            {"bufferView": i, "componentType": 5123, "count": len(idx), "type": "SCALAR"},
        ],
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    js = json.dumps(gltf, separators=(",", ":")).encode()
    js += b" " * ((4 - len(js) % 4) % 4)
    while len(blob) % 4:
        blob.append(0)
    total = 12 + 8 + len(js) + 8 + len(blob)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", 0x46546C67, 2, total))
        f.write(struct.pack("<II", len(js), 0x4E4F534A) + js)
        f.write(struct.pack("<II", len(blob), 0x004E4942) + bytes(blob))


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else "crate.glb")
