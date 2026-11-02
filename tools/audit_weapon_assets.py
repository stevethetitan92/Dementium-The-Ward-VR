"""Read-only installed-asset audit. Does not start, attach to or modify the game."""
import argparse
import hashlib
import json
import math
import mmap
from pathlib import Path
import re
import struct

MODELS = {
    '357': '.357 revolver', '9mm': '9mm handgun', 'ar15': 'AR15 rifle',
    'shotgun': 'Shotgun', 'sniper': 'Sniper rifle', 'buzzsaw': 'Buzz-saw',
    'nitstick': 'Nightstick', 'flashlig': 'Flashlight',
}
PREFIX = {'357': '357', '9mm': '9mm', 'ar15': 'ar15', 'sg': 'shotgun',
          'sr': 'sniper', 'bs': 'buzzsaw', 'ns': 'nitstick', 'fl': 'flashlig'}

def archive_index(data):
    result = {}
    for match in re.finditer(b'ustar', data):
        offset = match.start() - 257
        if offset < 0:
            continue
        header = data[offset:offset + 512]
        try:
            size = int(header[124:136].strip(b'\0 '), 8)
            checksum = int(header[148:156].strip(b'\0 '), 8)
            if sum(header[:148]) + 8 * 32 + sum(header[156:]) != checksum:
                continue
            name = header[:100].split(b'\0')[0].decode('ascii')
        except (ValueError, UnicodeError):
            continue
        assert offset + 512 + size <= len(data), name
        result[name] = (offset + 512, size)
    return result

def mesh_nodes(data):
    count, mask, offset = struct.unpack_from('<3I', data)
    assert 0 < count <= 31 and offset + count * 68 <= len(data)
    nodes = []
    for bone in range(count):
        position = offset + bone * 68
        matrix = struct.unpack_from('<12i', data, position)
        fields = struct.unpack_from('<5i', data, position + 48)
        links = []
        for pointer in fields[2:]:
            if pointer:
                assert pointer >= offset and (pointer-offset) % 68 == 0
                index = (pointer-offset)//68
                assert index < count
                links.append(index)
            else:
                links.append(None)
        nodes.append({'bone': bone, 'parent': links[0], 'sibling': links[1],
                      'child': links[2], 'bind_position': [v/4096 for v in matrix[9:12]]})
    return {'bone_count': count, 'animation_mask_raw': mask, 'nodes': nodes,
            'sha256': hashlib.sha256(data).hexdigest()}

def rotate(q, point):
    x,y,z,w = q
    vx,vy,vz = point
    a,b,c = 2*(y*vz-z*vy), 2*(z*vx-x*vz), 2*(x*vy-y*vx)
    return [vx+w*a+y*c-z*b, vy+w*b+z*a-x*c, vz+w*c+x*b-y*a]

def multiply(q, p):
    x,y,z,w = q
    X,Y,Z,W = p
    return [w*X+x*W+y*Z-z*Y, w*Y-x*Z+y*W+z*X,
            w*Z+x*Y-y*X+z*W, w*W-x*X-y*Y-z*Z]

def animation(data, rig):
    frames, header_word = struct.unpack_from('<2I', data)
    count = rig['bone_count']
    assert (8+(frames+1)*14*count+3)//4*4 == len(data)
    poses = []
    for sample in range(frames+1):
        row = []
        for bone in range(count):
            values = [v/4096 for v in struct.unpack_from('<7h', data, 8+(sample*count+bone)*14)]
            norm = math.sqrt(sum(v*v for v in values[:4]))
            assert .9 < norm < 1.1
            row.append(([v/norm for v in values[:4]], values[4:]))
        poses.append(row)
    result = []
    for bone in range(count):
        parent = rig['nodes'][bone]['parent']
        samples = []
        for frame in poses:
            q, t = frame[bone]
            if parent is not None:
                pq, pt = frame[parent]
                inverse = [-pq[0],-pq[1],-pq[2],pq[3]]
                q = multiply(inverse,q)
                t = rotate(inverse,[t[i]-pt[i] for i in range(3)])
            samples.append((q,t))
        first_q, first_t = samples[0]
        angle = max(2*math.degrees(math.acos(min(1,abs(sum(q[i]*first_q[i] for i in range(4)))))) for q,t in samples)
        distance = max(math.sqrt(sum((t[i]-first_t[i])**2 for i in range(3))) for q,t in samples)
        result.append({'bone': bone, 'translation_from_first_sample': round(distance,6),
                       'rotation_from_first_sample_degrees': round(angle,3),
                       'clear_relative_motion': distance > .002 or angle > 1})
    return {'frame_header': frames, 'stored_pose_samples': frames+1,
            'second_header_word': header_word, 'relative_motion': result,
            'sha256': hashlib.sha256(data).hexdigest()}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom', type=Path, default=Path(r'C:\Program Files (x86)\Steam\steamapps\common\Dementium - The Ward\rom.zar'))
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1]/'weapon-audit'/'asset-evidence.json')
    args = parser.parse_args()
    with args.rom.open('rb') as file, mmap.mmap(file.fileno(),0,access=mmap.ACCESS_READ) as data:
        index = archive_index(data)
        def asset(name):
            offset,size = index[name]
            return data[offset:offset+size]
        rigs = {name: mesh_nodes(asset('3d/'+name+'.msh')) for name in MODELS}
        clips = {}
        for name in index:
            if not name.startswith('3d/') or not name.endswith('.anm'):
                continue
            prefix = Path(name).stem.split('_')[0]
            if prefix in PREFIX:
                model = PREFIX[prefix]
                clips[name] = {'model': model, **animation(asset(name),rigs[model])}
        auxiliary = {name: mesh_nodes(asset(name)) for name in index
                     if name in ['3d/grenade.msh','3d/sg_blast.msh','3d/fl_glow.msh','3d/fl_cone.msh']}
        result = {'scope': 'Offline inventory weapon rigs, animation channels and ammo asset names; semantic part labels require visual confirmation.',
                  'rom_bytes': len(data), 'validated_archive_entries': len(index),
                  'models': rigs, 'animations': clips, 'auxiliary_models': auxiliary,
                  'ammo_texture_files': sorted(n for n in index if n.startswith('3d/tex/ammo_')),
                  'standalone_msh_files': sorted(n for n in index if n.endswith('.msh')),
                  'interpretation': 'Bone motion is relative to its parent, not whole-weapon/arm motion. Values use native fixed-point coordinates /4096; metre scale depends on mod world calibration. Small differences can be quantization. An animated part is not proof of independent native gameplay simulation.'}
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(f'PASS: {len(rigs)} inventory weapon rigs, {len(clips)} animation clips; archive checksums, node links, pose sizes and quaternion norms validated.')
    print(args.output)

if __name__ == '__main__':
    main()
