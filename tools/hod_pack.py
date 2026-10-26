#!/usr/bin/env python3
"""Make the Heart of Darkness packs for the Game & Watch.

Input: the game's data files, as installed from the Windows CD (or
extracted from its installer HeartOfDarkness.exe with 7-Zip):
SETUP.DAT and, per level, XXXX_HOD.LVL / .SSS / .MST.

Output (in the output folder, one pack per level so that only the setup
and the level being played need room in the external flash):
  setup.hod                SETUP.DAT, HOD.DEM
  rock.hod ... dark.hod    the level's LVL, SSS and MST
  hod.gwp                  the cutscenes, from the CD's paf/hod.paf
                           (tools/paf_convert.py; --paf, or found in the
                           data folder or its paf folder)

The files of the v1.2 data (all known Windows releases but the first) are
"sector aligned": 2048-byte sectors, each 2044 bytes of data and a 4-byte
checksum. The packs keep only the data (the checksums are checked here), so
the game reads everything in place; src/hode/fileio.cpp (GNW) maps the
engine's sector offsets onto it.

Pack format (little endian):
  "HODP", u32 version (1), u32 file count, u32 total size
  count x { char name[16], u32 offset, u32 size, u32 flags, u32 0 }
  data, each file 16-byte aligned
flags: 1 = desectorized; on SETUP.DAT also the game's data version and
kind, so that the engine does not open the level packs to find out:
  2 = v1.2 (sector aligned), 4 = v1.0, 8 = demo (no second level)
"""
import argparse
import os
import struct
import sys

LEVELS = ["rock", "fort", "pwr1", "isld", "lava", "pwr2", "lar1", "lar2", "dark"]


def find(folder, name):
    for n in os.listdir(folder):
        if n.lower() == name.lower():
            return os.path.join(folder, n)
    return None


def desectorize(data, name):
    if len(data) % 2048:
        return data, 0
    out = bytearray()
    for s in range(0, len(data), 2048):
        sector = data[s:s + 2048]
        crc = 0
        for (w,) in struct.iter_unpack("<I", sector):
            crc ^= w
        if crc != 0:
            # not sector aligned after all (v1.0 / v1.1 data): keep as is
            return data, 0
        out += sector[:2044]
    return bytes(out), 1


def make_pack(files):
    header = 16 + 32 * len(files)
    offset = (header + 15) & ~15
    table, blob = [], bytearray()
    for name, data, flags in files:
        table.append(struct.pack("<16sIIII", name.encode(), offset + len(blob), len(data), flags, 0))
        blob += data
        blob += b"\0" * (-len(blob) & 15)
    out = bytearray(struct.pack("<4sIII", b"HODP", 1, len(files), 0))
    out += b"".join(table)
    out += b"\0" * (offset - len(out))
    out += blob
    struct.pack_into("<I", out, 12, len(out))
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("data", help="folder with SETUP.DAT and the *_HOD.* files")
    ap.add_argument("out", nargs="?", default="HeartOfDarkness",
                    help="output folder: the SD card's homebrews/HeartOfDarkness "
                         "(default: a HeartOfDarkness folder here, to copy there)")
    ap.add_argument("--paf", help="hod.paf (the CD's paf folder), for the cutscenes")
    ap.add_argument("--no-paf", action="store_true", help="no cutscenes")
    args = ap.parse_args()
    if not find(args.data, "SETUP.DAT"):
        msg = f"SETUP.DAT not found in {args.data}: the game's files are needed."
        if find(args.data, "HeartOfDarkness.exe"):
            msg += ("\nOn the CD they are inside HeartOfDarkness.exe: open it with 7-Zip,"
                    " extract everything to a folder, and give that folder.")
        else:
            msg += ("\nInstall the game from the CD, or open the CD's HeartOfDarkness.exe"
                    " with 7-Zip and extract everything to a folder.")
        sys.exit(msg)
    os.makedirs(args.out, exist_ok=True)
    print(f"output: {os.path.abspath(args.out)}")

    groups = [("setup", ["SETUP.DAT", "HOD.DEM"])]
    for lvl in LEVELS:
        groups.append((lvl, [f"{lvl.upper()}_HOD.LVL", f"{lvl.upper()}_HOD.SSS", f"{lvl.upper()}_HOD.MST"]))

    # the version (Resource::Resource()): v1.2 = sector aligned, v1.0 = the
    # first screen background size at the v1.1 offset is 0
    rock = find(args.data, "ROCK_HOD.LVL")
    if not rock:
        sys.exit(f"ROCK_HOD.LVL not found in {args.data}")
    rock_data, sectored = desectorize(open(rock, "rb").read(), "ROCK_HOD.LVL")
    info = 0
    if sectored:
        info |= 2
    elif struct.unpack_from("<I", rock_data, 0x2B88 + 4)[0] == 0:
        info |= 4
    if not find(args.data, "FORT_HOD.LVL"):
        info |= 8
    print("data: " + ("v1.2" if info & 2 else "v1.0" if info & 4 else "v1.1") + (" demo" if info & 8 else ""))

    total = 0
    for group, names in groups:
        files = []
        for n in names:
            path = find(args.data, n)
            if not path:
                if n == "HOD.DEM":
                    continue
                if group == "setup":
                    sys.exit(f"{n} not found in {args.data}")
                break
            data, flags = desectorize(open(path, "rb").read(), n)
            if n == "SETUP.DAT":
                flags |= info
            files.append((n.upper(), data, flags))
        if len(files) != len(names) and group != "setup":
            print(f"{group}: missing files, skipped (demo version?)")
            continue
        blob = make_pack(files)
        with open(os.path.join(args.out, group + ".hod"), "wb") as f:
            f.write(blob)
        total += len(blob)
        print(f"{group}.hod: {len(blob) / 1e6:.1f} MB")
    print(f"total {total / 1e6:.1f} MB")

    if args.no_paf:
        return
    paf = args.paf
    if not paf:
        paf = find(args.data, "hod.paf")
        sub = find(args.data, "paf")
        if not paf and sub and os.path.isdir(sub):
            paf = find(sub, "hod.paf")
    if not paf:
        print("hod.paf not found (the CD's paf folder; --paf): no cutscenes")
        return
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import paf_convert
    print(f"cutscenes: {paf} -> hod.gwp (a few minutes)")
    paf_convert.convert(paf, os.path.join(args.out, "hod.gwp"))


if __name__ == "__main__":
    main()
