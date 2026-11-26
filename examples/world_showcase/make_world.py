#!/usr/bin/env python3
"""Writes the 4 x 4 km streamed open world (assets/maps/world.world) using level instances.

Run by hand only when the world layout changes (the output is checked in so loose assets and
astral-cook / .apak packages read the exact same source files):

    python make_world.py [assets-dir]    (default: assets next to this script)

Produces:
  - maps/world.world          (main 128 m / 384 m grid, landmarks 512 m / 2000 m grid, night layer)
  - maps/sector_day.scene     (1 km x 1 km reusable tile of trees and houses, centred on origin)
  - maps/sector_lamps.scene   (1 km x 1 km reusable tile of night street lamps)
  - maps/world.entities/*.ent (always-loaded ground, 16 day sectors, 16 night lamp sectors, 16 landmark towers)
"""
import json
import math
import random
import shutil
import sys
from pathlib import Path

WORLD_SIZE = 4000.0
SECTOR_SIZE = 1000.0
SECTORS = 4


def fnv_entity_id(instance_id, local_path):
    """Matches astral::level_instance_entity_id so we can assert zero ID collisions."""
    h = 14695981039346656037
    mask = 0xFFFFFFFFFFFFFFFF
    for i in range(8):
        h ^= (instance_id >> (i * 8)) & 0xFF
        h = (h * 1099511628211) & mask
    for b in local_path.encode("utf-8"):
        h ^= b
        h = (h * 1099511628211) & mask
    eid = h & 0x7FFFFFFF
    return 1 if eid == 0 else eid


def prim(shape, color, emissive=None, roughness=0.8):
    material = {"base_color": color + [1.0], "roughness": roughness}
    if emissive:
        material["emissive"] = emissive
    return {"astral::PrimitiveMesh": {"shape": shape, "material": material}}


def yaw_quat(angle_rad):
    half = angle_rad * 0.5
    return [0.0, round(math.sin(half), 6), 0.0, round(math.cos(half), 6)]


def build_day_sector(rng):
    """Trees and houses over [-500, 500)^2 so any 90-degree rotation stays inside the sector."""
    entities = []
    eid = 0
    step = 62.5  # 16 x 16 = 256 plots per 1 km sector
    count = int(SECTOR_SIZE / step)
    half = SECTOR_SIZE * 0.5
    for i in range(count):
        for j in range(count):
            x = -half + (i + 0.5) * step + rng.uniform(-16.0, 16.0)
            z = -half + (j + 0.5) * step + rng.uniform(-16.0, 16.0)
            roll = rng.random()
            if roll < 0.48:
                eid += 1
                h = rng.uniform(5.5, 11.5)
                r = h * 0.45
                color = [round(0.16 + rng.uniform(0.0, 0.08), 3),
                         round(0.38 + rng.uniform(0.0, 0.12), 3),
                         round(0.14 + rng.uniform(0.0, 0.04), 3)]
                entities.append({
                    "id": eid,
                    "components": {
                        "astral::Name": {"value": f"tree_{eid}"},
                        "astral::Transform": {
                            "position": [round(x, 2), round(h * 0.5, 2), round(z, 2)],
                            "scale": [round(r, 2), round(h, 2), round(r, 2)],
                        },
                        **prim("Sphere", color),
                    },
                })
            elif roll < 0.82:
                eid += 1
                w = rng.uniform(6.0, 14.0)
                d = rng.uniform(6.0, 14.0)
                h = rng.uniform(4.5, 13.0)
                g = rng.uniform(0.45, 0.75)
                color = [round(g, 3), round(g * 0.92, 3), round(g * 0.85, 3)]
                entities.append({
                    "id": eid,
                    "components": {
                        "astral::Name": {"value": f"house_{eid}"},
                        "astral::Transform": {
                            "position": [round(x, 2), round(h * 0.5, 2), round(z, 2)],
                            "rotation": yaw_quat(rng.uniform(-0.8, 0.8)),
                            "scale": [round(w, 2), round(h, 2), round(d, 2)],
                        },
                        **prim("Cube", color),
                    },
                })
    return {"format": "astral.scene", "version": 1, "entities": entities}


def build_lamp_sector(rng):
    """Night-only street lamps over [-500, 500)^2."""
    entities = []
    eid = 0
    step = 125.0  # 8 x 8 = 64 lamp plots per 1 km sector
    count = int(SECTOR_SIZE / step)
    half = SECTOR_SIZE * 0.5
    for i in range(count):
        for j in range(count):
            if rng.random() > 0.65:
                continue
            eid += 1
            x = -half + (i + 0.5) * step + rng.uniform(-24.0, 24.0)
            z = -half + (j + 0.5) * step + rng.uniform(-24.0, 24.0)
            entities.append({
                "id": eid,
                "components": {
                    "astral::Name": {"value": f"lamp_{eid}"},
                    "astral::Transform": {
                        "position": [round(x, 2), 4.0, round(z, 2)],
                        "scale": [0.6, 0.6, 0.6],
                    },
                    "astral::PointLight": {
                        "color": [1.0, 0.75, 0.45],
                        "intensity": 60.0,
                        "radius": 25.0,
                    },
                    **prim("Sphere", [1.0, 1.0, 1.0], emissive=[8.0, 6.0, 3.5]),
                },
            })
    return {"format": "astral.scene", "version": 1, "entities": entities}


def entity_doc(eid, body, grid=None, layers=None, always=False):
    doc = {"format": "astral.entity", "version": 1, "entity": dict({"id": eid}, **body)}
    if grid:
        doc["grid"] = grid
    if layers:
        doc["layers"] = layers
    if always:
        doc["always_loaded"] = True
    return doc


def main():
    assets = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent / "assets"
    maps = assets / "maps"
    ents = maps / "world.entities"
    if ents.exists():
        shutil.rmtree(ents)
    ents.mkdir(parents=True)

    rng = random.Random(42)
    day_scene = build_day_sector(rng)
    lamp_scene = build_lamp_sector(rng)
    (maps / "sector_day.scene").write_text(json.dumps(day_scene, separators=(",", ":")) + "\n", encoding="utf-8")
    (maps / "sector_lamps.scene").write_text(json.dumps(lamp_scene, separators=(",", ":")) + "\n", encoding="utf-8")

    world = {
        "format": "astral.map",
        "version": 1,
        "settings": {"name": "World Showcase", "size_m": WORLD_SIZE},
        "grids": [
            {"name": "main", "cell_size": 128, "load_range": 384},
            {"name": "landmarks", "cell_size": 512, "load_range": 2000},
        ],
        "layers": [
            {"name": "night", "active": False},
        ],
    }
    (maps / "world.world").write_text(json.dumps(world, indent=2) + "\n", encoding="utf-8")

    half_world = WORLD_SIZE * 0.5
    files = []
    used_ids = set()

    # 1: always-loaded 4 x 4 km ground plane.
    used_ids.add(1)
    files.append((1, entity_doc(
        1,
        {
            "components": {
                "astral::Name": {"value": "ground"},
                "astral::Transform": {"position": [half_world, 0.0, half_world], "scale": [WORLD_SIZE, 1.0, WORLD_SIZE]},
                **prim("Plane", [0.32, 0.36, 0.28]),
            }
        },
        always=True,
    )))

    # 10..25: 16 day sectors (4 x 4 km).
    # 30..45: 16 night lamp sectors (4 x 4 km, "night" layer).
    for iz in range(SECTORS):
        for ix in range(SECTORS):
            idx = iz * SECTORS + ix
            cx = (ix + 0.5) * SECTOR_SIZE
            cz = (iz + 0.5) * SECTOR_SIZE
            rot = yaw_quat(((ix * 3 + iz) % 4) * (math.pi * 0.5))

            day_id = 10 + idx
            lamp_id = 30 + idx
            for e in day_scene["entities"]:
                hid = fnv_entity_id(day_id, str(e["id"]))
                assert hid not in used_ids, f"ID collision {hid}"
                used_ids.add(hid)
            for e in lamp_scene["entities"]:
                hid = fnv_entity_id(lamp_id, str(e["id"]))
                assert hid not in used_ids, f"ID collision {hid}"
                used_ids.add(hid)

            files.append((day_id, entity_doc(
                day_id,
                {"level_instance": {"scene": "maps/sector_day.scene", "position": [cx, 0.0, cz], "rotation": rot}},
                grid="main",
            )))
            files.append((lamp_id, entity_doc(
                lamp_id,
                {"level_instance": {"scene": "maps/sector_lamps.scene", "position": [cx, 0.0, cz], "rotation": rot}},
                grid="main",
                layers=["night"],
            )))

    # 50..65: 16 landmark towers visible across 2 km (landmarks grid).
    for iz in range(SECTORS):
        for ix in range(SECTORS):
            tid = 50 + iz * SECTORS + ix
            assert tid not in used_ids, f"ID collision {tid}"
            used_ids.add(tid)
            x = round((ix + 0.5) * SECTOR_SIZE + rng.uniform(-220.0, 220.0), 1)
            z = round((iz + 0.5) * SECTOR_SIZE + rng.uniform(-220.0, 220.0), 1)
            h = round(rng.uniform(70.0, 125.0), 1)
            files.append((tid, entity_doc(
                tid,
                {
                    "components": {
                        "astral::Name": {"value": f"tower_{tid}"},
                        "astral::Transform": {"position": [x, round(h * 0.5, 1), z], "scale": [12.0, h, 12.0]},
                        **prim("Cube", [0.75, 0.30, 0.25]),
                    }
                },
                grid="landmarks",
            )))

    for fid, doc in files:
        (ents / f"{fid}.ent").write_text(json.dumps(doc, separators=(",", ":")) + "\n", encoding="utf-8")

    total_expanded = 1 + SECTORS * SECTORS * (len(day_scene["entities"]) + len(lamp_scene["entities"]) + 1)
    print(f"wrote {len(files)} .ent files ({total_expanded} expanded entities, 0 ID collisions) to {ents}")


if __name__ == "__main__":
    main()
