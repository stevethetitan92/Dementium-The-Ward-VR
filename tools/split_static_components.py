"""Lossless connected-component staging for static single-primitive GLB meshes.

Component labels are geometric identities, not verified mechanical parts.
Retains the original vertex attributes, materials, textures and parent transform.
"""
import argparse
import copy
import json
import struct
import sys
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'work/psx-right-grips'))
from glb_data import GLB


def split(source, node_index):
    raw_source = Path(source).read_bytes()
    if len(raw_source) < 28 or struct.unpack_from('<3I', raw_source) != (0x46546C67, 2, len(raw_source)):
        raise ValueError('Invalid GLB header')
    g = GLB(source)
    doc = copy.deepcopy(g.g)
    if len(doc.get('buffers', [])) != 1 or 'uri' in doc['buffers'][0]:
        raise ValueError('Requires one embedded binary buffer')
    if any('sparse' in a for a in doc.get('accessors', [])):
        raise ValueError('Sparse accessors require a separate conversion')
    if not 0 <= node_index < len(doc.get('nodes', [])) or 'mesh' not in doc['nodes'][node_index]:
        raise ValueError('Select a valid mesh node')
    if doc.get('skins') or doc.get('animations'):
        raise ValueError('Animated or skinned input is not supported by this static operation')
    node = doc['nodes'][node_index]
    primitives = doc['meshes'][node['mesh']]['primitives']
    if len(primitives) != 1:
        raise ValueError('Explicit per-primitive selection required')
    primitive = primitives[0]
    if primitive.get('mode', 4) != 4 or primitive.get('targets'):
        raise ValueError('Only static triangle primitives are supported')
    vertices = g.acc(primitive['attributes']['POSITION'])
    faces = g.acc(primitive['indices']).reshape(-1, 3)
    if not faces.size or not np.isfinite(vertices).all() or faces.max() >= len(vertices):
        raise ValueError('Vertex index out of bounds')
    _, weld = np.unique(vertices, axis=0, return_inverse=True)
    parents = list(range(int(weld.max()) + 1))

    def root(a):
        while parents[a] != a:
            parents[a] = parents[parents[a]]
            a = parents[a]
        return a

    for triangle in weld[faces]:
        a = root(int(triangle[0]))
        for b in triangle[1:]:
            parents[root(int(b))] = a
    roots = np.array([root(int(t[0])) for t in weld[faces]])
    groups = sorted(set(roots), key=lambda r: (-int(sum(roots == r)), int(r)))
    binary = bytearray(g.buf)
    report = []
    for number, key in enumerate(groups):
        triangles = faces[roots == key].astype('<u4')
        binary.extend(b'\0' * (-len(binary) % 4))
        offset = len(binary)
        raw = triangles.tobytes()
        binary.extend(raw)
        view = len(doc['bufferViews'])
        doc['bufferViews'].append({'buffer': 0, 'byteOffset': offset, 'byteLength': len(raw), 'target': 34963})
        accessor = len(doc['accessors'])
        doc['accessors'].append({'bufferView': view, 'componentType': 5125, 'count': int(triangles.size), 'type': 'SCALAR'})
        part = copy.deepcopy(primitive)
        part['indices'] = accessor
        mesh = len(doc['meshes'])
        label = 'component_%02d' % number
        doc['meshes'].append({'name': label, 'primitives': [part]})
        child = len(doc['nodes'])
        doc['nodes'].append({'name': label, 'mesh': mesh})
        node.setdefault('children', []).append(child)
        subset = vertices[np.unique(triangles)]
        report.append({'component': number, 'node': child, 'triangles': len(triangles),
                       'min': subset.min(0).tolist(), 'max': subset.max(0).tolist(), 'mechanical_identity': 'unverified'})
    del node['mesh']
    doc['buffers'][0]['byteLength'] = len(binary)
    binary.extend(b'\0' * (-len(binary) % 4))
    text = json.dumps(doc, separators=(',', ':')).encode()
    text += b' ' * (-len(text) % 4)
    payload = struct.pack('<2I', len(text), 0x4E4F534A) + text + struct.pack('<2I', len(binary), 0x004E4942) + binary
    return struct.pack('<3I', 0x46546C67, 2, len(payload) + 12) + payload, report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--node', type=int, required=True)
    args = parser.parse_args()
    data, report = split(args.source, args.node)
    with args.output.open('xb') as file:
        file.write(data)
    with args.output.with_suffix('.components.json').open('x') as file:
        json.dump(report, file, indent=2)
    print('Staged %d independently addressable components; identities remain unverified.' % len(report))
