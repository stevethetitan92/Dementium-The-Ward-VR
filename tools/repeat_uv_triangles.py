"""Split repeat-wrapped UV triangles into atlas-safe tiles without clamping detail."""
import numpy as np

def split_repeat(stream, uv_columns=(9,10)):
    stream=np.asarray(stream,dtype=float)
    if len(stream)%3 or not np.isfinite(stream).all():raise ValueError('Invalid triangle stream')
    out=[]
    def clip(poly,col,bound,greater):
        result=[]
        for a,b in zip(poly,np.roll(poly,-1,axis=0)):
            da=(a[col]-bound)*(1 if greater else -1);db=(b[col]-bound)*(1 if greater else -1)
            if da>=0:result.append(a)
            if (da>=0)!=(db>=0):result.append(a+(b-a)*(da/(da-db)))
        return np.array(result)
    for first in range(0,len(stream),3):
        tri=stream[first:first+3];uv=tri[:,uv_columns]
        lo=np.floor(uv.min(0)).astype(int);hi=np.ceil(uv.max(0)).astype(int)-1
        hi=np.maximum(hi,lo)
        if np.prod(hi-lo+1)>4096:raise ValueError('Excessive UV repetition')
        if np.all(lo==hi):
            piece=tri.copy();piece[:,uv_columns]-=lo;out.append(piece);continue
        for u in range(lo[0],hi[0]+1):
            for v in range(lo[1],hi[1]+1):
                poly=tri.copy()
                for col,bound,greater in [(uv_columns[0],u,True),(uv_columns[0],u+1,False),(uv_columns[1],v,True),(uv_columns[1],v+1,False)]:
                    if len(poly)<3:break
                    poly=clip(poly,col,bound,greater)
                if len(poly)<3:continue
                poly[:,uv_columns]-=[u,v]
                for i in range(1,len(poly)-1):
                    t=poly[[0,i,i+1]]
                    if np.linalg.norm(np.cross(t[1,:3]-t[0,:3],t[2,:3]-t[0,:3]))>1e-15:out.append(t)
    return np.concatenate(out) if out else np.empty((0,stream.shape[1]))
