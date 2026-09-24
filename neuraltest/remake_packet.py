"""Read and write remake-view.bin scene packets (v1..v5) in Python.

Mirrors the wire layout of core/rend/neural/remake_view_transport.cpp
(packet()/writePacket()). write(read(b)) reproduces b exactly for packets the
C++ writer produced; offline tools use this to make edited copies of saved
frames (never to edit retained evidence in place).
"""
import struct
from dataclasses import dataclass, field

import numpy as np

MAGIC = 0x56524346
ANCHORED = 'diagnostic-camera-embedded-anchor-not-world-reconstruction'
# 36-byte vertex record: position, normal, uv, public colour (BGRA bytes).
VERTEX = np.dtype([('pos', '<f4', 3), ('nrm', '<f4', 3), ('uv', '<f4', 2), ('col', 'u1', 4)])


@dataclass
class Mesh:
    id: int
    tsp: int
    alpha_ref: int  # 256 = none
    alpha_blend: int
    known: int
    texture: tuple  # id, generation, paletteGeneration, rttGeneration
    mode: int  # 0 carried, 1 registered, 2 referenced
    dds: bytes
    vertices: np.ndarray  # VERTEX
    indices: np.ndarray  # uint32, triangle list or strip as submitted


@dataclass
class Packet:
    version: int
    frame: int
    producer: tuple  # epoch, ordinal, cycle
    game: str
    git_sha: str
    scope: str
    fov_aspect_near_far: tuple
    pose: tuple | None  # position, right, up, forward, origin (each 3 floats)
    omissions: list
    meshes: list = field(default_factory=list)


class _Reader:
    def __init__(self, data):
        self.data, self.off = data, 0

    def take(self, n):
        if n < 0 or self.off + n > len(self.data):
            raise ValueError('truncated packet')
        out = self.data[self.off:self.off + n]
        self.off += n
        return out

    def unpack(self, fmt):
        return struct.unpack('<' + fmt, self.take(struct.calcsize('<' + fmt)))

    def string(self):
        return self.take(self.unpack('I')[0]).decode('utf-8')


def read(data: bytes) -> Packet:
    r = _Reader(data)
    magic, version = r.unpack('II')
    if magic != MAGIC or not 1 <= version <= 5:
        raise ValueError('not a remake-view packet')
    frame, *producer = r.unpack('QQQQ')
    game, git_sha, scope = r.string(), r.string(), r.string()
    camera = r.unpack('ffff')
    pose = None
    if version == 4 or (version >= 5 and scope == ANCHORED):
        pose = tuple(r.unpack('fff') for _ in range(5))
    omissions = [r.string() for _ in range(r.unpack('I')[0])]
    p = Packet(version, frame, tuple(producer), game, git_sha, scope, camera, pose, omissions)
    for _ in range(r.unpack('I')[0]):
        mesh_id, tsp = r.unpack('QI')
        alpha_ref = r.unpack('I')[0] if version >= 2 else 256
        alpha_blend = r.unpack('I')[0] if version >= 3 else 0
        known = r.unpack('I')[0]
        texture = r.unpack('QQQQ')
        mode = r.unpack('I')[0] if version >= 5 else 0
        dds = r.take(r.unpack('I')[0])
        n = r.unpack('I')[0]
        vertices = np.frombuffer(r.take(n * 36), dtype=VERTEX).copy()
        k = r.unpack('I')[0]
        indices = np.frombuffer(r.take(k * 4), dtype='<u4').copy()
        p.meshes.append(Mesh(mesh_id, tsp, alpha_ref, alpha_blend, known, texture, mode, dds, vertices, indices))
    if r.off != len(data):
        raise ValueError('trailing bytes after packet')
    return p


def required_version(p: Packet) -> int:
    version = 1
    if any(m.alpha_ref != 256 for m in p.meshes):
        version = 2
    if any(m.alpha_blend for m in p.meshes):
        version = 3
    if p.scope == ANCHORED:
        version = 4
    if any(m.mode != 0 for m in p.meshes):
        version = 5
    return version


def write(p: Packet) -> bytes:
    version = required_version(p)
    out = [struct.pack('<IIQQQQ', MAGIC, version, p.frame, *p.producer)]
    for s in (p.game, p.git_sha, p.scope):
        b = s.encode('utf-8')
        out.append(struct.pack('<I', len(b)) + b)
    out.append(struct.pack('<ffff', *p.fov_aspect_near_far))
    if version == 4 or (version >= 5 and p.scope == ANCHORED):
        for v in p.pose:
            out.append(struct.pack('<fff', *v))
    out.append(struct.pack('<I', len(p.omissions)))
    for s in p.omissions:
        b = s.encode('utf-8')
        out.append(struct.pack('<I', len(b)) + b)
    out.append(struct.pack('<I', len(p.meshes)))
    for m in p.meshes:
        out.append(struct.pack('<QI', m.id, m.tsp))
        if version >= 2:
            out.append(struct.pack('<I', m.alpha_ref))
        if version >= 3:
            out.append(struct.pack('<I', m.alpha_blend))
        out.append(struct.pack('<IQQQQ', m.known, *m.texture))
        if version >= 5:
            out.append(struct.pack('<I', m.mode))
        out.append(struct.pack('<I', len(m.dds)) + bytes(m.dds))
        v = np.ascontiguousarray(m.vertices, dtype=VERTEX)
        if not np.isfinite(v['pos']).all() or not np.isfinite(v['nrm']).all() or not np.isfinite(v['uv']).all():
            raise ValueError(f'nonfinite vertex in mesh {m.id:X}')
        out.append(struct.pack('<I', len(v)) + v.tobytes())
        i = np.ascontiguousarray(m.indices, dtype='<u4')
        out.append(struct.pack('<I', len(i)) + i.tobytes())
    return b''.join(out)


def dds_rgba(width, height, levels_rgba):
    """DDS in the exact layout the source_dds contract accepts (DX10 RGBA8, 2D)."""
    levels = len(levels_rgba)
    flags = 0x100f | (0x20000 if levels > 1 else 0)
    caps = 0x1000 | (0x400008 if levels > 1 else 0)
    header = bytearray(148)
    struct.pack_into('<4sIIIIII', header, 0, b'DDS ', 124, flags, height, width, width * 4, 0)
    struct.pack_into('<I', header, 28, levels)
    struct.pack_into('<II4s', header, 76, 32, 4, b'DX10')
    struct.pack_into('<I', header, 108, caps)
    struct.pack_into('<IIIII', header, 128, 28, 3, 0, 1, 0)
    body = b''.join(np.ascontiguousarray(level, dtype=np.uint8).tobytes() for level in levels_rgba)
    return bytes(header) + body


def dds_levels(data: bytes):
    """Decode a contract DDS back into RGBA levels (for tests and sampling)."""
    height, width = struct.unpack_from('<II', data, 12)
    levels = struct.unpack_from('<I', data, 28)[0]
    off, out, w, h = 148, [], width, height
    for _ in range(levels):
        n = w * h * 4
        out.append(np.frombuffer(data, dtype=np.uint8, count=n, offset=off).reshape(h, w, 4))
        off += n
        w, h = max(1, w // 2), max(1, h // 2)
    return out
