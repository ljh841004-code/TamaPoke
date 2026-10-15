#!/usr/bin/env python3
"""glTF/GLB con piel (Draco + WebP) -> .m3d (formato simple para el renderizador por software de pc/m3d.cpp)
  python3 pc/tools/glb_to_m3d.py 7.glb 007.m3d
.m3d (little endian):
  'TPM3' u32 ver=1  u16 nJoints u16 nPrims  f32 rootMatrix[16] (nodo de la malla, columna mayor)
  joints: i16 parent, char name[22], f32 t[3] r[4](xyzw) s[3], f32 invBind[16]
  prims : u32 nV, u32 nI, u16 texW, u16 texH,
          nV x (f32 pos[3], f32 nrm[3], f32 uv[2], u8 joint[4], f32 w[4]),  nI x u32,  texW*texH*4 RGBA
"""
import io, json, struct, sys
import numpy as np
import DracoPy
from PIL import Image

src, out = sys.argv[1], sys.argv[2]
d = open(src, 'rb').read()
L = struct.unpack('<I', d[12:16])[0]
j = json.loads(d[20:20 + L])
blen = struct.unpack('<I', d[20 + L:24 + L])[0]
B = d[28 + L:28 + L + blen]

def view(i):
    bv = j['bufferViews'][i]
    o = bv.get('byteOffset', 0)
    return B[o:o + bv['byteLength']]

def accessor(i):
    a = j['accessors'][i]
    comp = {5126: 'f4', 5125: 'u4', 5123: 'u2', 5121: 'u1'}[a['componentType']]
    n = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}[a['type']]
    raw = view(a['bufferView'])[a.get('byteOffset', 0):]
    return np.frombuffer(raw, dtype='<' + comp, count=a['count'] * n).reshape(a['count'], n)

nodes = j['nodes']
parent = {}
for i, nd in enumerate(nodes):
    for c in nd.get('children', []):
        parent[c] = i
skin = j['skins'][0]
joints = skin['joints']
jidx = {n: k for k, n in enumerate(joints)}
ibm = accessor(skin['inverseBindMatrices']).astype(np.float32)

def local(nd):
    t = nd.get('translation', [0, 0, 0]); r = nd.get('rotation', [0, 0, 0, 1]); s = nd.get('scale', [1, 1, 1])
    return t, r, s

def mat_of(nd):
    if 'matrix' in nd:
        return np.array(nd['matrix'], dtype=np.float64).reshape(4, 4).T
    t, r, s = local(nd)
    x, y, z, w = r
    R = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    M = np.eye(4); M[:3, :3] = R * np.array(s); M[:3, 3] = t
    return M

def world(i):
    M = mat_of(nodes[i])
    while i in parent:
        i = parent[i]
        M = mat_of(nodes[i]) @ M
    return M

# raiz del esqueleto: transformacion de los padres que no son huesos (se aplica al hueso raiz)
mesh_node = next(i for i, nd in enumerate(nodes) if 'mesh' in nd)
root = np.eye(4)
r0 = next(n for n in joints if parent.get(n, -1) not in jidx)  # hueso raiz
if r0 in parent:
    root = world(parent[r0])

prims = []
for p in j['meshes'][nodes[mesh_node]['mesh']]['primitives']:
    ext = p.get('extensions', {}).get('KHR_draco_mesh_compression')
    if ext:
        m = DracoPy.decode(view(ext['bufferView']))
        A = {a['unique_id']: np.asarray(a['data']) for a in m.attributes}
        at = ext['attributes']
        pos = A[at['POSITION']].astype(np.float32); nrm = A[at['NORMAL']].astype(np.float32)
        uv = A[at['TEXCOORD_0']].astype(np.float32)
        jn = A[at['JOINTS_0']].astype(np.int64); wt = A[at['WEIGHTS_0']].astype(np.float32)
        idx = np.asarray(m.faces, dtype=np.uint32).reshape(-1)
    else:
        at = p['attributes']
        pos = accessor(at['POSITION']).astype(np.float32); nrm = accessor(at['NORMAL']).astype(np.float32)
        uv = accessor(at['TEXCOORD_0']).astype(np.float32)
        jn = accessor(at['JOINTS_0']).astype(np.int64); wt = accessor(at['WEIGHTS_0']).astype(np.float32)
        idx = accessor(p['indices']).astype(np.uint32).reshape(-1)
    if wt.max() > 1.5:
        wt = wt / (255.0 if wt.max() <= 255 else 65535.0)
    s = wt.sum(1, keepdims=True); s[s == 0] = 1; wt = wt / s
    mat = j['materials'][p.get('material', 0)]
    tex = mat.get('pbrMetallicRoughness', {}).get('baseColorTexture')
    img = Image.new('RGBA', (4, 4), (200, 200, 200, 255))
    if tex is not None:
        t = j['textures'][tex['index']]
        src_i = t.get('source')
        if src_i is None:
            src_i = t.get('extensions', {}).get('EXT_texture_webp', {}).get('source')
        im = j['images'][src_i]
        img = Image.open(io.BytesIO(view(im['bufferView']))).convert('RGBA')
    prims.append((pos, nrm, uv, jn, wt, idx, img))
    print('prim', len(pos), 'verts', len(idx) // 3, 'tris', img.size, 'tex')

with open(out, 'wb') as f:
    f.write(b'TPM3' + struct.pack('<IHH', 1, len(joints), len(prims)))
    f.write(struct.pack('<16f', *root.T.reshape(-1)))
    for k, n in enumerate(joints):
        par = jidx.get(parent.get(n, -1), -1)
        t, r, s = local(nodes[n])
        nm = nodes[n].get('name', '')[:21].encode('ascii', 'replace').ljust(22, b'\0')
        f.write(struct.pack('<h', par) + nm + struct.pack('<10f', *t, *r, *s) + struct.pack('<16f', *ibm[k]))
    for pos, nrm, uv, jn, wt, idx, img in prims:
        f.write(struct.pack('<IIHH', len(pos), len(idx), img.size[0], img.size[1]))
        for v in range(len(pos)):
            f.write(struct.pack('<8f', *pos[v], *nrm[v], *uv[v]) + bytes(int(x) for x in jn[v]) + struct.pack('<4f', *wt[v]))
        f.write(idx.astype('<u4').tobytes())
        f.write(img.tobytes())
print(out, 'joints', len(joints), [nodes[n].get('name') for n in joints][:6], '...')
