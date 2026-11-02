"""Offline reference evaluator for local-space weapon draw packages.

Each optional delta is applied after that node's rest transform. This keeps a
measured pivot in place while rotating its children. Not a runtime renderer.
"""
import hashlib
import copy
import json
from pathlib import Path
import numpy as np


def local_matrix(node):
    if 'matrix' in node:
        if any(k in node for k in ('translation', 'rotation', 'scale')):
            raise ValueError('Node mixes matrix and TRS')
        return np.asarray(node['matrix'], dtype=float).reshape(4, 4).T
    x, y, z, w = node.get('rotation', [0, 0, 0, 1])
    q = np.array([x, y, z, w], dtype=float)
    if not np.isfinite(q).all() or abs(np.linalg.norm(q)-1) > 1e-4:
        raise ValueError('Invalid rotation quaternion')
    q /= np.linalg.norm(q)
    x, y, z, w = q
    r = np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
                  [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
                  [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])
    m = np.eye(4)
    m[:3, :3] = r @ np.diag(node.get('scale', [1, 1, 1]))
    m[:3, 3] = node.get('translation', [0, 0, 0])
    return m


def world_matrices(manifest, deltas=None):
    deltas = deltas or {}
    nodes = manifest['nodes']
    if any(not isinstance(i, int) or not 0 <= i < len(nodes) for i in deltas):
        raise ValueError('Unknown delta node')
    worlds = {}
    def visit(i, parent):
        if i in worlds or not 0 <= i < len(nodes):
            raise ValueError('Invalid or repeated hierarchy node')
        rest = local_matrix(nodes[i])
        delta = np.asarray(deltas.get(i, np.eye(4)), dtype=float)
        for matrix in (rest, delta):
            if matrix.shape != (4, 4) or not np.isfinite(matrix).all():
                raise ValueError('Invalid transform')
            if not np.allclose(matrix[3], [0, 0, 0, 1], atol=1e-8):
                raise ValueError('Non-affine transform')
        worlds[i] = parent @ rest @ delta
        for child in nodes[i].get('children', []):
            visit(child, worlds[i])
    for root in manifest['scene_roots']:
        visit(root, np.eye(4))
    if set(deltas)-worlds.keys():
        raise ValueError('Delta targets an inactive node')
    return worlds


def animated_manifest(manifest, arrays, clip_index, time_seconds):
    """Sample source TRS channels as absolute values, never additive deltas."""
    if not np.isfinite(time_seconds):
        raise ValueError('Invalid animation time')
    if not isinstance(clip_index, int) or not 0 <= clip_index < len(manifest['animations']):
        raise ValueError('Unknown animation clip')
    clip = manifest['animations'][clip_index]
    result = copy.deepcopy(manifest)
    targeted = set()
    for channel in clip['channels']:
        target = channel['target']; index = target['node']; path = target['path']
        if path not in ('translation', 'rotation', 'scale') or 'matrix' in result['nodes'][index]:
            raise ValueError('Unsupported animated node or path')
        if (index, path) in targeted:
            raise ValueError('Duplicate animation channel')
        targeted.add((index, path))
        sampler = clip['samplers'][channel['sampler']]
        mode = sampler['interpolation']
        if mode not in ('LINEAR', 'STEP'):
            raise ValueError('Unsupported interpolation: '+mode)
        times = arrays[sampler['input']].ravel()
        values = arrays[sampler['output']].astype(float)
        if not len(times) or len(times) != len(values) or not np.all(np.diff(times)>0):
            raise ValueError('Invalid key times')
        if not np.isfinite(times).all() or not np.isfinite(values).all():
            raise ValueError('Non-finite animation')
        if values.shape[1] != (4 if path == 'rotation' else 3):
            raise ValueError('Invalid animation value width')
        right = int(np.searchsorted(times, time_seconds, side='right'))
        if right == 0: value = values[0]
        elif right == len(times): value = values[-1]
        elif mode == 'STEP': value = values[right-1]
        else:
            a, b = values[right-1].copy(), values[right].copy()
            u = (time_seconds-times[right-1])/(times[right]-times[right-1])
            if path == 'rotation':
                if min(np.linalg.norm(a), np.linalg.norm(b)) < 1e-12:
                    raise ValueError('Zero quaternion')
                a /= np.linalg.norm(a); b /= np.linalg.norm(b)
                dot = float(a@b)
                if dot < 0: b = -b; dot = -dot
                if dot > .9995: value = a*(1-u)+b*u
                else:
                    angle = np.arccos(np.clip(dot, -1, 1))
                    value = (a*np.sin((1-u)*angle)+b*np.sin(u*angle))/np.sin(angle)
            else: value = a*(1-u)+b*u
        if path == 'rotation':
            if np.linalg.norm(value) < 1e-12: raise ValueError('Zero quaternion')
            value = value/np.linalg.norm(value)
        result['nodes'][index][path] = value.tolist()
    return result


def posed_draws(package, deltas=None, clip_index=None, time_seconds=0.):
    package = Path(package)
    manifest = json.loads((package/'draws.json').read_text())
    geometry = package/'geometry.npz'
    if hashlib.sha256(geometry.read_bytes()).hexdigest() != manifest['geometry_sha256']:
        raise ValueError('Geometry hash mismatch')
    result = []
    with np.load(geometry, allow_pickle=False) as arrays:
        if clip_index is not None:
            manifest = animated_manifest(manifest, arrays, clip_index, time_seconds)
        worlds = world_matrices(manifest, deltas)
        for draw in manifest['draws']:
            matrix = worlds[draw['node']]
            positions = arrays[draw['attributes']['POSITION']['array']]
            posed = np.column_stack((positions, np.ones(len(positions)))) @ matrix.T
            result.append({'node': draw['node'], 'material': draw['material'],
                           'positions': posed[:, :3], 'indices': arrays[draw['indices']].copy()})
    return result
