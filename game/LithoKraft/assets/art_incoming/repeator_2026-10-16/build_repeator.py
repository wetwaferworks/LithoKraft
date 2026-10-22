#!/usr/bin/env python3
"""LithoKraft repeator (2/4/8/16 tick) — model + texture generator. CloudAtlas 2026-10-15.
Outputs .lthm v1 (level 5, 32^3), JSON voxel/box model, 128/64/32/16 textures, contact sheet."""
import json, struct, zlib, os
from PIL import Image, ImageDraw, ImageFont
OUT = os.path.dirname(os.path.abspath(__file__))
R = 32; LVL = 5
# materials (palette byte); tint=True means engine multiplies the neutral layer by the feeding Trace color
MATS = {1:("base","#2B3040",False),2:("bevel","#3C4357",False),3:("etch","#454D63",False),
        4:("glow_seam","#FFFFFF",True),5:("button_up","#9A9A9A",True),6:("button_down","#FFFFFF",True),
        7:("button_well","#161922",False)}
TINT = {"red":{"off":"#5A5556","on":"#FF514B"},"blue":{"off":"#535659","on":"#2FB4FF"}}
SLAB_H = 6            # 6/32 = 3/16 block tall
BTN_X = [5, 11, 17, 23]; BTN_W = 4; BTN_Z0, BTN_Z1 = 14, 17
UP_H, DOWN_H = 3, 1   # cap height above slab: up = 3 cells (1.5/16), pressed = 1 cell
STATES = [(1,2),(2,4),(3,8),(4,16)]  # (state, ticks) -> buttons pressed = state
def arrow_cells(z0, tip_z_low):
    """chevron arrow pointing to +z (front/output). returns (x,z) set"""
    s=set()
    for i in range(5):
        z = z0 + i
        for x in range(16-1-(4-i)*0+0, 16+1): pass
    # simple chevron: rows widening toward the back
    for i in range(6):
        z = z0 + i
        half = 6 - i
        for dx in (-half, -half+1, half-1, half):
            s.add((16+dx-1 if dx<0 else 15+dx+1-1, z))
    return s
def chevron(zc):
    s=set()
    for i in range(6):
        hw = 5-i
        for x in (15-hw, 15-hw+1, 16+hw-1, 16+hw):
            s.add((x, zc+i))
    return s
def build(state):
    v = {}
    for x in range(R):
        for z in range(R):
            for y in range(SLAB_H):
                edge = x in (0,R-1) or z in (0,R-1)
                v[(x,y,z)] = 2 if (edge and y==SLAB_H-1) else 1
            # glowing seam ring one cell in from the bevel on top
            if (x in (1,R-2) and 1<=z<=R-2) or (z in (1,R-2) and 1<=x<=R-2):
                v[(x,SLAB_H-1,z)] = 4
            # etched circuit lines (untinted, recessed look via color)
            if (z in (9,22) and 3<=x<=28 and x%2==0) or (x in (3,28) and 3<=z<=28):
                if v[(x,SLAB_H-1,z)]==1: v[(x,SLAB_H-1,z)] = 3
    # input chevron at back (z small), output chevron at front (z large): raised 1 cell, tint layer
    for (x,z) in chevron(3) | chevron(23): v[(x,SLAB_H,z)] = 4
    # output nub on front face, input nub on back face (tint)
    for x in range(14,18):
        for y in (2,3): v[(x,y,R-1)] = 4; v[(x,y,0)] = 4
    # button wells + caps
    for i,bx in enumerate(BTN_X):
        pressed = i < state
        for x in range(bx-1, bx+BTN_W+1):
            for z in range(BTN_Z0-1, BTN_Z1+2):
                v[(x,SLAB_H-1,z)] = 7
        h = DOWN_H if pressed else UP_H
        for x in range(bx, bx+BTN_W):
            for z in range(BTN_Z0, BTN_Z1+1):
                for y in range(SLAB_H, SLAB_H+h):
                    v[(x,y,z)] = 6 if pressed else 5
    return v
def crc_write_lthm(path, name, v):
    b = b"LTHM"+struct.pack("<HH",1,0)
    for s in (name, "CloudAtlas"):
        e=s.encode(); b+=struct.pack("<H",len(e))+e
    leaves = sorted(v.items(), key=lambda kv:(kv[0][2],kv[0][1],kv[0][0]))
    b += struct.pack("<BI", LVL, len(leaves))
    for (x,y,z),m in leaves: b += struct.pack("<BBBBB",LVL,x,y,z,m)
    b += struct.pack("<I", zlib.crc32(b)&0xffffffff)
    open(path,"wb").write(b)
def hexc(h): return tuple(int(h[i:i+2],16) for i in (1,3,5))
def mul(c,t,k=1.0): return tuple(min(255,int(a*b/255*k)) for a,b in zip(c,t))
def mat_color(m, tint, powered, lit_pressed=True):
    name,h,tintable = MATS[m]; c = hexc(h)
    if not tintable: return c
    if tint is None: return c
    on, off = hexc(TINT[tint]["on"]), hexc(TINT[tint]["off"])
    if m==5: return off                                # raised button: inert gray
    if m==6: return on if powered else mul(on,(255,255,255),0.55)  # pressed: accent, full when powered
    return on if powered else off                      # seams/arrows/nubs
def render_iso(v, tint, powered, s=6):
    W = int(R*2*s*0.866)+40; H = int(R*s + 14*s)+60
    img = Image.new("RGBA",(W,H),(0,0,0,0)); d = ImageDraw.Draw(img)
    cx, cy = W//2, 30+ 14*s
    def P(x,y,z): return (cx+(x-z)*s*0.866, cy+(x+z)*s*0.5 - y*s)
    for (x,y,z) in sorted(v, key=lambda p:(p[0]+p[1]+p[2], p[1])):
        c = mat_color(v[(x,y,z)], tint, powered)
        glow = MATS[v[(x,y,z)]][2] and tint and (powered or v[(x,y,z)]==6)
        if (x,y+1,z) not in v:
            d.polygon([P(x,y+1,z),P(x+1,y+1,z),P(x+1,y+1,z+1),P(x,y+1,z+1)], fill=c)
        if (x+1,y,z) not in v:
            d.polygon([P(x+1,y,z),P(x+1,y+1,z),P(x+1,y+1,z+1),P(x+1,y,z+1)], fill=mul(c,(255,)*3,0.78 if not glow else 0.92))
        if (x,y,z+1) not in v:
            d.polygon([P(x,y,z+1),P(x+1,y,z+1),P(x+1,y+1,z+1),P(x,y+1,z+1)], fill=mul(c,(255,)*3,0.6 if not glow else 0.85))
    return img
# ---------- textures (128 px, hard-edged; 4 px per model cell) ----------
def face_layers(v, face):
    """returns (base RGBA, tint mask L) for a face by projecting the model, 4px/cell"""
    base = Image.new("RGBA",(128,128),(0,0,0,0)); mask = Image.new("L",(128,128),0)
    db, dm = ImageDraw.Draw(base), ImageDraw.Draw(mask)
    def put(u,w,m):
        name,h,tintable = MATS[m]
        box=[u*4,w*4,u*4+3,w*4+3]
        if tintable:
            db.rectangle(box, fill=hexc("#161922")+(255,))
            g = {4:255,5:150,6:255}[m]; dm.rectangle(box, fill=g)
        else: db.rectangle(box, fill=hexc(h)+(255,))
    if face=="top":
        for x in range(R):
            for z in range(R):
                ys=[y for y in range(R) if (x,y,z) in v]; put(x,R-1-z if False else z,v[(x,max(ys),z)])
    else:
        for a in range(R):
            for y in range(R):
                if face=="front": col=[v[(a,y,z)] for z in range(R-1,-1,-1) if (a,y,z) in v]
                elif face=="back": col=[v[(R-1-a,y,z)] for z in range(R) if (R-1-a,y,z) in v]
                else: col=[v[(x,y,a)] for x in range(R-1,-1,-1) if (x,y,a) in v]
                if col: put(a, R-1-y, col[0])
    # bevel highlight (hard 1px) on base
    px=base.load()
    for i in range(128):
        for j in range(128):
            if px[i,j][3] and (i==0 or j==0): px[i,j]=hexc("#5C6680")+(255,)
    return base, mask
def composite(base, mask, tint_hex):
    t = Image.new("RGBA",base.size, hexc(tint_hex)+(255,))
    m = mask.point(lambda g: 255 if g else 0)
    shade = Image.merge("RGBA",[mask,mask,mask,Image.new("L",base.size,255)])
    from PIL import ImageChops
    tinted = ImageChops.multiply(t, shade)
    out = base.copy(); out.paste(tinted,(0,0),m); return out
def save_sizes(img, rel):
    for sz in (128,64,32,16):
        p=os.path.join(OUT,f"pack_{sz}",rel); os.makedirs(os.path.dirname(p),exist_ok=True)
        (img if sz==128 else img.resize((sz,sz),Image.NEAREST)).save(p)
def main():
    os.makedirs(f"{OUT}/models",exist_ok=True)
    index={"format":"lithokraft_repeator_voxel_v1","resolution":R,"lthm_level":LVL,"axes":"x=east(right), y=up, z=south(front/output). input at z=0, output at z=31",
           "materials":{str(k):{"name":n,"neutral_hex":h,"tint":t} for k,(n,h,t) in MATS.items()},
           "tint_colors":TINT,"states":[]}
    renders={}
    for st,ticks in STATES:
        v=build(st); name=f"repeator_d{ticks}"
        crc_write_lthm(f"{OUT}/models/{name}.lthm", name, v)
        json.dump({"name":name,"state":st,"delay_ticks":ticks,"buttons_pressed":st,"resolution":R,
                   "voxels":[[x,y,z,m] for (x,y,z),m in sorted(v.items(),key=lambda kv:(kv[0][2],kv[0][1],kv[0][0]))]},
                  open(f"{OUT}/models/{name}.voxels.json","w"))
        index["states"].append({"state":st,"delay_ticks":ticks,"buttons_down":list(range(st)),"lthm":f"models/{name}.lthm",
                                "voxels_json":f"models/{name}.voxels.json","cells":len(v),"next_state":(st%4)+1})
        for face in ("top","front","back","side"):
            b,m = face_layers(v,face)
            if face!="top" and st>1: continue   # side/front/back identical across states (buttons only visible in top)
            tag = f"_d{ticks}" if face=="top" else ""
            save_sizes(b, f"textures/blocks/repeator/repeator_{face}{tag}_base.png")
            save_sizes(m, f"textures/blocks/repeator/repeator_{face}{tag}_tintmask.png")
            for tn in TINT:
                for pw in ("off","on"):
                    save_sizes(composite(b,m,TINT[tn][pw]), f"preview_tinted/{tn}/repeator_{face}{tag}_{pw}.png")
        for tn in TINT:
            for pw in (False,True): renders[(st,tn,pw)] = render_iso(v,tn,pw)
    # button sub-pieces
    for nm,h,m in (("button_up",UP_H,5),("button_down",DOWN_H,6)):
        v={(x,y,z):m for x in range(BTN_W) for y in range(h) for z in range(BTN_Z1-BTN_Z0+1)}
        crc_write_lthm(f"{OUT}/models/repeator_{nm}.lthm", f"repeator_{nm}", v)
    index["button_subpieces"]={"up":"models/repeator_button_up.lthm (4x3x4 cells)","down":"models/repeator_button_down.lthm (4x1x4 cells)",
        "slots_x":BTN_X,"slot_z":[BTN_Z0,BTN_Z1],"seat_y":SLAB_H}
    json.dump(index,open(f"{OUT}/models/repeator_index.json","w"),indent=2)
    # contact sheet
    cw,ch = renders[(1,"red",False)].size
    sheet = Image.new("RGB",(cw*4+40, (ch+22)*4+120), hexc("#14161E")); d=ImageDraw.Draw(sheet)
    try: f=ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",15)
    except: f=None
    d.text((20,10),"LithoKraft repeator 2026-10-15: pressed buttons = delay (1=2t, 2=4t, 3=8t, 4=16t). Rows: red unpowered / red powered / blue unpowered / blue powered",fill=(200,200,230),font=f)
    rows=[("red",False),("red",True),("blue",False),("blue",True)]
    for r,(tn,pw) in enumerate(rows):
        for c,(st,ticks) in enumerate(STATES):
            x0=20+c*cw; y0=40+r*(ch+22)
            sheet.paste(renders[(st,tn,pw)],(x0,y0),renders[(st,tn,pw)])
            d.text((x0+10,y0+ch-4),f"{tn} {'ON' if pw else 'OFF'}  {ticks} ticks ({st} down)",fill=(220,220,220),font=f)
    # top textures strip
    y0=40+4*(ch+22); x=20
    for st,ticks in STATES:
        for tn in TINT:
            im=Image.open(f"{OUT}/pack_128/preview_tinted/{tn}/repeator_top_d{ticks}_on.png").resize((64,64),Image.NEAREST)
            sheet.paste(im,(x,y0)); x+=70
    sheet.save(f"{OUT}/contact_sheet_repeator.png")
main()
