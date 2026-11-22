#!/usr/bin/env python3
"""Generates the M9 test map: a 4 x 4 km procedural world as a .world file plus one .ent file per
entity (ADR-0021). Deterministic (fixed seed). Output goes to a build folder, never into the repo.

    make_test_world.py <out-dir> [--size 4000] [--spacing 55] [--seed 42]

Writes <out-dir>/maps/test.world and <out-dir>/maps/test.entities/*.ent.
"""
import argparse
import json
import random
import shutil
from pathlib import Path


def entity(eid, name, components, layers=None, grid=None, always=False):
    doc = {"format": "astral.entity", "version": 1,
           "entity": {"id": eid, "components": dict({"astral::Name": {"value": name}}, **components)}}
    if grid:
        doc["grid"] = grid
    if layers:
        doc["layers"] = layers
    if always:
        doc["always_loaded"] = True
    return doc


def prim(shape, color, emissive=None, roughness=0.8):
    material = {"base_color": color + [1.0], "roughness": roughness}
    if emissive:
        material["emissive"] = emissive
    return {"astral::PrimitiveMesh": {"shape": shape, "material": material}}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--size", type=float, default=4000.0)
    ap.add_argument("--spacing", type=float, default=55.0)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    rng = random.Random(args.seed)
    maps = Path(args.out) / "maps"
    ents = maps / "test.entities"
    if ents.exists():
        shutil.rmtree(ents)
    ents.mkdir(parents=True)

    world = {"format": "astral.map", "version": 1,
             "settings": {"name": "M9 test world", "size_m": args.size},
             "grids": [{"name": "main", "cell_size": 128, "load_range": 384},
                       {"name": "landmarks", "cell_size": 512, "load_range": 2000}],
             "layers": [{"name": "night", "active": False}]}
    (maps / "test.world").write_text(json.dumps(world, indent=1) + "\n")

    entities = []
    half = args.size / 2.0
    entities.append(entity(1, "ground", dict(
        {"astral::Transform": {"position": [half, 0, half], "scale": [args.size, 1, args.size]}},
        **prim("Plane", [0.32, 0.36, 0.28])), always=True))

    eid = 100
    steps = int(args.size // args.spacing)
    for i in range(steps):
        for j in range(steps):
            x = (i + 0.5) * args.spacing + rng.uniform(-15, 15)
            z = (j + 0.5) * args.spacing + rng.uniform(-15, 15)
            roll = rng.random()
            eid += 1
            if roll < 0.45:  # tree
                h = rng.uniform(5, 11)
                entities.append(entity(eid, f"tree {eid}", dict(
                    {"astral::Transform": {"position": [x, h * 0.5, z], "scale": [h * 0.45, h, h * 0.45]}},
                    **prim("Sphere", [0.16 + rng.uniform(0, 0.08), 0.38 + rng.uniform(0, 0.12), 0.14]))))
            elif roll < 0.8:  # house
                w, d, h = rng.uniform(6, 14), rng.uniform(6, 14), rng.uniform(4, 12)
                yaw = rng.uniform(-1, 1)
                g = rng.uniform(0.45, 0.75)
                entities.append(entity(eid, f"house {eid}", dict(
                    {"astral::Transform": {"position": [x, h * 0.5, z], "rotation": [0, yaw * 0.38, 0, 0.92],
                                           "scale": [w, h, d]}},
                    **prim("Cube", [g, g * 0.92, g * 0.85]))))
            elif roll < 0.9:  # street lamp: only at night
                entities.append(entity(eid, f"lamp {eid}", dict(
                    {"astral::Transform": {"position": [x, 4.0, z], "scale": [0.6, 0.6, 0.6]},
                     "astral::PointLight": {"color": [1.0, 0.75, 0.45], "intensity": 60, "radius": 25}},
                    **prim("Sphere", [1, 1, 1], emissive=[8, 6, 3.5])), layers=["night"]))
            elif roll < 0.905:  # tower: a landmark seen from far away
                h = rng.uniform(60, 120)
                entities.append(entity(eid, f"tower {eid}", dict(
                    {"astral::Transform": {"position": [x, h * 0.5, z], "scale": [10, h, 10]}},
                    **prim("Cube", [0.75, 0.3, 0.25])), grid="landmarks"))
            else:
                eid -= 1  # empty plot

    for e in entities:
        (ents / f"{e['entity']['id']}.ent").write_text(json.dumps(e, separators=(",", ":")) + "\n")
    print(f"wrote {len(entities)} entities to {ents}")


if __name__ == "__main__":
    main()
