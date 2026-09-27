#!/usr/bin/env python3
"""Run clang-tidy on the project sources (main/, components/).

Needs Espressif's clang (xtensa inline asm in FreeRTOS/IDF headers):
    python $IDF_PATH/tools/idf_tools.py install esp-clang

The IDF compile database is made for xtensa GCC. This script rewrites it for clang:
GCC-only flags are dropped, GCC's system include dirs and predefined macros are
passed explicitly, so clang sees exactly what the real compiler sees.

Usage: idf.py build && tools/clang_tidy.py [--fix] [files...]
"""

import argparse
import glob
import concurrent.futures
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
OUT = BUILD / "clang-tidy"
SOURCE_DIRS = (ROOT / "main", ROOT / "components")
TOOLS_PATH = Path(os.environ.get("IDF_TOOLS_PATH", Path.home() / ".espressif"))

# GCC/xtensa flags clang does not understand (prefix match)
DROP_FLAGS = (
    "-mlongcalls",
    "-mdisable-hardware-atomics",
    "-fstrict-volatile-bitfields",
    "-fno-tree-switch-conversion",
    "-fno-shrink-wrap",
    "-fdiagnostics-color",
    "-specs=",
    "-Werror",
    "-Wno-frame-address",
    "-fmacro-prefix-map",
)

EXTENDED_FLOAT_MACRO = re.compile(r"#define __(STDCPP_FLOAT|STDCPP_BFLOAT|B?FLT(16|32|64|128)X?_)")


def esp_clang_bin() -> Path | None:
    candidates = sorted(glob.glob(str(TOOLS_PATH / "tools/esp-clang/*/esp-clang/bin")))
    return Path(candidates[-1]) if candidates else None


def gcc_environment(gcc: str, flags: list[str]) -> tuple[list[str], Path]:
    """System include dirs and a header with the predefined macros of the target GCC.

    `flags` must include everything that changes predefined macros (-specs, -fno-rtti, ...).
    """
    probe = subprocess.run(
        [gcc, *flags, "-xc++", "-E", "-v", "-dM", "-"],
        input="", capture_output=True, text=True, check=True,
    )
    lines = probe.stderr.splitlines()
    start = lines.index("#include <...> search starts here:") + 1
    end = lines.index("End of search list.")
    includes = [os.path.normpath(line.strip()) for line in lines[start:end]]

    # GCC's extended float types (_Float32, _Float64, ...) don't exist in clang's C++ mode, and
    # libstdc++ enables them based on these macros.
    macros = [m for m in probe.stdout.splitlines() if not EXTENDED_FLOAT_MACRO.match(m)]
    header = OUT / "gcc_predefined.h"
    header.write_text("\n".join(macros) + "\n")
    return includes, header


def rewrite(entry: dict, clangxx: Path, includes: list[str], header: Path) -> dict:
    args = entry.get("arguments") or shlex.split(entry["command"])
    expanded: list[str] = []
    for arg in args[1:]:
        if arg.startswith("@"):  # response file, e.g. build/toolchain/cxxflags
            expanded += shlex.split(Path(arg[1:].strip('"')).read_text())
        else:
            expanded.append(arg)

    out = [str(clangxx), "--target=xtensa-esp-elf", "-mcpu=esp32", "-nostdinc", "-nostdinc++", "-undef"]
    out += ["-Wno-unknown-warning-option", "-Wno-macro-redefined", "-Wno-builtin-macro-redefined"]
    out += ["-include", str(header)]
    for inc in includes:
        out += ["-isystem", inc]
    out += [a for a in expanded if not a.startswith(DROP_FLAGS)]
    return {"directory": entry["directory"], "file": entry["file"], "arguments": out}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--fix", action="store_true", help="apply clang-tidy fixes")
    parser.add_argument("files", nargs="*", help="limit to these sources")
    opts = parser.parse_args()

    bindir = esp_clang_bin()
    if bindir is None:
        print("esp-clang not installed: python $IDF_PATH/tools/idf_tools.py install esp-clang", file=sys.stderr)
        return 2
    db_path = BUILD / "compile_commands.json"
    if not db_path.exists():
        print("build/compile_commands.json missing, run `idf.py build` first", file=sys.stderr)
        return 2

    database = json.loads(db_path.read_text())
    ours = [e for e in database if any(Path(e["file"]).is_relative_to(d) for d in SOURCE_DIRS)]
    if opts.files:
        wanted = {Path(f).resolve() for f in opts.files}
        ours = [e for e in ours if Path(e["file"]) in wanted]
    if not ours:
        print("no matching sources in the compile database", file=sys.stderr)
        return 2

    OUT.mkdir(exist_ok=True)
    first = shlex.split(ours[0]["command"]) if "command" in ours[0] else ours[0]["arguments"]
    macro_flags = [f for f in first[1:] if f.startswith(("-std=", "-fno-rtti", "-fno-exceptions"))]
    cxxflags = BUILD / "toolchain" / "cxxflags"
    if cxxflags.exists():
        macro_flags += [f for f in shlex.split(cxxflags.read_text()) if f.startswith(("-specs=", "-fno-rtti"))]
    includes, header = gcc_environment(first[0], macro_flags)
    (OUT / "compile_commands.json").write_text(
        json.dumps([rewrite(e, bindir / "clang++", includes, header) for e in ours], indent=1))

    def tidy(entry: dict) -> tuple[str, int, str]:
        cmd = [str(bindir / "clang-tidy"), "-p", str(OUT), "--quiet", entry["file"]]
        if opts.fix:
            cmd.insert(1, "--fix")
        res = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT)
        return entry["file"], res.returncode, res.stdout + res.stderr

    failed = 0
    jobs = 1 if opts.fix else os.cpu_count()
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        for file, code, output in pool.map(tidy, ours):
            status = "ok" if code == 0 else "FAILED"
            print(f"[{status}] {Path(file).relative_to(ROOT)}")
            if code != 0:
                failed += 1
                print(output.rstrip())
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
