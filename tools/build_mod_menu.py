from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageFilter
import numpy as np
import json
ROOT=Path(__file__).resolve().parents[1]; A=ROOT/'assets/inventory'
font=lambda n:ImageFont.truetype(str(A/'HelpMe.ttf'),n)
ink=(43,39,32,255); lightInk=(230,221,198,255); red=(96,29,27,255)
def page(title,settings_page=True):
 rng=np.random.default_rng(291029)
 im=Image.new('RGBA',(1000,900),(0,0,0,0));d=ImageDraw.Draw(im)
 d.rounded_rectangle((9,7,991,896),radius=25,fill=(58,48,37),outline=(121,103,77),width=3)
 for j in range(180):
  x=int(rng.integers(16,984));y=int(rng.integers(15,886))
  d.line((x,y,x+int(rng.integers(4,28)),y+int(rng.integers(-3,4))),fill=(91,76,54),width=1)
 d.rectangle((31,43,974,879),fill=(27,25,20))
 # Weathered chart paper with stains concentrated at the edges, away from text.
 paper=Image.new('RGBA',(938,830),(214,204,176,255));stain=Image.new('RGBA',paper.size);sd=ImageDraw.Draw(stain)
 for j in range(90):
  x=int(rng.integers(-55,990));y=int(rng.integers(-45,870));r=int(rng.integers(18,90))
  alpha=55 if x<65 or x>865 or y<60 or y>780 else 13
  sd.ellipse((x-r,y-r,x+r,y+r),fill=(78,57,30,alpha))
 paper=Image.alpha_composite(paper,stain.filter(ImageFilter.GaussianBlur(17)))
 pd=ImageDraw.Draw(paper)
 for y in range(20,830,28):pd.line((8,y,929,y),fill=(182,173,149,255),width=1)
 for j in range(330):
  x=int(rng.integers(0,938));y=int(rng.integers(0,830));pd.line((x,y,x+int(rng.integers(1,5)),y),fill=(170,158,130,255))
 im.alpha_composite(paper,(28,43));d=ImageDraw.Draw(im)
 # Small old blood marks on the margins, not over instructions.
 for bx,by in [(949,93),(46,801)]:
  for j in range(13):
   x=bx+int(rng.integers(-17,17));y=by+int(rng.integers(-20,20));r=int(rng.integers(1,5))
   d.ellipse((x-r,y-r,x+r,y+r),fill=(93+int(rng.integers(0,20)),35,27))
 d.line((953,105,951,133),fill=(109,39,28),width=3)
 # Stamped section title and a sprung, tarnished steel clipboard clip.
 d.line((47,104,950,104),fill=red,width=3)
 d.text((500,72),title,font=font(39),fill=red,anchor='mm')
 d.rounded_rectangle((384,5,616,47),radius=7,fill=(49,48,43),outline=(137,132,116),width=2)
 for y in range(9,41):
  v=int(120+55*np.sin((y-9)/32*np.pi));d.line((391,y,609,y),fill=(v,v-3,v-12),width=1)
 d.rounded_rectangle((409,11,591,27),radius=6,fill=(55,55,49),outline=(189,183,163),width=2)
 for x in (399,601):
  d.ellipse((x-4,30,x+4,38),fill=(66,64,55),outline=(183,176,154));d.line((x-2,34,x+2,34),fill=(28,27,24),width=1)
 if settings_page:d.text((500,132),'SETTINGS    /    CONTROLS',font=font(30),fill=ink,anchor='mm')
 if settings_page:d.text((500,830),'GRIPS CHANGE TABS   A: CHANGE   B: RESUME',font=font(24),fill=ink,anchor='mm')
 return im,d
im,d=page('VR MOD MENU')
d.rounded_rectangle((60,205,940,355),radius=10,fill=None,outline=(180,169,140),width=3)
d.text((95,240),'WHITE HAZE',font=font(37),fill=ink)
d.rounded_rectangle((60,365,940,515),radius=10,fill=None,outline=(180,169,140),width=3)
d.text((95,400),'BLACK FOG',font=font(37),fill=ink)
im.save(A/'mod-menu-settings.png');settings=im
im,d=page('QUEST TOUCH PLUS',False)
# Paper remains visible behind the page heading.
d.text((500,132),'CONTROLS  1 / 2  -  BUTTONS',font=font(28),fill=ink,anchor='mm')
for left,cx in [(True,255),(False,745)]:
 d.rounded_rectangle((cx-220,174,cx+220,780),radius=9,fill=None,outline=(112,111,99),width=2)
 d.text((cx,200),'LEFT CONTROLLER' if left else 'RIGHT CONTROLLER',font=font(26),anchor='mm',fill=ink)
 model=Image.open(A/('quest-touch-plus-'+('left' if left else 'right')+'.png')).convert('RGBA')
 original_width=model.width
 model.thumbnail((230,245),Image.Resampling.LANCZOS)
 im.alpha_composite(model,(cx-model.width//2,224))
 anchors=json.loads((A/('quest-touch-plus-'+('left' if left else 'right')+'.json')).read_text())
 def point(key):
  x,y=anchors[key];return (cx-model.width//2+x*model.width/original_width,224+y*model.width/original_width)
 for key,label in [('button_x','X'),('button_y','Y')] if left else [('button_a','A'),('button_b','B')]:
  px,py=point(key);d.ellipse((px-8,py-8,px+8,py+8),fill=red,outline=ink);d.text((px,py),label,font=font(12),anchor='mm',fill=lightInk)
 def callout(key,label,xy):
  px,py=point(key);tx,ty=xy
  d.text((tx,ty),label,font=font(17),fill=ink)
  edge=tx+80 if tx<cx else tx
  d.line([(edge,ty+21),(edge,py),(px,py)],fill=(109,90,58),width=1)
  d.ellipse((px-2,py-2,px+2,py+2),fill=ink)
 callout('thumbstick','STICK',(cx-198 if left else cx+118,245))
 callout('button_grip','GRIP',(cx+118 if left else cx-198,365))
 callout('trigger','TRIGGER',(cx-198 if left else cx+118,405))
 d.text((cx-198 if left else cx+118,439),'FRONT BUTTON',font=font(12),fill=(96,81,60))
 rows=[('STICK','MOVE FORWARD, BACK AND SIDEWAYS'),('Y','OPEN / CLOSE INVENTORY'),('X','CYCLE WEAPON IN GAMEPLAY'),('GRIP','HOLD TO GRAB AMMO OR SUPPORT GUN'),('TRIGGER','CONSUME PILL HELD IN THIS HAND'),('STICK CLICK','FLASHLIGHT ON / OFF (HELD OR MOUNTED)')] if left else [('STICK','LEFT / RIGHT: TURN'),('A','EXAMINE; PRESS AGAIN TO TAKE'),('B','EJECT MAG / OPEN REVOLVER CYLINDER'),('GRIP','GRAB / STOW AT YOUR HOLSTER'),('TRIGGER','FIRE GUN / RUN SAW / USE HELD PILL'),('STICK CLICK','WITH LEFT STICK CLICK: MOD MENU')]
 for j,(button,action) in enumerate(rows):
  yy=474+j*49
  d.text((cx-197,yy),button,font=font(19),fill=(112,57,37))
  d.text((cx-197,yy+22),action,font=font(17),fill=ink)
  d.line((cx-197,yy+44,cx+197,yy+44),fill=(159,147,119))
# Page-specific navigation below.
d.text((500,804),'CLICK BOTH STICKS TO OPEN THE MOD MENU',font=font(22),anchor='mm',fill=ink)
d.text((500,847),'GRIPS CHANGE TABS   B: RESUME',font=font(23),anchor='mm',fill=ink)
im.save(A/'mod-menu-controls.png');controls=im
im,d=page('RELOADING & ITEMS',False)
# Paper remains visible behind the page heading.
d.text((500,132),'CONTROLS  2 / 2  -  HOW TO PLAY',font=font(28),fill=ink,anchor='mm')
cards=[
 ('MAGAZINE-FED GUNS',['PRESS B TO DROP THE GUN MAGAZINE.','HOLD LEFT GRIP AT YOUR LEFT HIP','TO GRAB A SPARE. KEEP HOLDING GRIP','AND PUSH IT INTO THE MAGAZINE WELL.']),
 ('SLIDE / CHARGING HANDLE',['AFTER AN EMPTY RELOAD, LET GO OF','THE MAGAZINE FIRST. GRAB THE SLIDE','OR HANDLE, PULL BACK, THEN RELEASE.','A CHAMBERED ROUND NEEDS NO RACK.']),
 ('SHOTGUN',['HOLD LEFT GRIP ON THE FORE-END.','AFTER FIRING, PULL FULLY BACK','THEN PUSH FULLY FORWARD.','LOAD SHELLS FROM YOUR LEFT HIP.','YOU DO NOT NEED TO PRESS B.']),
 ('REVOLVER',['PRESS B TO OPEN THE CYLINDER.','GRAB A SPEEDLOADER FROM LEFT HIP','WITH LEFT GRIP. GUIDE THE ROUNDS','INTO THE OPEN CYLINDER.','CLOSE THE CYLINDER BEFORE FIRING.']),
 ('HOLSTERS & MELEE',['RIGHT GRIP AT A HOLSTER: DRAW / STOW.','RIGHT HIP: SIDEARM. SHOULDER: LONG GUN.','LEFT SHOULDER: NIGHTSTICK / BUZZSAW.','SWING THE NIGHTSTICK TO STRIKE.','HOLD TRIGGER TO RUN THE BUZZSAW.']),
 ('PILLS & MENUS',['GRAB A PILL FROM YOUR FRONT HIP','WITH A FREE HAND. ITS TRIGGER','CONSUMES THE PILL. YOU CAN CARRY 2.','IN INVENTORY: STICK SELECTS, A EQUIPS.','B CLOSES THE MENU.'])]
for i,(title,lines) in enumerate(cards):
 x=45+(i%2)*470;y=178+(i//2)*202
 d.rounded_rectangle((x,y,x+440,y+188),radius=8,fill=None,outline=(112,111,99),width=2)
 d.rectangle((x+1,y+1,x+439,y+39),fill=red)
 d.text((x+16,y+9),title,font=font(23),fill=lightInk)
 for j,line in enumerate(lines):d.text((x+16,y+53+j*24),line,font=font(18),fill=ink)
# Page-specific navigation below.
d.text((500,808),'GRIP = SQUEEZE WITH YOUR MIDDLE FINGER',font=font(22),anchor='mm',fill=ink)
d.text((500,847),'GRIPS CHANGE TABS   B: RESUME',font=font(23),anchor='mm',fill=ink)
im.save(A/'mod-menu-reloading.png')
packed=Image.new('RGBA',(2048,2048));packed.paste(settings,(0,0));packed.paste(controls,(1024,0));packed.paste(im,(0,1024))
d=ImageDraw.Draw(packed)
for y,label in [(915,'ON'),(965,'OFF')]:d.text((15,y),label,font=font(34),fill=ink)
pixels=np.asarray(packed).astype('uint32');w=(pixels[:,:,0]|pixels[:,:,1]<<8|pixels[:,:,2]<<16|pixels[:,:,3]<<24).ravel();starts=np.r_[0,np.flatnonzero(w[1:]!=w[:-1])+1];ends=np.r_[starts[1:],len(w)]
with (ROOT/'src/mod_menu_art.hpp').open('w') as f:
 f.write('#pragma once\n#include <cstdint>\nnamespace modMenuArt {struct Run{unsigned count;std::uint32_t rgba;};inline constexpr Run runs[]={\n')
 for a,b in zip(starts,ends):f.write('{%d,0x%08xu},\n'%(b-a,w[a]))
 f.write('};}\n')





