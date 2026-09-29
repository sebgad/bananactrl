#!/usr/bin/env python3
"""Release assets of a build, as published on GitHub, into <build>/dist/.

Run by the `dist` target of every `idf.py build` (see CMakeLists.txt) and by .github/workflows/build.yml:

    bananactrl-<version>.bin          app image (OTA page, app-flash)
    webui-<version>.tar               the files of data/ (OTA page, Update from GitHub)
    storage-<version>.bin             LittleFS image of data/
    bananactrl-factory-<version>.bin  everything merged, flashed at 0x0
    bootloader.bin, partition-table.bin, ota_data_initial.bin, flash_args
    MD5SUMS                           of the *.bin and *.tar files (format of md5sum)
    VERSION                           firmware version (not a release asset)
"""

import argparse
import hashlib
import io
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path


def web_ui_tar(data_dir: Path, target: Path) -> None:
    """Reproducible ustar archive of the top-level files in data/ (TarReader on the device reads it)."""
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w", format=tarfile.USTAR_FORMAT) as tar:
        for path in sorted(data_dir.iterdir()):
            if not path.is_file() or path.name.startswith("."):
                continue
            info = tarfile.TarInfo(path.name)
            info.size = path.stat().st_size
            info.mode = 0o644
            info.mtime = 0
            info.uid = info.gid = 0
            info.uname = info.gname = ""
            with path.open("rb") as file:
                tar.addfile(info, file)
    target.write_bytes(buffer.getvalue())


def md5sums(dist: Path) -> str:
    files = sorted(dist.glob("*.bin")) + sorted(dist.glob("*.tar"))
    return "".join(f"{hashlib.md5(f.read_bytes()).hexdigest()}  {f.name}\n" for f in files)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--chip", default="esp32")
    args = parser.parse_args()

    build: Path = args.build_dir.resolve()
    version: str = args.version
    dist = build / "dist"
    shutil.rmtree(dist, ignore_errors=True)  # no images of an older version
    dist.mkdir()

    shutil.copyfile(build / "bananactrl.bin", dist / f"bananactrl-{version}.bin")
    shutil.copyfile(build / "storage.bin", dist / f"storage-{version}.bin")
    shutil.copyfile(build / "bootloader" / "bootloader.bin", dist / "bootloader.bin")
    shutil.copyfile(build / "partition_table" / "partition-table.bin", dist / "partition-table.bin")
    shutil.copyfile(build / "ota_data_initial.bin", dist / "ota_data_initial.bin")
    shutil.copyfile(build / "flash_args", dist / "flash_args")
    web_ui_tar(args.data_dir, dist / f"webui-{version}.tar")

    # as `idf.py merge-bin`: the images at the offsets in flash_args (relative to the build directory)
    subprocess.run([sys.executable, "-m", "esptool", "--chip", args.chip, "merge-bin",
                    "-o", str(dist / f"bananactrl-factory-{version}.bin"), "@flash_args"],
                   cwd=build, check=True, stdout=subprocess.DEVNULL)

    (dist / "MD5SUMS").write_text(md5sums(dist))
    (dist / "VERSION").write_text(version + "\n")
    print(f"Release assets of {version} in {dist}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
