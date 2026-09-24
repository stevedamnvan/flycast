"""Hair mesh proof of concept (HAIR-MESH-DESIGN Phase A, D-245): offline tools.

Subcommands (run from the repo; outputs go to evidence folders, never Git):
  export  REF_FRAME_DIR --out DIR   reference-pose OBJ + textures for modelling
  cards   REF_FRAME_DIR --out FILE  procedural placeholder hair cards (.npz)
  bind    REF_FRAME_DIR --hair FILE --out FILE   bind a hair model to the pose
  apply   --binding FILE --frames DIR... --out DIR   edited packet copies

A hairstyle is found per frame by texture identity + UV box (LOG1167/1168).
Triangles are matched between frames by their UV corner set; mirrored twins
that share UVs are told apart with a rigid fit of the unique ones.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path

import numpy as np
from PIL import Image

import remake_packet as rp

# Sophitia, shrine captures. Hair = texture identity + UV box, per atlas (256x256):
# the alpha-blended outer band (LOG1167/1168) and the opaque main hair strip on
# the skin/hair/straps atlas (LOG1192). The side-hair/ear and hairline/forehead
# areas on that atlas also hold skin and stay visible.
SOPHITIA = dict(size=256, regions=[
    ('84e109e20530276bae203992478e71af4eb410498cea42d7f9df42496d8ce833', (117, 187, 179, 248)),
    ('1bd5db470822b9be', (80, 100, 160, 180)),
])


def sha(b):
    return hashlib.sha256(b).hexdigest() if b else None


def hair_triangles(p, style=SOPHITIA):
    """Hair triangles of one packet: list of dicts with mesh index, triangle
    index, key (atlas + sorted UV corners), positions/normals/uvs in key order."""
    out = []
    for mi, m in enumerate(p.meshes):
        h = sha(m.dds)
        boxes = [box for atlas, box in style['regions'] if h and h.startswith(atlas)]
        if not boxes:
            continue
        idx = m.indices.reshape(-1, 3)
        uv = m.vertices['uv'][idx]
        pix = uv * style['size']
        inside = np.zeros(len(idx), bool)
        for box in boxes:
            inside |= ((pix >= box[:2]) & (pix <= box[2:])).all(axis=(1, 2))
        for t in np.nonzero(inside)[0]:
            corners = idx[t]
            order = sorted(range(3), key=lambda k: tuple(np.round(uv[t, k], 5)))
            c = corners[order]
            out.append(dict(mesh=mi, tri=int(t),
                            key=(h[:16],) + tuple(tuple(np.round(uv[t, k], 5)) for k in order),
                            pos=m.vertices['pos'][c].astype(np.float64),
                            nrm=m.vertices['nrm'][c].astype(np.float64),
                            uv=m.vertices['uv'][c].astype(np.float64)))
    return out


def kabsch(a, b):
    """Rigid transform (R, t) minimising |R a + t - b|."""
    ca, cb = a.mean(0), b.mean(0)
    u, _, vt = np.linalg.svd((a - ca).T @ (b - cb))
    d = np.sign(np.linalg.det(vt.T @ u.T))
    r = vt.T @ np.diag([1, 1, d]) @ u.T
    return r, cb - r @ ca


def correspond(ref, cur):
    """For each reference hair triangle, the index of the same triangle in cur."""
    by_key = {}
    for i, t in enumerate(cur):
        by_key.setdefault(t['key'], []).append(i)
    ref_keys = {}
    for i, t in enumerate(ref):
        ref_keys.setdefault(t['key'], []).append(i)
    if sorted((k, len(v)) for k, v in by_key.items()) != sorted((k, len(v)) for k, v in ref_keys.items()):
        raise ValueError('hair triangle set differs from the reference')
    match = [None] * len(ref)
    unique = [k for k, v in ref_keys.items() if len(v) == 1]
    for k in unique:
        match[ref_keys[k][0]] = by_key[k][0]
    # Twins: choose the assignment whose corners meet the corners they were welded
    # to at the reference (seams with matched unique neighbours); a rigid-fit
    # position test alone swaps mirrored pieces in some poses (LOG1192).
    corners = np.array([t['pos'] for t in ref]).reshape(-1, 3)
    weld = np.unique(np.round(corners / 1e-4).astype(np.int64), axis=0, return_inverse=True)[1].ravel()
    welded = {}
    for c, w in enumerate(weld):
        welded.setdefault(w, []).append(c)
    a = np.array([ref[ref_keys[k][0]]['pos'].mean(0) for k in unique])
    b = np.array([cur[by_key[k][0]]['pos'].mean(0) for k in unique])
    r, t = kabsch(a, b)

    def seam_cost(i, j):
        cost, n = 0.0, 0
        for corner in range(3):
            for other in welded[weld[i * 3 + corner]]:
                oi, oc = divmod(other, 3)
                if oi != i and match[oi] is not None and ref[oi]['key'] not in twins:
                    cost += np.linalg.norm(cur[j]['pos'][corner] - cur[match[oi]]['pos'][oc])
                    n += 1
        if n:
            return cost / n
        return np.linalg.norm(cur[j]['pos'].mean(0) - (r @ ref[i]['pos'].mean(0) + t))

    twins = {k for k, v in ref_keys.items() if len(v) > 1}
    for k in twins:
        group, cand = ref_keys[k], list(by_key[k])
        best = min(itertools.permutations(cand, len(group)) if len(group) <= 4 else [tuple(cand)],
                   key=lambda perm: sum(seam_cost(i, j) for i, j in zip(group, perm)))
        for i, j in zip(group, best):
            match[i] = j
    return match


def decode_texture(dds):
    return Image.fromarray(np.array(rp.dds_levels(dds)[0]), 'RGBA')  # DXGI 28: RGBA8 bytes


def export(args):
    """OBJ of the character meshes around the hair at the reference frame."""
    frame = Path(args.frame)
    p = rp.read((frame / 'remake-view.bin').read_bytes())
    hair = hair_triangles(p)
    hair_set = {(h['mesh'], h['tri']) for h in hair}
    centre = np.concatenate([h['pos'] for h in hair]).mean(0)
    out = Path(args.out)
    (out / 'textures').mkdir(parents=True, exist_ok=False)
    obj, mtl = ['mtllib reference.mtl'], []
    written, vbase = set(), 1
    for mi, m in enumerate(p.meshes):
        pos = m.vertices['pos']
        if len(pos) == 0 or np.linalg.norm(pos - centre, axis=1).min() > args.radius or not m.dds:
            continue
        tex = sha(m.dds)[:16]
        if tex not in written:
            decode_texture(m.dds).save(out / 'textures' / f'{tex}.png')
            mtl += [f'newmtl t{tex}', f'map_Kd textures/{tex}.png', 'Kd 1 1 1', '']
            written.add(tex)
        for v in m.vertices:
            obj.append('v %.6f %.6f %.6f' % tuple(v['pos']))
            obj.append('vn %.6f %.6f %.6f' % tuple(v['nrm']))
            obj.append('vt %.6f %.6f' % (v['uv'][0], 1 - v['uv'][1]))
        idx = m.indices.reshape(-1, 3)
        for group, keep in (('hair', True), ('body', False)):
            faces = [t for t in range(len(idx)) if ((mi, t) in hair_set) == keep]
            if not faces:
                continue
            obj += [f'o mesh{mi:02d}_{group}', f'usemtl t{tex}']
            for t in faces:
                a, b, c = (int(i) + vbase for i in idx[t])
                obj.append(f'f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}')
        vbase += len(m.vertices)
    (out / 'reference.obj').write_text('\n'.join(obj) + '\n')
    (out / 'reference.mtl').write_text('\n'.join(mtl) + '\n')
    cam = dict(zip(['position', 'right', 'up', 'forward', 'origin'], p.pose))
    info = dict(frame=p.frame, hair_triangles=len(hair), hair_centre=centre.tolist(),
                hair_bbox=[np.concatenate([h['pos'] for h in hair]).min(0).tolist(),
                           np.concatenate([h['pos'] for h in hair]).max(0).tolist()],
                camera=cam, fov_aspect_near_far=p.fov_aspect_near_far,
                notes='Packet space of this frame (anchor-embedded). OBJ vt v is flipped (1-v). '
                      'Objects *_hair are the original hair triangles to be replaced.')
    (out / 'reference.json').write_text(json.dumps(info, indent=1))
    print(json.dumps({k: info[k] for k in ('frame', 'hair_triangles', 'hair_bbox')}))


# ---------------------------------------------------------------- hair model
# A hair model is an .npz with pos (N,3), nrm (N,3), uv (N,2) in [0,1] with v
# down (D3D convention), tri (M,3) and rgba (H,W,4) uint8 texture.

def mip_chain(rgba, threshold):
    """Box-filtered mips; alpha rescaled per level to keep the same coverage
    above the alpha-test threshold as the base level (cards stay full at distance)."""
    levels = [rgba]
    base = (rgba[..., 3] >= threshold).mean()
    cur = rgba.astype(np.float32)
    while cur.shape[0] > 1 or cur.shape[1] > 1:
        h, w = max(1, cur.shape[0] // 2), max(1, cur.shape[1] // 2)
        cur = cur[:h * 2, :w * 2].reshape(h, cur.shape[0] // h, w, cur.shape[1] // w, 4).mean(axis=(1, 3))
        level = cur.copy()
        a = level[..., 3]
        if base > 0 and a.max() > 0:
            lo, hi = 0.0, 8.0
            for _ in range(20):
                s = (lo + hi) / 2
                if (np.clip(a * s, 0, 255) >= threshold).mean() < base:
                    lo = s
                else:
                    hi = s
            level[..., 3] = np.clip(a * hi, 0, 255)
        levels.append(np.round(level).astype(np.uint8))
    return levels


def load_obj_model(obj_path, png_path):
    """Hair cards from an OBJ (standard vt, v up) + RGBA PNG."""
    vs, vts, vns, faces = [], [], [], []
    for line in Path(obj_path).read_text().splitlines():
        s = line.split()
        if not s:
            continue
        if s[0] == 'v':
            vs.append([float(x) for x in s[1:4]])
        elif s[0] == 'vt':
            vts.append([float(x) for x in s[1:3]])
        elif s[0] == 'vn':
            vns.append([float(x) for x in s[1:4]])
        elif s[0] == 'f':
            corners = [c.split('/') for c in s[1:]]
            for k in range(1, len(corners) - 1):
                faces.append([corners[0], corners[k], corners[k + 1]])
    pos, nrm, uv, tri, seen = [], [], [], [], {}
    for f in faces:
        t = []
        for c in f:
            key = tuple(c)
            if key not in seen:
                seen[key] = len(pos)
                pos.append(vs[int(c[0]) - 1])
                uv.append(vts[int(c[1]) - 1] if len(c) > 1 and c[1] else [0, 0])
                nrm.append(vns[int(c[2]) - 1] if len(c) > 2 and c[2] else [0, 1, 0])
            t.append(seen[key])
        tri.append(t)
    uv = np.array(uv, np.float64)
    uv[:, 1] = 1 - uv[:, 1]
    rgba = np.array(Image.open(png_path).convert('RGBA'))
    return dict(pos=np.array(pos), nrm=np.array(nrm), uv=uv, tri=np.array(tri, np.int64), rgba=rgba)


def strand_texture(rng, width=256, height=1024, clumps=4, colour=(214, 176, 104)):
    """Procedural strand clumps side by side (u across, v along the strand)."""
    img = np.zeros((height, width, 4), np.float32)
    cw = width // clumps
    v = np.linspace(0, 1, height)[:, None]
    for c in range(clumps):
        for _ in range(28):
            x = c * cw + rng.uniform(2, cw - 2)
            wobble = x + 1.5 * np.sin(v * rng.uniform(3, 9) + rng.uniform(0, 6))
            px = np.arange(width)[None, :]
            d = np.abs(px - wobble)
            tip = rng.uniform(0.75, 1.0)
            fade = np.clip((tip - v) / 0.25, 0, 1)
            alpha = np.clip(1.2 - d, 0, 1) * fade
            shade = rng.uniform(0.8, 1.1)
            for k in range(3):
                img[..., k] = np.maximum(img[..., k], alpha * colour[k] * shade)
            img[..., 3] = np.maximum(img[..., 3], alpha * 255)
    return np.clip(img, 0, 255).astype(np.uint8)


def procedural_cards(ref, seed=7, per_tri=6, segs=4):
    """Placeholder cards over the original hair, flowing along the texture's v axis."""
    rng = np.random.default_rng(seed)
    pos, nrm, uv, tri = [], [], [], []
    clumps = 4
    for t in ref:
        p, n_, w = t['pos'], t['nrm'], t['uv']
        e1, e2 = p[1] - p[0], p[2] - p[0]
        d1, d2 = w[1] - w[0], w[2] - w[0]
        det = d1[0] * d2[1] - d1[1] * d2[0]
        if abs(det) < 1e-12:
            continue
        dpdv = (-d2[0] * e1 + d1[0] * e2) / det  # surface direction of increasing v
        normal = np.cross(e1, e2)
        normal /= np.linalg.norm(normal) + 1e-12
        if np.dot(normal, n_.mean(0)) < 0:
            normal = -normal
        flow = dpdv - np.dot(dpdv, normal) * normal
        if np.linalg.norm(flow) < 1e-9:
            continue
        flow /= np.linalg.norm(flow)
        side = np.cross(normal, flow)
        edge = np.linalg.norm(e1)
        for _ in range(per_tri):
            a, b = rng.random(2)
            if a + b > 1:
                a, b = 1 - a, 1 - b
            root = p[0] + a * e1 + b * e2 + normal * rng.uniform(0.002, 0.012)
            length = edge * rng.uniform(1.2, 2.2)
            width = edge * rng.uniform(0.25, 0.45)
            twist = rng.normal(0, 0.25)
            f = flow * np.cos(twist) + side * np.sin(twist)
            s = np.cross(normal, f)
            c = rng.integers(clumps)
            base = len(pos)
            for k in range(segs + 1):
                q = k / segs
                centre = root + f * length * q + normal * 0.004 * q * q
                for sgn in (-0.5, 0.5):
                    pos.append(centre + s * width * sgn * (1 - 0.5 * q))
                    nrm.append(normal)
                    uv.append([(c + 0.5 + sgn) / clumps, q])
            for k in range(segs):
                i = base + 2 * k
                tri += [[i, i + 1, i + 2], [i + 1, i + 3, i + 2]]
    return dict(pos=np.array(pos), nrm=np.array(nrm), uv=np.array(uv), tri=np.array(tri, np.int64),
                rgba=strand_texture(rng))


def save_model(path, m):
    np.savez_compressed(path, **m)


def load_model(path):
    z = np.load(path)
    return {k: z[k] for k in z.files}


# ---------------------------------------------------------------- binding

def frame_basis(p):
    """Per-triangle origin and orthonormal basis (e1, e2, n), rows."""
    a, b, c = p[:, 0], p[:, 1], p[:, 2]
    e1 = b - a
    e1 /= np.linalg.norm(e1, axis=1, keepdims=True) + 1e-12
    n = np.cross(b - a, c - a)
    n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-12
    e2 = np.cross(n, e1)
    return a, np.stack([e1, e2, n], axis=1)


def closest_on_triangles(q, tris, k=1):
    """For points q (N,3) and triangles (T,3,3): the k nearest triangles, with
    barycentric coordinates and distance of the closest point on each
    (Ericson region test, vectorised over triangles). Arrays (N,), (N,3), (N,)
    for k=1, else (N,k), (N,k,3), (N,k), nearest first."""
    n = len(q)
    out_i, out_b, out_d = np.zeros((n, k), int), np.zeros((n, k, 3)), np.zeros((n, k))
    a, b, c = tris[:, 0], tris[:, 1], tris[:, 2]
    ab, ac = b - a, c - a
    for i0 in range(0, n, 256):
        qq = q[i0:i0 + 256, None, :]
        ap = qq - a
        d1, d2 = (ab * ap).sum(-1), (ac * ap).sum(-1)
        bp = qq - b
        d3, d4 = (ab * bp).sum(-1), (ac * bp).sum(-1)
        cp = qq - c
        d5, d6 = (ab * cp).sum(-1), (ac * cp).sum(-1)
        va = d3 * d6 - d5 * d4
        vb = d5 * d2 - d1 * d6
        vc = d1 * d4 - d3 * d2
        denom = va + vb + vc
        safe = np.where(denom == 0, 1, denom)
        v = np.where(np.abs(denom) > 1e-18, vb / safe, 0)
        w = np.where(np.abs(denom) > 1e-18, vc / safe, 0)
        bary = np.stack([1 - v - w, v, w], -1)
        # inside: the projection; outside: the nearest point on one of the three edges
        cands = [bary]
        for (p0, p1, i, j) in ((a, b, 0, 1), (a, c, 0, 2), (b, c, 1, 2)):
            e = p1 - p0
            t = np.clip(((qq - p0) * e).sum(-1) / ((e * e).sum(-1) + 1e-18), 0, 1)
            bb = np.zeros(bary.shape)
            bb[..., i] = 1 - t
            bb[..., j] = t
            cands.append(bb)
        inside = (bary >= 0).all(-1)
        dist, bb = None, None
        for m, cand in enumerate(cands):
            pt = cand[..., 0:1] * a + cand[..., 1:2] * b + cand[..., 2:3] * c
            dd = np.linalg.norm(qq - pt, axis=-1)
            if m == 0:
                dd = np.where(inside, dd, np.inf)
            if dist is None:
                dist, bb = dd, cand
            else:
                better = dd < dist
                dist, bb = np.where(better, dd, dist), np.where(better[..., None], cand, bb)
        order = np.argsort(dist, axis=1)[:, :k]
        rows = np.arange(len(order))[:, None]
        out_i[i0:i0 + 256] = order
        out_b[i0:i0 + 256] = bb[rows, order]
        out_d[i0:i0 + 256] = dist[rows, order]
    if k == 1:
        return out_i[:, 0], out_b[:, 0], out_d[:, 0]
    return out_i, out_b, out_d


def make_binding(pos, nrm, tris, k=4, far=0.08, d0=0.01):
    """Bind points to the reference hair: each point stores its closest point
    and offset (in the triangle local frame) on its k nearest triangles, with
    weights 1/(d+d0)^2. Points farther than `far` follow the rigid hair fit."""
    k = max(1, min(k, len(tris)))
    idx, bary, dist = closest_on_triangles(pos, tris, k=max(k, 2))
    idx, bary, dist = idx[:, :k], bary[:, :k], dist[:, :k]
    _, basis = frame_basis(tris)
    anchor = (bary[..., None] * tris[idx]).sum(2)
    offset = np.einsum('nkij,nkj->nki', basis[idx], pos[:, None, :] - anchor)
    local_n = np.einsum('nkij,nj->nki', basis[idx], nrm)
    weight = 1 / (dist + d0) ** 2
    weight /= weight.sum(1, keepdims=True)
    return dict(tri_index=idx, bary=bary, offset=offset, local_n=local_n, weight=weight,
                far=dist[:, 0] > far, dist=dist[:, 0], pos=pos, nrm=nrm)


def bind(args):
    frame = Path(args.frame)
    p = rp.read((frame / 'remake-view.bin').read_bytes())
    ref = hair_triangles(p)
    tris = np.array([t['pos'] for t in ref])
    if args.hair.endswith('.obj'):
        model = load_obj_model(args.hair, args.texture)
    elif args.hair == 'procedural':
        model = procedural_cards(ref)
    else:
        model = load_model(args.hair)
    b = make_binding(model['pos'], model['nrm'], tris, k=args.k, far=args.far)
    np.savez_compressed(args.out, ref_keys=np.array([repr(t['key']) for t in ref]), ref_pos=tris,
                        uv=model['uv'], tri=model['tri'], rgba=model['rgba'], frame=p.frame, **b)
    print(json.dumps(dict(vertices=len(model['pos']), triangles=len(model['tri']),
                          texture=list(model['rgba'].shape), far_vertices=int(b['far'].sum()),
                          distance_median=float(np.median(b['dist'])), distance_max=float(b['dist'].max()))))


# ---------------------------------------------------------------- apply

def deform(b, cur_tris, rigid):
    idx, w = b['tri_index'], b['weight'][..., None]
    _, basis = frame_basis(cur_tris)
    anchor = (b['bary'][..., None] * cur_tris[idx]).sum(2)
    pos = ((anchor + np.einsum('nkji,nkj->nki', basis[idx], b['offset'])) * w).sum(1)
    nrm = (np.einsum('nkji,nkj->nki', basis[idx], b['local_n']) * w).sum(1)
    if b['far'].any():
        r, t = rigid
        pos[b['far']] = b['pos'][b['far']] @ r.T + t
        nrm[b['far']] = b['nrm'][b['far']] @ r.T
    nrm /= np.linalg.norm(nrm, axis=1, keepdims=True) + 1e-12
    return pos, nrm


def edited_packet(p, b, dds, alpha_ref, keep_original=False, hide_only=False, style=None):
    cur = hair_triangles(p, style or SOPHITIA)
    ref = [dict(key=eval(k), pos=pos) for k, pos in zip(b['ref_keys'], b['ref_pos'])]
    match = correspond(ref, cur)
    cur_tris = np.array([cur[j]['pos'] for j in match])
    rigid = kabsch(b['ref_pos'].reshape(-1, 3), cur_tris.reshape(-1, 3))
    pos, nrm = deform(b, cur_tris, rigid)
    template = p.meshes[cur[0]['mesh']]
    if not keep_original:
        drop = {}
        for t in cur:
            drop.setdefault(t['mesh'], set()).add(t['tri'])
        meshes = []
        for mi, m in enumerate(p.meshes):
            if mi in drop:
                idx = m.indices.reshape(-1, 3)
                keep = np.array([t not in drop[mi] for t in range(len(idx))])
                if not keep.any():
                    continue
                m = rp.Mesh(**{**m.__dict__, 'indices': idx[keep].ravel()})
            meshes.append(m)
        p.meshes = meshes
    if not hide_only:
        v = np.zeros(len(pos), rp.VERTEX)
        v['pos'], v['nrm'], v['uv'] = pos, nrm, b['uv']
        v['col'] = 255
        p.meshes.append(rp.Mesh(id=0x7F0000A1, tsp=template.tsp, alpha_ref=alpha_ref, alpha_blend=0,
                                known=0, texture=(0, 0, 0, 0), mode=0, dds=dds, vertices=v,
                                indices=b['tri'].astype('<u4').ravel()))
    return p


def apply(args):
    b = dict(np.load(args.binding))
    dds = rp.dds_rgba(b['rgba'].shape[1], b['rgba'].shape[0], mip_chain(b['rgba'], args.alpha_ref))
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=False)
    frames = sorted(Path(args.frames).glob('frame-*'))[args.start:args.stop:args.step]
    for d in frames:
        p = rp.read((d / 'remake-view.bin').read_bytes())
        p = edited_packet(p, b, dds, args.alpha_ref, hide_only=args.hide_only)
        o = out / d.name
        o.mkdir()
        (o / 'remake-view.bin').write_bytes(rp.write(p))
        (o / 'source.txt').write_text(str(d) + '\n')
    print(f'{len(frames)} edited frames -> {out}')


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    sub = ap.add_subparsers(dest='cmd', required=True)
    e = sub.add_parser('export')
    e.add_argument('frame')
    e.add_argument('--out', required=True)
    e.add_argument('--radius', type=float, default=0.6, help='Mesh distance from the hair centre')
    e.set_defaults(fn=export)
    c = sub.add_parser('bind')
    c.add_argument('frame')
    c.add_argument('--hair', required=True, help="'procedural', a model .npz, or an .obj (with --texture)")
    c.add_argument('--texture', help='RGBA PNG for an .obj model')
    c.add_argument('--far', type=float, default=0.08, help='Beyond this distance a vertex follows the rigid hair fit')
    c.add_argument('--k', type=int, default=4, help='Nearest original hair triangles blended per vertex')
    c.add_argument('--out', required=True)
    c.set_defaults(fn=bind)
    a_ = sub.add_parser('apply')
    a_.add_argument('--binding', required=True)
    a_.add_argument('--frames', required=True, help='Captures folder with frame-* subfolders')
    a_.add_argument('--out', required=True)
    a_.add_argument('--start', type=int, default=0)
    a_.add_argument('--stop', type=int, default=None)
    a_.add_argument('--step', type=int, default=1)
    a_.add_argument('--alpha-ref', type=int, default=96)
    a_.add_argument('--hide-only', action='store_true', help='Remove the original hair, add nothing (control)')
    a_.set_defaults(fn=apply)
    a = ap.parse_args()
    a.fn(a)


if __name__ == '__main__':
    main()
