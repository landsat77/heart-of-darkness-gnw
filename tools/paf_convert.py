#!/usr/bin/env python3
"""Convert Heart of Darkness's cutscenes (hod.paf) for the Game & Watch.

The game's PAF player keeps four 64 KB pages (each frame is built from
blocks of the previous ones), plus the demux buffers: about 330 KB, more
than is free while a level is loaded, and most cutscenes play inside a
level. This tool plays the PAF files on the PC (same decoder as
src/hode/paf.cpp) and writes hod.gwp, which the console plays with one
256 x 192 picture (the engine's shadow layer, free during a cutscene) and
a small sound ring (src/port/hd_cutscene.cpp).

hod.gwp (little endian):
  "HGWP", u32 version (1), u32 count (50), count x u32 offset (0: none)
  per cutscene, at its offset:
    u32 frame duration (ms), u32 frame count, u32 sound samples, u32 0
    per frame:
      u32 size of what follows
      u8 flags: 1 = palette (768 bytes, 6-bit, as PAF), 2 = picture changed
      [palette]
      u16 n, n x s16 sound samples (22050 Hz mono)
      [picture ops, if flag 2] until the end of the frame:
        varint skip, varint literal count, literal bytes ... (bytes of
        the 256 x 192 picture, from the previous frame's)

Sound: PAF sound is a stream of 100 ms stereo strides decoded as the
blocks arrive, played from the first frame on. Here it is mixed to mono
and cut into the frames: frame i carries the samples up to the time of
frame i + 1, plus kLeadMs in advance (the console's ring buffer).
"""
import argparse
import os
import struct
import sys
import time

SIGNATURE = b"Packed Animation File V1.0\n(c) 1992-96 Amazing Studio\n"
MAX_VIDEOS = 50
BLOCK = 2048
PAGE = 256 * 256
W, H = 256, 192
STRIDE = 4922  # 256 x s16 + 2205 x 2 bytes
STRIDE_SAMPLES = 2205
RATE = 22050
LEAD_MS = 150

UPDATE_SEQUENCES = [
    b"", b"\x02", b"\x05\x07", b"\x05", b"\x06", b"\x05\x07\x05\x07", b"\x05\x07\x05",
    b"\x05\x07\x06", b"\x05\x05", b"\x03", b"\x06\x06", b"\x02\x04", b"\x02\x04\x05\x07",
    b"\x02\x04\x05", b"\x02\x04\x06", b"\x02\x04\x05\x07\x05\x07",
]


def u16(b, p):
    return b[p] | b[p + 1] << 8


def u32(b, p):
    return b[p] | b[p + 1] << 8 | b[p + 2] << 16 | b[p + 3] << 24


class Decoder:
    """PafPlayer::decodeVideoFrame(): the four pages are one block (reads
    may run past the end of a page into the next one, as in the C code)."""

    def __init__(self):
        self.mem = bytearray(PAGE * 4 + 256 * 4)
        self.palette = bytearray(768)
        self.palette_changed = True
        self.page = 0

    def page_offset(self, v):
        x = v & 0x7F
        y = (v >> 7) & 0x7F
        return (v >> 14) * PAGE + (y * 256 + x) * 2

    def frame(self, src, p):
        mem = self.mem
        base = p
        code = src[p]
        p += 1
        if code & 0x20:
            mem[0:PAGE * 4] = bytes(PAGE * 4)
            self.palette[:] = bytes(768)
            self.palette_changed = True
            self.page = 0
        if code & 0x40:
            index = src[p]
            count = (src[p + 1] + 1) * 3
            p += 2
            self.palette[index * 3:index * 3 + count] = src[p:p + count]
            self.palette_changed = True
            p += count
        op = code & 0xF
        cur = self.page * PAGE
        if op == 0:
            self.op0(src, p, base, code, cur)
        elif op == 1:
            mem[cur:cur + W * H] = src[p + 2:p + 2 + W * H]
        elif op == 2:
            page = src[p]
            if page != self.page:
                mem[cur:cur + W * H] = mem[page * PAGE:page * PAGE + W * H]
        elif op == 4:
            p += 2
            d = cur
            end = cur + W * H
            while d < end:
                c = src[p]
                p += 1
                if c >= 128:
                    n = 1 - (c - 256)
                    mem[d:d + n] = bytes((src[p],)) * n
                    p += 1
                else:
                    n = c + 1
                    mem[d:d + n] = src[p:p + n]
                    p += n
                d += n

    def op0(self, src, p, base, code, cur):
        mem = self.mem
        po = self.page_offset
        count = src[p]
        p += 1
        if count:
            if code & 0x10:
                align = (p - base) & 3
                if align:
                    p += 4 - align
            for _ in range(count):
                d = po(src[p] << 8 | src[p + 1])
                offset = (src[p + 1] & 0x7F) * 2
                end = u16(src, p + 2) + offset
                p += 4
                while True:
                    offset += 1
                    mem[d:d + 4] = src[p:p + 4]
                    mem[d + 256:d + 260] = src[p + 4:p + 8]
                    mem[d + 512:d + 516] = src[p + 8:p + 12]
                    mem[d + 768:d + 772] = src[p + 12:p + 16]
                    p += 16
                    if (offset & 0x3F) == 0:
                        d += 256 * 3
                    d += 4
                    if offset >= end:
                        break
        d = cur
        for y in range(0, H, 4):
            for x in range(0, W, 4):
                s = po(src[p] << 8 | src[p + 1])
                p += 2
                mem[d:d + 4] = mem[s:s + 4]
                mem[d + 256:d + 260] = mem[s + 256:s + 260]
                mem[d + 512:d + 516] = mem[s + 512:s + 516]
                mem[d + 768:d + 772] = mem[s + 768:s + 772]
                d += 4
            d += 256 * 3
        opcodes_size = u16(src, p)
        p += 4
        ops = p
        p += opcodes_size
        mask = color = 0
        s2 = 0
        d = cur
        for y in range(0, H, 4):
            for x in range(0, W, 4):
                if x & 4:
                    seq = UPDATE_SEQUENCES[src[ops] & 15]
                    ops += 1
                else:
                    seq = UPDATE_SEQUENCES[src[ops] >> 4]
                for c in seq:
                    offset = 512
                    if c <= 4:
                        if c == 2:
                            offset = 0
                        if c <= 3:
                            color = src[p]
                            p += 1
                        mask = src[p]
                        p += 1
                        q = d + offset
                        if mask & 0x80: mem[q] = color
                        if mask & 0x40: mem[q + 1] = color
                        if mask & 0x20: mem[q + 2] = color
                        if mask & 0x10: mem[q + 3] = color
                        q += 256
                        if mask & 0x08: mem[q] = color
                        if mask & 0x04: mem[q + 1] = color
                        if mask & 0x02: mem[q + 2] = color
                        if mask & 0x01: mem[q + 3] = color
                    else:
                        if c == 5:
                            offset = 0
                        if c <= 6:
                            s2 = po(src[p] << 8 | src[p + 1])
                            p += 2
                        mask = src[p]
                        p += 1
                        q = d + offset
                        r = s2 + offset
                        if mask & 0x80: mem[q] = mem[r]
                        if mask & 0x40: mem[q + 1] = mem[r + 1]
                        if mask & 0x20: mem[q + 2] = mem[r + 2]
                        if mask & 0x10: mem[q + 3] = mem[r + 3]
                        q += 256
                        r += 256
                        if mask & 0x08: mem[q] = mem[r]
                        if mask & 0x04: mem[q + 1] = mem[r + 1]
                        if mask & 0x02: mem[q + 2] = mem[r + 2]
                        if mask & 0x01: mem[q + 3] = mem[r + 3]
                d += 4
            d += 256 * 3

    def picture(self):
        cur = self.page * PAGE
        return bytes(self.mem[cur:cur + W * H])


def read_table(f, count):
    data = f.read(count * 4)
    align = (count * 4) & 0x7FF
    if align:
        f.seek(0x800 - align, 1)
    return list(struct.unpack(f"<{count}I", data))


def stride_mono(s):
    lut = struct.unpack("<256h", s[:512])
    idx = s[512:512 + STRIDE_SAMPLES * 2]
    return [(lut[idx[2 * i]] + lut[idx[2 * i + 1]]) >> 1 for i in range(STRIDE_SAMPLES)]


def play(f, offset):
    """One cutscene: yields (picture, palette or None, frame_ms) per frame
    and collects its sound; returns the sound samples at the end."""
    f.seek(offset)
    hdr = f.read(BLOCK)
    if not hdr.startswith(SIGNATURE):
        return None
    frame_ms = u32(hdr, 0x88)
    start = u32(hdr, 0xA4)
    preload = u32(hdr, 0x9C)
    assert u32(hdr, 0x98) == BLOCK
    frames = u32(hdr, 0x84)
    max_video = u32(hdr, 0xA8)
    max_audio = u32(hdr, 0xAC)
    blocks = u32(hdr, 0xA0)
    if frames <= 0 or blocks <= 0:
        return None
    blocks_count = read_table(f, frames)
    frames_offset = read_table(f, frames)
    blocks_offset = read_table(f, blocks)
    return dict(frame_ms=frame_ms, start=start, preload=preload, frames=frames, max_video=max_video,
                max_audio=max_audio, blocks_count=blocks_count, frames_offset=frames_offset,
                blocks_offset=blocks_offset)


def frames_of(f, offset, hdr, sound):
    """PafPlayer::mainLoop(): the demux and decoding of every frame."""
    f.seek(offset + hdr["start"])
    video = bytearray(hdr["max_video"] * BLOCK)
    audio = bytearray(hdr["max_audio"] * BLOCK)
    flush = (hdr["max_audio"] - 1) * BLOCK if hdr["max_audio"] else 0
    rd = wr = 0
    dec = Decoder()
    block = 0
    pending = hdr["preload"]
    for i in range(hdr["frames"]):
        pending += hdr["blocks_count"][i]
        while pending:
            data = f.read(BLOCK)
            if len(data) < BLOCK:
                data += bytes(BLOCK - len(data))
            dst = hdr["blocks_offset"][block] & 0x7FFFFFFF
            if hdr["blocks_offset"][block] & 0x80000000:
                audio[dst:dst + BLOCK] = data
                if dst != wr:
                    rd = wr = 0  # (flush earlier than expected)
                wr = dst + BLOCK
                n = (wr - rd) // STRIDE
                for k in range(n):
                    sound.extend(stride_mono(audio[rd + k * STRIDE:rd + (k + 1) * STRIDE]))
                rd += n * STRIDE
                if wr == flush:
                    rd = wr = 0
            else:
                video[dst:dst + BLOCK] = data
            block += 1
            pending -= 1
        dec.frame(video, hdr["frames_offset"][i])
        pal = None
        if dec.palette_changed:
            dec.palette_changed = False
            pal = bytes(dec.palette)
        yield dec.picture(), pal
        dec.page = (dec.page + 1) & 3


def varint(n):
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        if n:
            out.append(b | 0x80)
        else:
            out.append(b)
            return bytes(out)


def delta(prev, cur):
    """Skip/literal runs of cur against prev (skips shorter than 8 bytes
    are kept in the literals)."""
    out = bytearray()
    n = len(cur)
    i = 0
    skip = 0
    while i < n:
        # equal bytes, 16 at a time then one at a time
        j = i
        while j + 16 <= n and prev[j:j + 16] == cur[j:j + 16]:
            j += 16
        while j < n and prev[j] == cur[j]:
            j += 1
        skip += j - i
        i = j
        if i >= n:
            break
        # literal run, until 8 equal bytes
        j = i
        while j < n:
            if prev[j:j + 8] == cur[j:j + 8] and j + 8 <= n:
                break
            j += 1
        out += varint(skip) + varint(j - i) + cur[i:j]
        skip = 0
        i = j
    return bytes(out)


def convert(paf_path, out_path, log=print):
    size = os.path.getsize(paf_path)
    t0 = time.time()
    with open(paf_path, "rb") as f, open(out_path, "wb") as out:
        offsets = struct.unpack(f"<{MAX_VIDEOS}I", f.read(MAX_VIDEOS * 4))
        out.write(b"HGWP" + struct.pack("<II", 1, MAX_VIDEOS) + bytes(4 * MAX_VIDEOS))
        table = [0] * MAX_VIDEOS
        for num, offset in enumerate(offsets):
            if offset == 0 or offset + BLOCK > size:
                continue
            hdr = play(f, offset)
            if not hdr:
                continue
            frame_ms = hdr["frame_ms"]
            # the sound of the whole cutscene first (it is decoded ahead of
            # the frames in the PAF file anyway)
            sound = []
            pictures = []
            for pic, pal in frames_of(f, offset, hdr, sound):
                pictures.append((pic, pal))
            table[num] = out.tell()
            frames = hdr["frames"]
            out.write(struct.pack("<IIII", frame_ms, frames, len(sound), 0))
            prev = bytes(W * H)
            sent = 0
            lead = LEAD_MS * RATE // 1000
            for i, (pic, pal) in enumerate(pictures):
                upto = min(len(sound), (i + 1) * frame_ms * RATE // 1000 + lead)
                if i == frames - 1:
                    upto = len(sound)
                samples = sound[sent:upto]
                sent = max(sent, upto)
                flags = 0
                body = bytearray()
                if pal is not None:
                    flags |= 1
                    body += pal
                body += struct.pack("<H", len(samples)) + struct.pack(f"<{len(samples)}h", *samples)
                if pic != prev:
                    flags |= 2
                    body += delta(prev, pic)
                    prev = pic
                out.write(struct.pack("<IB", len(body) + 1, flags) + body)
            log(f"cutscene {num:2}: {frames} frames, {frame_ms} ms, {len(sound) / RATE:.1f} s of sound, "
                f"{(out.tell() - table[num]) / 1e6:.1f} MB ({time.time() - t0:.0f} s)")
        out.seek(12)
        out.write(struct.pack(f"<{MAX_VIDEOS}I", *table))
        total = out.seek(0, 2)
    log(f"hod.gwp: {total / 1e6:.1f} MB")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("paf", help="hod.paf (from the CD's paf folder)")
    ap.add_argument("out", help="output file (hod.gwp, beside the packs)")
    args = ap.parse_args()
    convert(args.paf, args.out)


if __name__ == "__main__":
    main()
