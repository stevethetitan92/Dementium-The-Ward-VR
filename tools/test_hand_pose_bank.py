"""Validate the actual baked triangle streams, including bolt and stock poses."""
from pathlib import Path
import struct,json,numpy as np
from rig_supplied_arms import read_glb,accessor
folder=Path(__file__).resolve().parents[1]/'assets/hands'
raw=(folder/'FloatingHands.bin').read_bytes();magic,version,count=struct.unpack_from('<4sII',raw)
assert magic==b'DHND'and version==1 and count==36
counts=struct.unpack_from('<'+str(count)+'I',raw,12);data=np.frombuffer(raw,dtype='<f4',offset=12+count*4).reshape(-1,12)
doc,blob=read_glb(folder/'FloatingHands-WristHUD.glb');p=accessor(doc,blob,doc['meshes'][0]['primitives'][0]['attributes']['POSITION']);rest=p[p[:,0]>0]
lengths=np.linalg.norm(rest.reshape(-1,3,3)[:,[1,2,0]]-rest.reshape(-1,3,3),axis=2)*1.6;valid=lengths>.001
sections=[];cursor=0
for n in counts:sections.append(data[cursor:cursor+n]);cursor+=n
assert cursor==len(data)and np.isfinite(data).all()
labels=json.loads((folder/'runtime-poses.json').read_text())['sections'];report=[]
for i in range(0,36,2):
 if i==16:continue # wristband is separate geometry, not a skinned pose
 left,right=sections[i],sections[i+1];assert len(left)==len(right)==len(rest)
 mirror=left[:,:3].reshape(-1,3,3)[:,[0,2,1]].reshape(-1,3).copy();mirror[:,0]*=-1
 mismatch=float(np.linalg.norm(mirror-right[:,:3],axis=1).max());assert mismatch<1e-5,(i,mismatch)
 for k in [i,i+1]:
  tri=(mirror if k==i else right[:,:3]).reshape(-1,3,3);edge=np.linalg.norm(tri[:,[1,2,0]]-tri,axis=2)
  ratio=edge[valid]/lengths[valid];maximum=float(ratio.max());p99=float(np.quantile(ratio,.99))
  assert maximum<8,(labels[k]['name'],maximum)
  report.append({'pose':labels[k]['name'],'max_edge_stretch_above_1mm':maximum,'p99_edge_stretch_above_1mm':p99,'mirror_error_metres':mismatch})
(folder/'pose-bank-quality.json').write_text(json.dumps(report,indent=2))
print('PASS: all 34 hand streams finite, exact mirrored pairs, bounded mesh deformation; wristband excluded from skin checks')
