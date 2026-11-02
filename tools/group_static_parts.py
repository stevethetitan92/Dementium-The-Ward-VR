"""Create a reversible, identity-transform group for verified static mesh nodes.

Only siblings are accepted, avoiding hidden rebases, scale changes, or skin damage.
This creates a rest-pose grouping, not a calibrated reload animation.
"""
import argparse
import json
import struct
from pathlib import Path
from stage_static_weapon import select


def group(data, members, name):
    # Reuse the static GLB validator, retaining every original scene node.
    if len(data) < 20:
        raise ValueError('Truncated GLB')
    size = struct.unpack_from('<I', data, 12)[0]
    doc = json.loads(data[20:20+size])
    select(data, list(range(len(doc.get('nodes', [])))))
    if not members or len(set(members)) != len(members):
        raise ValueError('Members must be unique and nonempty')
    nodes = doc['nodes']
    parents = {c:i for i,n in enumerate(nodes) for c in n.get('children', [])}
    if any(not 0 <= i < len(nodes) or 'mesh' not in nodes[i] for i in members):
        raise ValueError('Every member must be an existing mesh node')
    parent = parents.get(members[0])
    if parent is None or any(parents.get(i) != parent for i in members):
        raise ValueError('Only mesh siblings with a common parent may be grouped')
    if any(n.get('name') == name for n in nodes):
        raise ValueError('Group name already exists')
    new_index = len(nodes)
    nodes.append({'name':name, 'children':list(members),
                  'extras':{'purpose':'rest-pose part binding; motion uncalibrated'}})
    nodes[parent]['children'] = [i for i in nodes[parent]['children'] if i not in members] + [new_index]
    raw = json.dumps(doc,separators=(',', ':')).encode()
    raw += b' ' * (-len(raw) % 4)
    # Original remaining chunks remain byte-for-byte intact.
    tail = data[20+size:]
    payload = struct.pack('<2I',len(raw),0x4E4F534A)+raw+tail
    return struct.pack('<3I',0x46546C67,2,len(payload)+12)+payload, new_index


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--nodes',type=int,nargs='+',required=True)
    p.add_argument('--name',required=True)
    args=p.parse_args()
    data,index=group(args.source.read_bytes(),args.nodes,args.name)
    with args.output.open('xb') as f:f.write(data)
    print(json.dumps({'group_node':index,'members':args.nodes,'motion_calibrated':False}))
