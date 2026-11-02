from pathlib import Path
import struct,json,numpy as np
folder=Path(__file__).resolve().parents[1]/'assets/hands'
raw=(folder/'NativeHands.bin').read_bytes();magic,version,count=struct.unpack_from('<4sII',raw);assert(magic,version,count)==(b'DHND',1,36)
counts=struct.unpack_from('<36I',raw,12);data=np.frombuffer(raw,dtype='<f4',offset=156).reshape(-1,12);assert sum(counts)==len(data)and np.isfinite(data).all()
offset=np.r_[0,np.cumsum(counts)];sections=[data[offset[i]:offset[i+1]]for i in range(36)];metadata=json.loads((folder/'native-hand-fit.json').read_text())
for i in range(0,36,2):
 if i==16:continue
 left,right=sections[i],sections[i+1];assert len(left)==len(right)==672
 mirror=left.reshape(-1,3,12)[:,[0,2,1]].reshape(-1,12).copy();mirror[:,0]*=-1;mirror[:,3]*=-1
 assert np.allclose(mirror,right,atol=1e-6),'Original hand pair must match in geometry, normals, texture and topology'
 lengths=np.linalg.norm(right[:,3:6],axis=1);assert np.allclose(lengths,1,atol=1e-5)
 assert np.abs(right[:,:3]).max()<.3
 assert np.allclose(right[-72:,:3].reshape(-1,3,3)[:,0,2][2::3],.028,atol=1e-6)
 assert np.isclose(right[:,2].max(),.028,atol=1e-6),'Wrist must end at band bottom'
for side in range(2):
 band=sections[16+side];assert np.isclose(np.ptp(band[:,2]),.028,atol=1e-6)
 assert np.isclose(band[:,2].max(),.028,atol=1e-6)and np.isclose(band[:,2].min(),0,atol=1e-6)
assert np.max(np.abs(sections[3][:,:3]-sections[5][:,:3]))>1e-5,'Native trigger motion must survive extraction'
print('PASS: actual native hand bank, matching mirrored anatomy, original trigger motion, finite unit normals and wrist ends flush with 28 mm band')
