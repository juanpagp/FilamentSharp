#!/usr/bin/env python3
"""Generates the minimal GLB test model (assets/models/triangle.glb).

A single red triangle (embedded positions + indices, ubershader-compatible
PBR material) used by the native tests, the managed tests, and the sample
app. Self-contained: no external buffers or textures.

Usage: python3 scripts/gen-test-gltf.py
"""
from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "models" / "triangle.glb"

# Triangle in the XY plane (CCW when viewed from +Z), matching the extents
# asserted by the tests.
POSITIONS = [
    -1.0, -1.0, 0.0,   # v0
     3.0, -1.0, 0.0,   # v1
    -1.0,  3.0, 0.0,   # v2
]
INDICES = [0, 1, 2]

vertex_blob = struct.pack(f"<{len(POSITIONS)}f", *POSITIONS)
index_blob = struct.pack(f"<{len(INDICES)}H", *INDICES)

# Buffer views: 4-byte aligned offsets, padded to 4-byte multiples.
bin_data = vertex_blob
index_offset = len(bin_data)
bin_data += index_blob
bin_data += b"\x00" * (-len(bin_data) % 4)

gltf = {
    "asset": {"version": "2.0", "generator": "FilamentSharp gen-test-gltf.py"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0, "name": "Triangle"}],
    "meshes": [{
        "primitives": [{
            "attributes": {"POSITION": 0},
            "indices": 1,
            "material": 0,
            "mode": 4,
        }],
    }],
    "materials": [{
        "name": "RedUnlit",
        "pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 0.0, 0.0, 1.0],
            "metallicFactor": 0.0,
            "roughnessFactor": 1.0,
        },
        "doubleSided": True,
    }],
    "buffers": [{"byteLength": len(bin_data)}],
    "bufferViews": [
        {
            "buffer": 0, "byteOffset": 0, "byteLength": len(vertex_blob),
            "target": 34962,  # ARRAY_BUFFER
        },
        {
            "buffer": 0, "byteOffset": index_offset, "byteLength": len(index_blob),
            "target": 34963,  # ELEMENT_ARRAY_BUFFER
        },
    ],
    "accessors": [
        {
            "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
            "min": [-1.0, -1.0, 0.0], "max": [3.0, 3.0, 0.0],
        },
        {
            "bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR",
        },
    ],
}

json_data = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
json_data += b" " * (-len(json_data) % 4)

glb = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(json_data) + 8 + len(bin_data))
glb += struct.pack("<II", len(json_data), 0x4E4F534A) + json_data
glb += struct.pack("<II", len(bin_data), 0x004E4942) + bin_data

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_bytes(glb)
print(f"wrote {OUT} ({len(glb)} bytes)")
