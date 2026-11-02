"""Validate actual exported skin, inverse binds, and every authored grip offline."""
from pathlib import Path
import numpy as np
from rig_supplied_arms import read_glb,accessor

path=Path(__file__).resolve().parents[1]/'assets/hands/ReplacementArms.glb'
doc,blob=read_glb(path);skin=doc['skins'][0];prim=doc['meshes'][0]['primitives'][0]
p=accessor(doc,blob,prim['attributes']['POSITION'])
weights=accessor(doc,blob,prim['attributes']['WEIGHTS_0'])
joints=accessor(doc,blob,prim['attributes']['JOINTS_0'])
inverse=accessor(doc,blob,skin['inverseBindMatrices']).reshape(-1,4,4).transpose(0,2,1)
assert len(skin['joints'])==36 and joints.max()<36
assert np.all(weights>=0) and np.allclose(weights.sum(axis=1),1,atol=2e-7)
parent={c:i for i,n in enumerate(doc['nodes'])for c in n.get('children',[])}
def pose(rotations):
    world={}
    def visit(i):
        if i in world:return world[i]
        n=doc['nodes'][i];m=np.eye(4);m[:3,3]=n.get('translation',[0,0,0])
        x,y,z,w=rotations.get(i,[0,0,0,1])
        m[:3,:3]=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                    [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                    [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]
        world[i]=(visit(parent[i])@m)if i in parent else m;return world[i]
    transforms=np.array([visit(i)@inverse[k]for k,i in enumerate(skin['joints'])])
    homogeneous=np.concatenate([p,np.ones((len(p),1))],axis=1)
    result=np.zeros((len(p),4))
    for slot in range(4):result+=np.einsum('nij,nj->ni',transforms[joints[:,slot]],homogeneous)*weights[:,slot,None]
    assert np.isfinite(result).all() and abs(result[:,:3]).max()<.6
    return result[:,:3]
assert np.allclose(pose({}),p,atol=1e-7)
for a in doc['animations']:
    rotations={c['target']['node']:accessor(doc,blob,a['samplers'][c['sampler']]['output'])[-1]for c in a['channels']}
    posed=pose(rotations)
    assert np.linalg.norm(posed-p,axis=1).max()>.005
    # Authoring finger curls must not drag the upper arm or detach its wrist root.
    assert np.allclose(posed[abs(p[:,0])<.34],p[abs(p[:,0])<.34],atol=1e-7)
print('PASS: exported bind reproduction, 36 joints, normalized weights, eight finite grip poses, stationary forearms')
