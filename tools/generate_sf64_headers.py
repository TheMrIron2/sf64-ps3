#!/usr/bin/env python3
"""Generate the imported game's asset declarations for the title build."""

import argparse
import hashlib
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


US_REV1_SHA1 = "09f0d105f476b00efa5303a3ebc42e60a7753b7a"
US_REV1_DECOMPRESSED_SHA1 = "f7475fb11e7e6830f82883412638e8390791ab87"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--torch", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cc", default="cc")
    args = parser.parse_args()

    source = args.source.resolve()
    rom = args.rom.resolve()
    torch = args.torch.resolve()
    output = args.output.resolve()
    if not torch.is_file():
        raise ValueError(
            f"Torch not found at {torch}. Run 'git submodule update --init "
            "game/tools/Torch' and 'make title-tools', or set SF64_TORCH."
        )
    if hashlib.sha1(rom.read_bytes()).hexdigest() != US_REV1_SHA1:
        raise ValueError("expected a big-endian Star Fox 64 US Rev 1.1 ROM")

    output.mkdir(parents=True, exist_ok=True)
    (output / "headers.stamp").unlink(missing_ok=True)
    mio0 = output / "mio0"
    uncompressed = output / "baserom.us.rev1.uncompressed.z64"
    subprocess.run(shlex.split(args.cc) + [
        "-O2", "-DMIO0_STANDALONE", "-o", str(mio0),
        str(source / "tools/mio0-decompressor/libmio0.c"),
    ], check=True)
    subprocess.run([
        sys.executable, str(source / "tools/comptool.py"), "-d", "-m", str(mio0),
        str(rom), str(uncompressed),
    ], check=True)
    if hashlib.sha1(uncompressed.read_bytes()).hexdigest() != US_REV1_DECOMPRESSED_SHA1:
        raise ValueError("decompressed ROM checksum does not match US Rev 1.1")

    stage = output / "torch-work"
    stage.mkdir(exist_ok=True)
    for name in ("assets", "include"):
        link = stage / name
        if link.is_symlink():
            link.unlink()
        elif link.exists():
            raise ValueError(f"refusing to replace {link}")
        link.symlink_to(source / name, target_is_directory=True)
    config = (source / "config.yml").read_text(encoding="utf-8")
    if "headers: include/assets" not in config:
        raise ValueError("Torch configuration has no expected header output")
    config = config.replace("headers: include/assets", "headers: ../generated/assets")
    (stage / "config.yml").write_text(config, encoding="utf-8")
    generated = output / "generated"
    if generated.exists():
        shutil.rmtree(generated)
    subprocess.run([str(torch), "header", str(uncompressed)], cwd=stage, check=True)
    for name in ("ast_radio.h", "ast_common.h", "ast_title.h"):
        if not (generated / "assets" / name).is_file():
            raise ValueError(f"Torch did not generate {name}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.CalledProcessError) as exc:
        sys.exit(f"error generating SF64 headers: {exc}")
