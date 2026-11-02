"""Stage explicitly selected static GLB nodes without altering mesh/material data.

Preserves node indices, local transforms and all binary chunks. Animated or
skinned assets require a different path and are rejected rather than flattened.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def select(data, selected):
    if len(data) < 20 or struct.unpack_from('<3I', data) != (0x46546C67, 2, len(data)):
        raise ValueError('Invalid GLB header')
    chunks = []
    offset = 12
    while offset < len(data):
        if offset + 8 > len(data):
            raise ValueError('Truncated chunk header')
        size, kind = struct.unpack_from('<2I', data, offset)
        offset += 8
        if offset + size > len(data):
            raise ValueError('Truncated chunk')
        chunks.append((kind, data[offset:offset + size]))
        offset += size
    if not chunks or chunks[0][0] != 0x4E4F534A:
        raise ValueError('Missing JSON chunk')
    doc = json.loads(chunks[0][1])
    if doc.get('animations') or doc.get('skins'):
        raise ValueError('Rigged/animated models must retain their animation pipeline; static selection refused')
    for group in ('buffers', 'images'):
        if any('uri' in item and not item['uri'].startswith('data:') for item in doc.get(group, [])):
            raise ValueError('External resources must be packaged before staging')
    nodes = doc.get('nodes', [])
    parents = {}
    for i, node in enumerate(nodes):
        for child in node.get('children', []):
            if child in parents or not 0 <= child < len(nodes):
                raise ValueError('Invalid node hierarchy')
            parents[child] = i
    keep = set()

    def descendants(i, active):
        if i in active:
            raise ValueError('Cyclic node hierarchy')
        keep.add(i)
        for child in nodes[i].get('children', []):
            descendants(child, active | {i})

    if not selected:
        raise ValueError('Select at least one node')
    for i in selected:
        if not 0 <= i < len(nodes):
            raise ValueError('Selected node does not exist')
        descendants(i, set())
        ancestors = set()
        while i in parents:
            i = parents[i]
            if i in ancestors:
                raise ValueError('Cyclic node hierarchy')
            ancestors.add(i)
            keep.add(i)
    for i in keep:
        if 'children' in nodes[i]:
            nodes[i]['children'] = [c for c in nodes[i]['children'] if c in keep]
    # Existing mod exporters enumerate nodes rather than the active scene.
    # Disable excluded mesh instances as well, while retaining mesh buffers.
    for i, node in enumerate(nodes):
        if i not in keep:
            node.pop('mesh', None)
    doc['scenes'] = [{'name': 'Isolated replacement preview', 'nodes': sorted(i for i in keep if i not in parents)}]
    doc['scene'] = 0
    raw = json.dumps(doc, separators=(',', ':')).encode('utf-8')
    raw += b' ' * (-len(raw) % 4)
    chunks[0] = (0x4E4F534A, raw)
    payload = b''.join(struct.pack('<2I', len(raw), kind) + raw for kind, raw in chunks)
    return struct.pack('<3I', 0x46546C67, 2, 12 + len(payload)) + payload, sorted(keep)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--nodes', type=int, nargs='+', required=True)
    args = parser.parse_args()
    data = args.source.read_bytes()
    output, keep = select(data, args.nodes)
    with args.output.open('xb') as stream:
        stream.write(output)
    print(json.dumps({'source_sha256': hashlib.sha256(data).hexdigest(),
                      'output_sha256': hashlib.sha256(output).hexdigest(),
                      'reachable_nodes': keep, 'installed': False}))


if __name__ == '__main__':
    main()
