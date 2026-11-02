# Character display spec (standing sprites + Live2D)

Tags: **[C]** = CONFIRMED from code or data. **[A]** = ASSUMED.
Addresses are Ghidra addresses in `EBOOT.BIN.dec`. "Tick" = one 60 Hz scene update (`FUN_0004f278`) [A: 60 Hz is inferred from the `ms*60/1000` conversion in every native].
Coordinates: world origin at screen centre, X right, Y **up**, screen ±240 / ±136 [C: ortho(-240,240,-136,136,-1,1), eye (0,0,0), look (0,0,-1), up (0,1,0), scene draw `FUN_0005455c`].

Verification done offline against the real files:
- All 785 `.csu` parsed; overlay rectangles visually line up with the body (eyes half/closed, 3 mouth shapes) [C].
- All 15 `.lbd` parsed, each moc consumed to the exact last byte including the `0x88888888` sentinel [C].
- A Python re-implementation of sections 5-7 rendered `L2DMSR_U` correctly at default parameters and with an expression plus head turn applied (face, hair, arms, deformers all correct) [C].
- Not compared frame-by-frame with the running game: the emulator was out of scope for this task.

---

## 1. Draw order in the scenario scene [C]

`scene draw` pushes a model translate before each layer. Larger Z is nearer.

| Order | Layer | world Z |
|---|---|---|
| 1 | BG/CG `+0x1abdc0` | -1.0 |
| 2 | msg window B `+0x152c40` (if UI visible) | -0.9 |
| 3 | character slot 0 `+0x356180` | -0.8 |
| 4 | character slot 1 `+0x3f3480` | -0.7 |
| 5 | Live2D `+0x4a0bc0` | -0.6 |
| 6+ | msg window A etc. | not in this spec |

Global GE state (`FUN_00000924`) [C]: depth test ON with func 7 (GEQUAL), depth write ON (`0xE7000000`), depth range (0xFFFF, 0) so nearer = larger depth, depth clear 0; blend ON; scissor (0,0,480,272); clip planes ON; alpha test never enabled.
Every wrapper draw (`FUN_00000be0`) sets ADD, SRC_ALPHA, ONE_MINUS_SRC_ALPHA; tex func MODULATE with RGBA.

Exactly **2** slots exist (loops `i<2`). Scripts map three stand positions onto them.

---

## 2. CSU sprite format [C]

Little endian. All 785 files fit. Max load size `0x4B000` bytes (buffer size, `DAT_001f5080`).

```
+00 'CSU\0'
+04 u32 anchorOff   always 0x20
+08 u32 bodyOff     always 0x30  -> XGTL
+0C u32 eyeHdrOff
+10 u32 eyeTexOff   (eyeHdrOff+0x20)
+14 u32 mouthHdrOff
+18 u32 mouthTexOff (mouthHdrOff+0x20)
+1C 0

anchor block (at anchorOff):  i32 ax, ay, 0, 0
XGTL: see RE_NOTES. Always tile 32x32, firstOff 0x30, step 2096 (CLUT8).
Overlay header (0x20 bytes): i32 count, x, y, w, h, texW, texH, pad
```

- `count` may be 0; then no TXG follows and the tex offset may be at EOF. Eyes: count 2 in 746 files, 1 in 29, 0 in 10. Mouth: 3 in 776, 2 in 1, 0 in 8.
- Overlay TXG is fmt 4 (CLUT8), swizzled. Frames are stacked vertically: frame `i` is the rect (0, h*i, w, h) in a texW x texH texture.
- `(x,y)` is the top-left of the overlay in body image pixels (Y down).
- Overlay frames are **opaque rectangular patches** (alpha 253-255 inside; `SUMSRNOM_U` measured) that contain skin/hair. Eye frame 0 = half closed, frame 1 = closed. The body image holds the open eyes and closed mouth.

### Filenames [C for structure, A for meanings of words]

`SU` + 3-letter character + 3-letter expression + `_` + costume + `.csu`. Exceptions: `SUTRJNOM.csu`, `SUSECRET.csu`, `su_test0N.csu` (debug only).

Suffixes are **costume**, not size or distance: geometry is identical across suffixes and renders show different clothes. For Mashiro: U=school uniform, UC=uniform+coat and scarf, R=orange indoor dress, S=poncho, N=pink shirt dress, P=pink halter dress, T=towel-like wrap. Others: B, M=small chibi (maid), O=large chibi. Script tables use the order `U, UC, R, S, N, P, T` per expression. The same letters appear on Live2D files (`L2DMSR_U/R/S`).

Characters (script default-sprite table) [C]: MSR ましろ, NNM 七海, MSK 美咲, JIN 仁, RNS 龍之介, OTH 乙羽, SRT 空太, CHR 千尋, SNG 品川, RIT リタ, SIR 舘林, SOR はうはう(姫宮沙織), YUK 優子, KHR 小春, DIC 宮原, KZK 藤沢和希, MID メイドちゃん, TRJ 虎次郎.

Expression codes are `NOM`, `EMI`, `MAZ`, `NEO`, `TUY`, `DYA/B`, `KNA/B`, `KYO`, `IKA/B`, ... (about 70). The port does not need their meaning: scripts pass full filenames.

---

## 3. Sprite object ("ConversationStandup") [C]

### 3.1 Structure

Slot object (0x9D300 bytes, at `scene+0x356180+i*0x9D300`) holds 2 sprite objects `A`=+0, `B`=+0x4E900 and:

| Offset | Meaning |
|---|---|
| +0x9D200 | `front` pointer (draws first, Z+0) |
| +0x9D204 | `back` pointer (draws second, Z+0.05; load target) |
| +0x9D208 | alpha state: 0 idle, 1 crossfade (dur +0x9D21C), 2 fadeIn (+0x9D228), 3 fadeOut (+0x9D234) |
| +0x9D238 | move state: 0 idle, 1 moving (target +0x9D24C, dur +0x9D25C) |
| +0x9D260 | bounce state: 0 idle, 1 bouncing (amp +0x9D274, dur +0x9D284) |
| +0x9D288 | shake state: 0 idle, 1 timed (amp +0x9D29C, dur +0x9D2AC), 2 infinite (amp +0x9D2B8) |
| +0x9D2C8 | swing state: 0 idle, 1 swinging (amp +0x9D2DC, period +0x9D2EC) |

Sprite object (0x4E900 bytes):

| Offset | Meaning |
|---|---|
| +0 | file buffer |
| +0x4B000/4/8 | ptr anchor / eye hdr / mouth hdr |
| +0x4E6C4 | filename[0x40] |
| +0x4E704 | alpha int 0-255 |
| +0x4E708 | pos vec3 |
| +0x4E718 | bounce offset vec3 |
| +0x4E728 | shake offset vec3 |
| +0x4E738 | swing offset vec3 |
| +0x4E748 | speaker name[0x20] |
| +0x4E768 | lip mode: 0 match speaker name, 1 always, other never |
| +0x4E76C | RNG state, seeded from clock (`FUN_001e5ed4`) |
| +0x4E770 | loaded flag |
| +0x4E774 | pointer to skip state; byte at +1 nonzero = skip |
| +0x4E778 | load thread |

Skip pointer is `scene+0x144F80`, set by `FUN_000606bc` [C].

RNG (`FUN_000dd6d0`): `x = x*0x41C64E6D + 0x3039; return (x>>16)&0x7FFF`.

### 3.2 Per-tick update (`FUN_0005fdb4` then `FUN_0005c1a8` for each sprite)

Order inside `FUN_0005fdb4`: sprite `front`, sprite `back`, then slot state machines alpha, move, bounce, shake, swing.
Order inside a sprite: alpha, move, bounce, shake, swing states; then if loaded and not loading, blink, then lip.

`skip` below means `skipPtr && skipPtr[1] != 0`. A skip ends any timed effect at its next tick.

```
alpha states
 hidden enter: alpha=0              shown enter: alpha=255
 fadeIn enter: alpha=0,t=0          fadeOut enter: alpha=255,t=0
 fadeIn tick:  t++; alpha=(t*255)/dur;        if skip||t>=dur -> shown
 fadeOut tick: t++; alpha=255-(t*255)/dur;    if skip||t>=dur -> hidden

move enter: start=pos; t=0
move tick:  t++; pos=lerp(start,target,float(t)/dur)
            if skip||t>=dur: pos=target; -> idle

bounce idle enter: offset=0
bounce enter: t=0
bounce tick: f=float(t+1)/dur; t++; offset=amp*(-4*f*f+4*f)
             if skip||t>=dur -> idle

shake idle enter: offset=0
shake timed enter: t=0; rng2=1
shake timed tick: t++; for x,y,z: offset=(amp*2*r)/32767.0-amp, r=next(rng2)
                  if skip||t>=dur -> idle
shake infinite enter: rng2=1; tick same, never ends by itself
                  (the 15-bit r is cast through signed int; it is never negative)

swing idle enter: offset=0
swing enter: t=0
swing tick: t=(t+1)%period; offset=amp*sin(2*PI*float(t)/period)
```

`sin` here takes radians: `FUN_001b5168` multiplies by `vcst 5` (2/π) and calls `vsin`, whose input unit is π/2, and the caller passes `2π*t/period` [C].

**Blink** (idle state draws nothing):
```
idle tick:  c++; if rng()%180==0 -> blink
blink enter: c=0; T=rng()%6+3
blink tick: c++; if c>=T*2 -> idle
blink draw: N=eye.count; if N<=0 return
  if c<T:   f=0; for i=1..N-1: if c<T*i/N break; f=i         // integer divide
  else:     f=0; for i=1..N-1: if c-T<T*i/N break; f=i
            f=N-1-f
  draw eye frame f
```

**Lip sync** (idle state draws nothing):
```
speaking() = (mode==1 || (mode==0 && name==game.currentSpeaker)) && voice.isPlaying()
idle tick: if speaking() -> talk
talk enter: c=0; frame=0
talk tick:
  if mouth.count>0:
    c++
    if rng()%20 < c:
      if frame<1: frame++
      else if frame<count-1: frame += (rng()&1)==0 ? 1 : -1
      else frame--
      c=0
  mode 1: if !voice.isPlaying() -> idle
  mode 0: if name!=speaker || !voice.isPlaying() -> idle
  other : -> idle
talk draw: if mouth.count>0 draw mouth frame `frame`
```
`game.currentSpeaker` = string at `game+0x59F984`, written by the name-window natives (`FUN_00051524`, `FUN_0005161c`, and the set-name natives around `0x663105`).
`voice.isPlaying()` = soundMgr `+0x44FC` object vtable slot `+0xA4` [C for the call; A that this object is the voice channel: its volume is set from a 0-5 option by `FUN_0000d22c`].

### 3.3 Draw (`FUN_0005fee4`, `FUN_0005e428`)

```
drawSlot: push; translate(0,0,0);    drawSprite(front); pop
          push; translate(0,0,0.05); drawSprite(back);  pop

drawSprite(s): if !loaded || loading || alpha<=0 return
  p = pos+bounce+shake+swing
  push; translate(p)
  push-free: translate(-ax, +ay, 0)        // stays until end
  color = alpha<<24 | 0xFFFFFF
  push; translate(0,0,0.01); blink.draw(); pop
  push; translate(0,0,0.01); lip.draw();   pop
  push; scale(32,32,1); drawTiles();       pop
```
Note the overlays are drawn **before** the body, at +0.01, with depth write on. The body is then rejected inside the overlay rectangle by the depth test. During a fade the patch is blended once, and the body under it is not drawn. A port must keep this: use a depth buffer with the same order, or skip body pixels inside active overlay rectangles. Drawing overlay on top of the body gives a visibly wrong result at alpha<255.

Quads (all triangle strips, 4 verts, `u,v,x,y,z` floats):
- Body tile (col,row): unit quad translated by `(col,-row)` under the ×32 scale, so 32×32 px at `(32col, -32row)`; UV 0..1. Full tile drawn even past image width.
- Overlay frame i: `x0=hdr.x, x1=hdr.x+w, y0=-hdr.y, y1=-hdr.y-h`; `u 0..w/texW`, `v h*i/texH .. (h*i+h)/texH`.

Sampling: **nearest**, clamp [C: `FUN_000e53f8(tex,..,1,1,0,0)`: wrap=1,1 filter=0,0].

Effective placement: image pixel (ax,ay) sits at `p`. Scripts use `y=-136` so that row is the screen bottom. Sprite Y is otherwise unscaled: no sprite scaling exists.

### 3.4 Load

`N161` starts an async file load into `back`. `N162` waits. `N163` joins and swaps `front`/`back`. After a swap `front` is the new sprite. Everything else (alpha state, positions) lives per sprite.

On load: only filename and data change; alpha/pos remain as they were in that object.

### 3.5 Slot alpha actions

| Action | Immediate form (ms<1) | Timed form |
|---|---|---|
| fadeIn (N164) | front shown, back hidden | back hidden; front fadeIn; idle when front shown |
| fadeOut (N165) | both hidden | back hidden; front fadeOut; idle when front hidden |
| crossfade (N166) | front shown, back hidden | **back fadeOut, front instantly shown**; idle when front shown and back hidden |

Crossfade: old sprite sits above the new one and fades out.

Slot-level move/bounce/shake/swing apply to both sprites with the same args. The slot goes idle when both sprites are idle.

### 3.6 Natives 161-178 [C]

`ms` converted as `ms>=0 ? ms*60/1000 : ms` where shown. Blocking = returns 1 (re-executes op next frame) while busy.

| # | Args | Semantics | Blocks |
|---|---|---|---|
| 161 | slot, file, speakerName | Copy name (31 chars) to back; start async load | no |
| 162 | slot | Wait for back load | yes |
| 163 | slot | Join, swap front/back | no |
| 164 | slot, ms | fadeIn | no |
| 165 | slot, ms | fadeOut | no |
| 166 | slot, ms | crossfade | no |
| 167 | slot | Wait alpha idle | yes |
| 168 | slot, x, y | Set pos on both, stop move | no |
| 169 | slot, x, y, ms | Move, linear. ms<1 acts as 168 | no |
| 170 | slot | Wait move idle | yes |
| 171 | slot, ampX, ampY, ms | Parabolic hop, peak at mid. ms<1 only sets the slot state idle: the slot idle enter is empty, so a hop already running on the sprites continues to its end | no |
| 172 | slot | Wait bounce idle | yes |
| 173 | slot, ampX, ampY, ms | Shake; ms<0 infinite; ms==0 stops | no |
| 174 | slot | Stop shake | no |
| 175 | slot | Wait shake idle (never ends while infinite) | yes |
| 176 | slot, ampX, ampY, ms | Sine swing with period ms; ms<1 stops | no |
| 177 | slot | Stop swing | no |
| 178 | slot, mode | Set lip mode on both (unused by shipped scripts; default 0) | no |

Script constants (library code inside each SSB) [C]: X centre 0, sides ±192, mail-screen sides ±110, some special ±200/196; Y -136 except special cases -56, -70, -81. Typical timings: fadeIn 500/300 ms, fadeOut 800/300 ms, crossfade 210 ms.

Slot save data = 0x260 bytes: two 0x124-byte sprite records (`FUN_0005ef68`: name 0x40, alpha, 4 vec3, state indices, effect timers, speaker name, lip mode), then +0x248 swapped flag, +0x24C..0x25C the five slot states. On restore, a nonblank name reloads the CSU synchronously.

---

## 4. LBD container [C]

Little endian. File is a chain of chunks `{u32 type; u32 size /*incl. header*/; ...}`. Loaded to `obj+0x40`. Offsets below are from the chunk start. Type 9 is always first (needed for names).

| Type | Layout | Meaning |
|---|---|---|
| 9 | +8 NUL-terminated strings | String table. "name" fields are byte offsets into it |
| 0 | +8 u32 mocSize, +0xC 0, +0x10 moc | Model, one per file |
| 1 | +8 u32 gimSize, +0xC 0, +0x10 GIM | Texture. Order = texture index |
| 2 | +8 name, +C textLen, +10 u32 loop, +14 text | Motion (max 20) |
| 3 | +8 name, +C t0, +10 t1, +14 t2, +18 t3 | Eye-close window for a motion name, in 60 Hz frames (max 20). Matched by exact string compare to the name passed to N185-187. 60 of 66 entries match a type 2 name; the other 6 (`kubiyoko_a/b` in the 3 MSR files, whose motions are `kubi_yoko_a/b`) can never match and are inert |
| 4 | +8 u32 n, +C name[n] | Expression parameter list (max 30) |
| 5 | +8 f32 eyeWeight, +C f32 value[n] | One expression. Index = order in file (max 30) |
| 6 | +8 u32 n, +C i32 id[n] | Expression ids where automatic blink is off |
| 7 | +8 name, +C f32 value | Initial parameter override, max 10. Absent from all 15 files |
| 8 | +8 u32 n, +C name[n] | Part visibility group, max 4. Names start `VISIBLE:` |
| other | | Skipped by size |

Motion names in scripts that do not exist in the file (for example `ude_ushiro` vs `ude_usiro`) are silently ignored.

### GIM (all 52 textures) [C]

Header 16 bytes `MIG.00.1PSP\0....`. Then nested blocks `{u16 id; u16 ?; u32 size; u32 nextRel; u32 dataRel}`: root 2 at 0x10, picture 3 at 0x20, image 4 at 0x30, palette 5, info 0xFF.

Image block: header at `block+dataRel`. Header `+04 fmt=5 (index8) +06 order=1 (swizzled) +08 w +0A h +0C bpp=8`, `+1C dataOff` from header start (0x40). Palette block: same header, `fmt=3 (RGBA8888) w=256`.

So pixels start at file offset 0x80, length w*h; palette 256×RGBA. Sizes 512×512 (47) or 256×256 (5). Unswizzle with the usual 16-byte × 8-line blocks.

---

## 5. moc format (Live2D 1.0.00alpha5, moc version 9) [C]

Starts `'m','o','c',ver`. **All numbers big endian.** Remainder: `readObject()` -> Model, then `u32 0x88888888`.

Primitives:
```
readNum: 7-bit big-endian varint, top bit = continue, up to 4 bytes
readInt/readFloat/readDouble: BE 4/4/8 bytes
readBit: if bitPos==0 or 8 read a byte; MSB first. Any readObject/readInt/readFloat resets bitPos=0.
intArray:   readNum n, n BE ints
floatArray: readNum n, n BE floats
```

`readObject(flags=0)`: `readNum cid`, then

| cid | Object |
|---|---|
| 0 | null |
| 1 | string: readNum len, bytes |
| 0x0F | object list: readNum n, n×readObject |
| 0x19 | intArray. With flags&1: same bytes, keep low 16 bits as u16 |
| 0x1A | double array |
| 0x1B | floatArray |
| 0x21 | reference: readInt i → `objects[i]`; not registered |
| 0x32 / 0x33 / 0x3C / 0x86 | DrawDataID / BaseDataID / ParamID / PartsDataID: readNum len, bytes; interned by string |
| 0x41 | WarpDeformer |
| 0x42 | PivotManager |
| 0x43 | ParamPivots |
| 0x44 | RotationDeformer |
| 0x45 | AffineEnt |
| 0x46 | Mesh |
| 0x83 | ParamDef |
| 0x85 | PartsData |
| 0x88 | ModelImpl |
| 0x89 | ParamDefSet |

Every non-reference object, including nulls, is appended to `objects` **after** it has finished reading (post-order). Classes 0x0A-0x17, 0x8E exist in the reader but do not occur in these files.

Class fields, in file order:

```
ModelImpl:   params=obj(ParamDefSet) parts=obj(list PartsData) canvasW=int canvasH=int
ParamDefSet: list=obj
ParamDef:    min=float max=float default=float id=obj
PartsData:   locked=bit visible=bit id=obj deformers=obj(list) meshes=obj(list)
PivotManager:list=obj(list ParamPivots)
ParamPivots: paramId=obj count=int pivots=obj(float[])
Warp:        id=obj target=obj rows=int cols=int pivotMgr=obj keys=obj(list float[])
Rotation:    id=obj target=obj pivotMgr=obj keys=obj(list AffineEnt)
AffineEnt:   originX originY scaleX scaleY rotateDeg (5 floats)
Mesh:        id=obj target=obj pivotMgr=obj drawOrder=int
             orderKeys=intArray opacityKeys=floatArray
             texNo=int pointCount=int triCount=int
             indices=obj(flags=1) keys=obj(list float[]) uv=obj(float[])
             (ver>=8) flags=int
                  if flags&1: color=int
                  blend=(flags&0x1E)>>1
```
Note Warp stores `rows` first. Field +0x14 in memory = rows, +0x10 = cols. Grid point count `(rows+1)*(cols+1)`.
`target` is `DST_BASE` for root items. Key arrays hold `pointCount*2` floats (`x,y` pairs), one per keyform. `uv` holds `pointCount*2` floats. `indices` has `triCount*3` entries.

Data seen in all 15 files: `flags` only 0 or 32 (bit 5 is ignored by the runtime), blend always 0, `color` never present, up to 3 pivot params on a mesh and 4 on a deformer. Implement blend 1/2 anyway (section 7.5).

---

## 6. Live2D runtime: evaluation [C]

### 6.1 Setup (`FUN_0017ed74`)

Walk `model.parts` in order:
- partsCtx[p] = `{opacity = visible?1:0, data}`
- Collect deformers and meshes. A mesh index is global, increasing with part order; mesh remembers partIndex.

Sort deformers topologically: repeat passes over the unplaced deformers in collected order, placing every deformer whose `target` is null, `DST_BASE`, or already placed; stop when a pass places none. Leftovers are dropped.

Then register params in file order: value=saved=prev=default, min, max.

`paramIndex(name)`: looks up by id; if unknown it **appends** a param with default 0, range -1e6..1e6. This is how `VISIBLE:` params appear. Search goes from the end of the list.

`setParam(i,v)`: if NaN v=0; clamp to [min,max]; store. `FUN_0017e180`.

- `addWeighted(name,v,w)` (`FUN_00176414`): `setParam(i, cur*(1-w)+v*w)`
- `loadParam`: value[] = saved[] (min of both lengths). `saveParam`: saved[] = value[] (extends saved).

Model update (`FUN_0017f1cc`) processes deformers in sorted order then meshes in index order; for each: `interpolate` then `setupTransform`. The real code skips interpolation when no pivot param changed since last frame; this is an optimisation only, except that it runs fresh on the first update. Always evaluating gives the same result.

### 6.2 Key selection (`FUN_00181f1c`, `FUN_00182160`)

```
EPS = 0.0001   // DAT_002247d0
for each ParamPivots pp, in list order:
   v = value[paramIndex(pp.paramId)]   // a missing index gives 0
   n = pp.count; p = pp.pivots; t = 0
   if n < 1:                index=-1                       // not in data
   elif n == 1:             index=0;  if v<=p[0]-EPS || p[0]+EPS<=v: outside=true
   elif v < p[0]-EPS:       index=0;  outside=true
   elif v < p[0]+EPS:       index=0
   else:
      for i=1..n-1:
         if v < p[i]+EPS:
            if v <= p[i]-EPS: index=i-1; t=(v-p[i-1])/(p[i]-p[i-1]); interpCount++
            else index=i
            goto done
      index=n-1; outside=true
   done: pp.index=index; pp.t=t

base=0; mul=1; dims=[]
for pp: base += pp.index*mul; if pp.t!=0: dims.push((mul,pp.t)); mul*=pp.count
keys k in 0..(1<<len(dims))-1:
   keyIndex[k] = base + sum(dims[j].mul for bit j set in k)
   weight[k]   = product(bit j set ? dims[j].t : 1-dims[j].t)
```
`outside` flags the owner as "outside": a mesh with it is not drawn; a deformer with it hides its children. Interpolation is multilinear. Results for 0-4 dims are written as nested lerps; the output is the weighted sum (float rounding aside).

Integer `orderKeys` are lerped through nested `(int)` truncation:
```
lerpI(a,b,t) = (int)((float)a + (float)(b-a)*t)
1 dim: lerpI(k0,k1,t0)
2 dim: lerpI(lerpI(k0,k1,t0), lerpI(k2,k3,t0), t1)
3,4 dim: same nesting, with t2,t3 outermost
>=5 dim: (int)(weighted float sum)
```

### 6.3 Mesh

```
interpolate:
   outside=false
   order = interpInt(orderKeys)              // sets outside
   if !outside: opacity = interpFloat(opacityKeys)
   if !outside: localPts = interp(keys)      // pairs, into a 5-float vertex at +2
setupTransform:
   if !outside and target != DST_BASE:
      parent = deformer ctx (missing -> error, keeps state)
      if parent missing or parent.outside: available=false
      else: finalPts = parent.transform(localPts); available=true
   finalPts = (has target) ? that : localPts
```
`available` starts true. Note only `parent.outside` is tested here, not `parent.ready`.

### 6.4 Warp deformer (getType 2)

```
interpolate: grid = interp(keys) with (rows+1)*(cols+1) points; outside as computed
setupTransform:
   ready=true
   if target != DST_BASE:
      if parent missing or !parent.ready or parent.outside: ready=false
      else: world = parent.transform(grid); ready=true
transform(src->dst): g = world ?: grid
   W=cols; H=rows
   for each point:
      u=clamp(src.x,0,1); v=clamp(src.y,0,1)
      x=u*W; y=v*H; ix=(int)x; iy=(int)y; fx=x-ix; fy=y-iy
      k=ix+iy*(W+1)
      A=g[k]; R=g[k+1]; D=g[k+W+1]
      if fx+fy<1: dst=A+fx*(R-A)+fy*(D-A)
      else: Q=g[k+W+2]; dst=Q+(1-fx)*(D-Q)+(1-fy)*(R-Q)
```
From raw disassembly at `0x1880a0` (no Ghidra function there). No extrapolation: coordinates **clamp**. The `k+W+1` and `k+1` reads can run past the end at u=1 or v=1, always with weight 0, so bounds-guard them in C++.

### 6.5 Rotation deformer (getType 1)

```
interpolate: a = weighted sum of AffineEnt {ox,oy,sx,sy,rot}; outside as computed
setupTransform:
   ready=true
   if target==DST_BASE: done (world absent)
   if parent missing: ready=false; done
   step = (parent.type==1) ? -10.0 : -0.1
   dir0=(0,step)
   d = directionThroughParent(a.origin, dir0)
   angle = norm(atan2(dir0.y,dir0.x) - atan2(d.y,d.x))      // to [-pi,pi], in double
   world.origin = parent.transform(a.origin)
   world.scale  = a.scale
   world.rot    = a.rot - 57.29578*angle
   ready = parent.ready && !parent.outside
transform: e = world ?: a
   r = 0.017453292*e.rot; c=cos r; s=sin r
   dst.x = c*e.sx*x - s*e.sy*y + e.ox
   dst.y = s*e.sx*x + c*e.sy*y + e.oy

directionThroughParent(o,dir):
   p0 = T(o); f=1
   loop up to 10:
      d=T(o+f*dir)-p0; if d!=0: return d
      d=T(o-f*dir)-p0; if d!=0: return d     // original keeps the sign unnegated
      f*=0.1
   prints error; output left unset (never hit in testing)
```
Note this path does **not** test `parent.outside` before using the parent; it calls transform regardless.

### 6.6 Parts

Opacity per part, default visible?1:0, addressed by `PartsDataID` string. `getPartOpacity` of an unknown part returns 0, `set` is ignored.

---

## 7. Live2D game layer (`scene+0x4A0BC0`) [C]

### 7.1 State

```
this+0x4        current motion ptr
this+0xC        current motion name[0x20]
this+0x8        current motion duration frames = (duration ms)/16, truncating
this+0x18C..    free pool, +0x1C8.. FIFO queue (8 entries of {kind, motion, frames, name})
this+0x204      MotionQueueManager
this+0x218      motion frame counter (int)
+0x220          expression (below)
+0x31C          model wrapper
+0x340          LBD parsed object
+0x177F40       k (default 0.27)
+0x177F44       pos vec3
+0x177F54       scale s (default 1)
+0x177F58       hop offset h
+0x177F5C/60/70/80  move dur/start/target/t
+0x177F84/88/98/A8/AC/B0/B4  jump dur/start/target/s0/s1/height/t
+0x177FB8       speaker name[0x20]
+0x177FD8       lip mode (N196; 0 default)
+0x177FDC       filename[0x20]
+0x177FFC       blink
+0x178014       mouth
+0x178020       tick counter
+0x178024       created flag
```

### 7.2 Tick (`FUN_00066c90`). Runs every tick but acts on every **third** one, `dt=3.0` frames

```
if !created return
if tickCounter==0:
   blinkWasIdle = (blink.state==0)
   loadParam()
   motionUpdate(dt, blink.state)
   saveParam()
   expressionUpdate(dt, canSwitch=blinkWasIdle)
   blinkUpdate(dt)
   mouthUpdate(dt)
   moveUpdate(dt); jumpUpdate(dt)
   model.update()
tickCounter=(tickCounter+1)%3
```
So motions accumulate through saved params, and expression, blink, mouth are re-applied each update on top.

### 7.3 Create (`N181`, `FUN_00066e88`)

Release previous; Live2D init; k=0.27, pos=0, s=1, h=0, all timers 0, tick=0. Parse chunks, build model, register textures in chunk order, reset motion mgr, expression reset then push `(id 0, frames 1)`, blink reset, mouth reset, apply type-7 overrides (`index>0` only), saveParam, seed blink RNG from clock.

### 7.4 Motions

**Text format** (`FUN_0017cf8c`):
- Line starting `#`: comment.
- `$fps=N`: only if key is exactly `$fps` and 5<N<121. Default 30.
- `NAME=v,v,v,...`: key must start with a letter or `_`. Separators `,` space tab. Number parser: optional `-`, digits, optional `.digits`.
- Keys starting `VISIBLE:` type 1 (whole string including prefix is the param name). Keys starting `LAYOUT:`: types 100-105, parsed and **ignored** at runtime. Others type 0.
- maxFrames = longest value list; durationMs = `(int)(maxFrames*1000/fps)`.

Each motion defaults fadeIn 1000 ms, fadeOut 100 ms, weight 1. Loop from chunk.

**Evaluate motion** (`FUN_0017c95c`), `w` = weight, `t` = clock ms, `entry.start`:
```
elapsed = max(0,t-start); ft = elapsed*fps/1000; f=(int)ft
for track:
   n=track.count
   if type==1:   setParam(id, values[min(f,n-1)])
   elif 100<=type<=105: skip
   else:
      cur=value[id]
      limit=(max[id]-min[id])*0.4
      a=values[min(f,n-1)]; b=values[min(f+1,n-1)]
      v = (abs(b-a) > limit) ? a : a+(b-a)*(ft-f)        // big jump: no interpolation
      setParam(id, cur+(v-cur)*w)
if f>=maxFrames:
   if !loop: entry.finished=true
   else: entry.start=t; entry.fadeStart=t
```

**Queue manager** (`FUN_0017fb10`, `FUN_0017fc94`, `FUN_0018553c`):
```
easing(x): x<0->0; x>1->1; else 0.5-0.5*cos(x*3.1415)

startMotion(m):
   for e in entries:  // on each, end=min(end, now+e.motion.fadeOut); unset end is replaced
      e.startFadeout()
   entries.append({motion:m, available:1, finished:0, started:0, start:-1, end:-1})

update(model): for e in entries (oldest first):
   if !e.available or e.finished: (still removed if finished below)
   t=clock
   if !started: started=1; e.start=t; e.fadeStart=t
                if e.end<0: dur=m.loop ? -1 : m.durationMs
                            e.end = dur>0 ? e.start+dur : -1
   wIn  = m.fadeIn ? easing((t-e.fadeStart)/m.fadeIn) : 1
   wOut = (m.fadeOut && e.end>=0) ? easing((e.end-t)/m.fadeOut) : 1
   evaluate(model, 1.0*wIn*wOut)
   if e.end>=0 && e.end<t: e.finished=1
   if e.finished: remove

isFinished: all entries finished (empty counts as true)
```
The duration passed to `startFadeout` is the entry's own motion fadeOut: `lw a1,8(entry); lw a2,8(a1); sra a3,a2,31` before `jal 0x18b740` [C]. Clock = `sceKernelGetSystemTime()/1000` [C], so real time. For a deterministic port use ticks×(1000/60) [A].

**Game queue** (`FUN_00064db0`, `dt`=3):
```
frameCounter = (int)(float(frameCounter)+dt)
if queueCount<1:
   if !mgr.isFinished(): mgr.update(model)
   elif cur: mgr.start(cur); frameCounter=0            // restart current
else:
   if !cur: next()
   elif cur.loop: next()
   elif mgr.isFinished(): next()
   else mgr.update(model)

next():
   h=queue.head
   if blink.state!=0 and motion name h has a type-3 entry: return     // wait
   kind 0: cur=h; cur.fadeIn=1000; cur.fadeOut=100
   kind 1: if cur: cur.fadeOut=0
           cur=h; cur.fadeIn=0; cur.fadeOut=0
   kind 2: cur=h; cur.fadeIn=1000; cur.fadeOut=0
   mgr.start(cur); frameCounter=0; pop
```
Motions keep playing via `mgr.update` only in the branches above. On a tick that takes `next()`, the manager is not updated, and params stay at `saved`.

**Not drawn until a motion exists**: `FUN_00067aa8` returns early if queue empty and `cur==null`.

### 7.5 Draw (`FUN_00067aa8`, runs every tick)

```
if !model or (queueCount<1 && !cur): return
now=clock ms
dt = (lastTime==0) ? 0.0 : (now-lastTime)/1000.0         // static lastTime, seconds
if groups>0:
   lastTime=now
   for g in groups: updateGroup(dt, g)

M = current model matrix
  * translate(pos) * scale(s,s,1) * translate(0,h,0)
  * scale(k,-k,0) * translate(-canvasW*0.5,-canvasH*0.5,0)
set blend default; filter linear,linear; tex func modulate+RGBA; wrap clamp,clamp
load gum projection, view, model
for meshes sorted by (order ascending, mesh index ascending):
   if !available or outside: continue
   alpha = opacity * partOpacity[mesh.part]
   drawMesh
```
The real sort is a bucket list by `order - globalMin`; the result equals the stable sort above.

Model space is Y-down canvas pixels. Canvas centre maps to `pos`.

drawMesh (`FUN_001884ac`):
```
blend: 0 -> SRC_ALPHA, ONE_MINUS_SRC_ALPHA
       1 -> SRC_ALPHA, FIX(0xFFFFFF)   additive
       2 -> FIX(0), SRC_COLOR          multiply
bind texture[texNo]
a = clamp((int)(alpha*255.0*1.0), 0, 255); color = a<<24 | 0xFFFFFF
draw indexed triangles, vtype T32F|V32F|I16, verts {u,v,x,y,z=0}
```
Vertices are at Z=0 model space, so all meshes share a depth and pass GEQUAL. [A] GIM bind does not alter filter/wrap: the GIM object's dirty mask (`+8`) is 0 after construction.

Visibility group (`FUN_00067148`), `names` include `VISIBLE:`; part name = name+8:
```
if dt==0:
   for name: setOpacity(part, value(name)!=0 ? 1:0)
   return
vis=-1; o=1
for i,name: if value(name)!=0:
              if vis>=0: break
              o=min(1, getOpacity(part)+dt/0.5); vis=i
if vis<0: vis=1; o=1
hide=1-o
for i,name:
   if i==vis: setOpacity(part,o)
   else:
      c=getOpacity(part)
      t = o<0.5 ? (o*-0.5)/0.5+1 : (hide*0.5)/0.5
      if 0.15 < (1-t)*hide: t=1-0.15/hide
      setOpacity(part, min(c,t))
```

### 7.6 Expressions (`FUN_00065784`)

Expression at `+0x220`: `f[0]=t(1)`, `f[1]=rate`, `f[2]=id(0)`, FIFO capacity 4 of `{id,frames}`, snapshot[30] at +0x84.

```
expressionUpdate(dt,canSwitch):
   t+=dt*rate
   if t>=1 && queueCount>0 && canSwitch:
      h=head
      if h.id!=id:
         snapshot[i]=currentValue(name[i]) for all
         t=0; id=h.id
         if h.frames<1: t=1; rate=0  else rate=1/h.frames
      pop
   if t>=1: t=1
   w0=table[id].eyeWeight
   for i,name:
      v=table[id].value[i]
      addWeighted(name, v, (name is PARAM_EYE_BALL_X or _Y) ? w0 : 1.0)
      if t<1: addWeighted(name, snapshot[i], 1-t)
```

Native 188 frames = `ms*60/1000`. Motion-driven eye direction survives when `eyeWeight`=0.

### 7.7 Blink (`FUN_00066128`)

```
blink: L,R, counter(int), state, rng, armed
reset: state=0; armed=0; L=R=1; counter=rng()%900

blinkUpdate(dt):
   if expr.t<1: return
   if expr.id in type-6 list: return
   if motionWindow(): set L,R; return
   timedBlink(dt); set L,R          // setParam EYE_L_OPEN, EYE_R_OPEN
motionWindow():
   if cur name not in type-3 table: return false
   pos=frameCounter % curFrames
   if pos<t0: armed=1
   elif !armed: return false
   L=window(target(EYE_L)); R=window(target(EYE_R))
   return true
window(v): pos<t0: v
           pos<t1: v - v*(pos-t0)/(t1-t0)
           pos<t2: 0
           pos<t3: v*(pos-t2)/(t3-t2)
           else v
target(name): expression table value; 90.0 if name not listed
timedBlink(dt):
   state 0: counter=(int)(float(counter)-dt); if counter<=0: state=1
            L=target(L); R=target(R)
   state 1: for each eye: v-=dt*0.1; if v<=0: v=0; state=2
   state 2: for each eye: v+=dt*0.1; if v>=target: v=target; state=0;
                          counter=rng()%900       // once per eye that reached target
```
`rng` is the same LCG as sprites, seeded from clock at create.

### 7.8 Mouth (`FUN_00067770`)

```
state 0 closed, 1 talking, 2 stopping; phase=90
active()= (mode==1 || (mode==0 && name==currentSpeaker)) && voice.isPlaying()
state 0: if active(): state=1
state 2: if active(): state=1
         elif phase<90 && phase+dt*7.2>=90: state=0
state 1: if !active(): state=2
then:
state 0: phase=90
else:    phase+=dt*7.2; if phase>=180: phase=0
setParam(PARAM_MOUTH_OPEN_Y, 1-sin(phase/180*PI))
```
One open/close cycle = 25 frames.

### 7.9 Move and jump

```
moveUpdate(dt): if t<dur: t=(int)(float(t)+dt)
                 if t>=dur: t=dur; pos=target else pos=lerp(start,target,float(t)/dur)
jumpUpdate(dt): if t<dur: t=(int)(float(t)+dt)
                 if t<dur: f=float(t)/dur; pos=lerp(start,target,f); s=s0*(1-f)+s1*f
                           h=height*(-4*f*f+4*f)
                 else: t=dur; pos=target; h=0; s=s1
```
Durations in frames, advanced 3 per update. A running move and jump both write `pos`; move runs first.

### 7.10 Natives 179-196 [C]

Live2D natives use the same return convention; blocking ones return 1 while busy.

| # | Args | Semantics | Blocks |
|---|---|---|---|
| 179 | speakerName, file | Store name; start async load | no |
| 180 | | Wait load | yes |
| 181 | | Create (7.3) | no |
| 182 | scaleMs | `k = arg/1000.0` (script values 300-340) | no |
| 183 | x,y,z,ms | Move. frames=ms*60/1000; <1 snaps | no |
| 184 | x,y,z,scale,height,ms | Jump. `s1=scale/1000.0`, height float | no |
| 185 | motionName | Queue kind 0 (name unknown: ignored) | no |
| 186 | motionName | Queue kind 1 (no fades; unused by scripts) | no |
| 187 | motionName | Queue kind 2 (unused by scripts) | no |
| 188 | exprId, ms | Queue expression | no |
| 189 | | Wait until the motion **queue** is empty. It does not wait for the current motion to end: `FUN_00064ee8` calls `isFinished` but discards the result and returns 1 (checked in disassembly) | yes |
| 190 | | Wait expression: `t>=1` and its queue empty | yes |
| 191 | | Wait move | yes |
| 192 | | Wait jump | yes |
| 193 | | Clear motion queue (current keeps playing) | no |
| 194 | | Clear expression queue | no |
| 195 | | Release model | no |
| 196 | mode | Lip mode: 0 name match, 1 always, other never (unused by scripts) | no |

Script library [C]: `N183(0,0,0,0); N191; N182(F(chara))`, scale table: 0→325, 1→320, 2→300, 3→340, 4→330, 5→305. Entrance `N184(0,-2,0,1000+s/50,-1,150); N192; N184(0,y-2,0,s,h,400); N192; N183(0,y,0,200); N191`. Typical y -3..-11. Expression changes use 300/1000/10000 ms.

Save data (`FUN_000666f0`, at save+0x1748): created flag, k, pos, s, h, move and jump fields, speaker name, filename, then motion queue (`FUN_00064aac`: count of pending plus current name and kind) and expression record (`FUN_00065518`). Restore reloads the file, recreates, re-queues current then pending motions and expression 0 frames.

---

## 8. Model inventory [C]

All moc version 9. Canvas 1090×1780, except JIN 1000×1000.

| File | Params | Parts | Deformers | Meshes | Points | Tris | Tex | Motions | Expr |
|---|---|---|---|---|---|---|---|---|---|
| L2DJIN_R | 34 | 15 | 59 | 62 | 2018 | 2649 | 2 | 5 | 9 |
| L2DMSK_R | 42 | 34 | 80 | 95 | 1675 | 2064 | 4 | 20 | 16 |
| L2DMSK_S | 42 | 34 | 81 | 93 | 1651 | 2033 | 4 | 20 | 16 |
| L2DMSK_U | 42 | 34 | 82 | 99 | 1846 | 2291 | 4 | 20 | 16 |
| L2DMSR_R | 32 | 32 | 78 | 69 | 1861 | 2525 | 3 | 12 | 18 |
| L2DMSR_S | 32 | 32 | 85 | 73 | 1799 | 2371 | 4 | 12 | 18 |
| L2DMSR_U | 32 | 32 | 79 | 70 | 1974 | 2694 | 3 | 12 | 18 |
| L2DNNM_R | 33 | 33 | 71 | 74 | 1849 | 2482 | 4 | 13 | 21 |
| L2DNNM_R2 | 33 | 33 | 71 | 74 | 1849 | 2482 | 4 | 13 | 21 |
| L2DNNM_S | 33 | 33 | 73 | 74 | 2167 | 3039 | 4 | 13 | 21 |
| L2DNNM_U | 33 | 33 | 72 | 77 | 1758 | 2287 | 4 | 13 | 21 |
| L2DOTH_R | 34 | 34 | 102 | 79 | 1609 | 1912 | 3 | 15 | 17 |
| L2DOTH_S | 34 | 34 | 97 | 81 | 1701 | 2024 | 3 | 15 | 17 |
| L2DOTH_U | 34 | 34 | 100 | 81 | 1655 | 1975 | 3 | 15 | 17 |
| L2DRNS_R | 33 | 21 | 64 | 80 | 1523 | 1937 | 3 | 12 | 13 |

Script character index for Live2D: ましろ U/R/S, 七海 U/R/R2/S, 美咲 U/R/S, 仁 R, 龍之介 R, 乙羽 U/S/R. L2D file `JIN` has no type-8 chunk.

Standard parameters: `PARAM_ANGLE_X/Y/Z` ±30, `PARAM_EYE_L/R_OPEN` 0..2 default 1, `PARAM_EYE_BALL_X/Y`, `PARAM_ETE_BALL_FORM` (sic), `PARAM_BROW_*`, `PARAM_MOUTH_FORM`, `PARAM_MOUTH_OPEN_Y`, `PARAM_TERE`, `PARAM_ARM_*`, `PARAM_BODY_ANGLE_X/Y/Z` ±10, `PARAM_BREATH`, `PARAM_HAIR_*`.

---

## 9. Open items

- [A] `voice.isPlaying` = voice channel (section 3.2).
- [A] GIM bind leaves filter/wrap alone (section 7.5).
- [A] Fixed 60 Hz tick.
- Comparison against live emulator frames not performed.
