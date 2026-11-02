"""Read-only weapon dependency audit. Never exports, deploys, or edits saved fits."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
STAGING = ROOT / 'work/psx-runtime-staging'
WEAPONS = ['revolver', 'pistol', 'm16', 'shotgun', 'hunting-rifle', 'buzzsaw', 'nightstick']


def fingerprint(path):
    if not path.is_file():
        return {'path': str(path), 'missing': True}
    data = path.read_bytes()
    return {'path': str(path), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def model_info(path):
    data = path.read_bytes()
    if len(data) < 20:
        raise ValueError('Truncated GLB')
    magic, version, length, size, kind = struct.unpack_from('<5I', data)
    if magic != 0x46546C67 or version != 2 or length != len(data) or kind != 0x4E4F534A or 20 + size > length:
        raise ValueError('Invalid GLB header or JSON chunk')
    doc = json.loads(data[20:20 + size])
    return {
        'nodes': [{'index': i, 'name': n.get('name'), 'mesh': n.get('mesh'), 'skin': n.get('skin')} for i, n in enumerate(doc.get('nodes', []))],
        'meshes': len(doc.get('meshes', [])), 'materials': len(doc.get('materials', [])),
        'skins': len(doc.get('skins', [])),
        'animations': [{'name': a.get('name'), 'channels': len(a.get('channels', []))} for a in doc.get('animations', [])],
        'part_identity': 'Node names are metadata, not verified mechanical identities.'
    }


def snapshot(staging):
    configs = json.loads((staging / 'selected-preview-configs.json').read_text())
    profiles = {}
    for index, slug in enumerate(WEAPONS):
        label = next(k for k in configs if k.lower().replace(' ', '-') == slug)
        names = [slug + '-selected.glb', slug + '-right-fit.json', slug + '-right.mesh']
        if index in (2, 3, 4, 5):
            names += [slug + '-left-fit.json', slug + '-left.mesh', slug + '-left-wrist.mesh', slug + '-wrist-frame.bin']
        profiles[slug] = {'runtime_slot': index, 'weapon_matrix': configs[label]['weaponMatrix'],
                          'files': {name: fingerprint(staging / name) for name in names}}
    shared = ['SelectedGeometry.psx', 'SelectedGeometry.png', 'SelectedFrames.bin', 'hand-texture-0.png', 'left-wrist-frames.json']
    return {'schema': 1, 'scope': 'staged files only; not installed state or visual acceptance',
            'weapons': profiles, 'shared': {name: fingerprint(staging / name) for name in shared}}


def compare(before, after):
    changes = []
    for slug in WEAPONS:
        old, new = before['weapons'][slug], after['weapons'][slug]
        files = [name for name, value in new['files'].items() if value != old['files'].get(name)]
        if files or old['weapon_matrix'] != new['weapon_matrix']:
            changes.append({'weapon': slug, 'changed_files': files,
                            'model_transform_changed': old['weapon_matrix'] != new['weapon_matrix'],
                            'required': ['rebuild shared geometry/atlas if geometry or material changed',
                                         're-export affected hand geometry and attachment frames if fit changed',
                                         'inspect grip, support contact, muzzle and moving-part alignment',
                                         'desktop stereo verification, then headset acceptance']})
    return {'weapons': changes, 'shared_changed': [k for k, v in after['shared'].items() if v != before['shared'].get(k)],
            'safe_to_install': False, 'reason': 'This report detects dependencies; it does not certify visual or runtime compatibility.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staging', type=Path, default=STAGING)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--candidate', type=Path)
    parser.add_argument('--weapon', choices=WEAPONS)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.candidate and not args.weapon:
        parser.error('--candidate requires --weapon')
    result = snapshot(args.staging)
    if args.baseline:
        result = compare(json.loads(args.baseline.read_text()), result)
    if args.candidate:
        result['candidate_review'] = {'weapon': args.weapon, 'file': fingerprint(args.candidate),
            'model': model_info(args.candidate), 'applied': False,
            'required': ['verify actual part identities and local axes', 'calibrate model placement against saved hand contacts',
                         'verify muzzle/effects alignment and reload-part bindings', 'preserve original model and fit files']}
    # Exclusive creation prevents accidentally overwriting a saved fit or baseline.
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(result, stream, indent=2)
    print(str(args.output))


if __name__ == '__main__':
    main()
