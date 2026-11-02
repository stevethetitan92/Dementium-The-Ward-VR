"""Audit our baked hand banks against the runtime morph eligibility rule.

UV/color correspondence is a necessary diagnostic, not proof of anatomical fit.
Reads only mod-authored export files; does not inspect the game.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import numpy as np


def audit(path):
    raw = path.read_bytes()
    magic, version, n = struct.unpack_from('<4sII', raw)
    if magic != b'DHND' or version != 1 or n not in (18, 28, 36, 64):
        raise ValueError('Unsupported mod hand-bank header')
    counts = struct.unpack_from(f'<{n}I', raw, 12)
    if any(c == 0 or c % 3 for c in counts):
        raise ValueError('Invalid triangle section count')
    data = np.frombuffer(raw, '<f4', offset=12 + 4*n).reshape(-1, 12)
    if len(data) != sum(counts) or not np.isfinite(data).all():
        raise ValueError('Invalid vertex payload')
    offsets = np.r_[0, np.cumsum(counts)]
    sections = [data[offsets[i]:offsets[i+1]] for i in range(n)]
    mismatches = []
    checked = 0
    for a in range(n):
        for b in range(a+1, n):
            # Same-side transitions only; mirrors intentionally reverse winding.
            if a % 2 != b % 2 or counts[a] != counts[b]:
                continue
            eligible = (a < 36 and b < 36) or (
                a >= 36 and b >= 36 and (a-36)//4 == (b-36)//4)
            if not eligible:
                continue
            checked += 1
            # Position/normals may change. Material/UV identity should not.
            error = float(np.max(np.abs(sections[a][:, 6:] - sections[b][:, 6:])))
            if error > 1e-6:
                mismatches.append({'from': a, 'to': b, 'attribute_error': error})
    return {'file': str(path.resolve()), 'sha256': hashlib.sha256(raw).hexdigest(),
            'sections': n, 'checked_same_side_pairs': checked,
            'attribute_mismatches': mismatches,
            'limits': 'Matching attributes do not prove vertex identity, contact fit or headset acceptance.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('bank', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = audit(args.bank)
    result = json.dumps(report, indent=2)
    if args.output:
        args.output.write_text(result + '\n', encoding='utf-8')
    print(result)
