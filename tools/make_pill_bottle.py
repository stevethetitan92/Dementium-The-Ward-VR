"""Closed worn white medicine bottle, thick red crosses and red ribbed cap."""
from pathlib import Path
import io,math,struct,zlib
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1]
rows=[]
def vertex(x,angle,radius,normal,color):
    return [x,-.032+math.cos(angle)*radius,-.072+math.sin(angle)*radius,*normal,*color,0,0,4]
segments=32
rings=[(-.032,-.027,.013,.015,False)]
for x0,x1 in zip(np.linspace(-.027,.021,17)[:-1],np.linspace(-.027,.021,17)[1:]):rings.append((x0,x1,.015,.015,True))
rings += [(.021,.025,.015,.014,False),(.025,.033,.014,.014,False)]
def label(x,angle,printed):
    # Deterministic wear stays attached to the bottle instead of flickering.
    grain=.5+.5*math.sin(angle*71+x*943)*math.cos(angle*29-x*619)
    stain=.06+.10*grain+.08*max(0,math.sin(angle*3+1))**8
    if x>=.025:
        rib=.86+.14*math.cos(angle*32)
        return np.array([.58,.045,.035])*rib*(1-stain)
    color=np.array([.88,.86,.78])*(1-stain)
    if printed:
        # Opposing labels remain legible in either hand.
        lateral=abs(angle % math.pi-math.pi/2)*.015
        height=abs(x+.003)
        if (lateral<.0038 and height<.018) or (lateral<.0105 and height<.005):
            color=np.array([.56,.045,.035])*(1-stain)
    return color
for x0,x1,r0,r1,printed in rings:
    for i in range(segments):
        a=2*math.pi*i/segments;b=2*math.pi*(i+1)/segments
        na=[0,math.cos(a),math.sin(a)];nb=[0,math.cos(b),math.sin(b)]
        p=vertex(x0,a,r0,na,label(x0,a,printed));q=vertex(x0,b,r0,nb,label(x0,b,printed));r=vertex(x1,a,r1,na,label(x1,a,printed));s=vertex(x1,b,r1,nb,label(x1,b,printed))
        rows.extend([p,q,r,q,s,r])
for x,radius,sign in [(-.032,.013,-1),(.033,.014,1)]:
    for i in range(segments):
        a=2*math.pi*i/segments;b=2*math.pi*(i+1)/segments;color=label(x,a,False);normal=[sign,0,0]
        tri=[vertex(x,a,radius,normal,color),vertex(x,b,radius,normal,color),vertex(x,0,0,normal,color)]
        rows.extend(tri if sign>0 else tri[::-1])
array=np.array(rows,dtype='<f4');assert len(array)==3840 and np.isfinite(array).all()
(root/'assets/hands/PillBottle.mesh').write_bytes(array.tobytes());print('Closed pill bottle:',len(array)//3,'triangles')
