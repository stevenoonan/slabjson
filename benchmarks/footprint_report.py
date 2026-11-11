#!/usr/bin/env python3
"""Build comparable Cortex-M4 ELFs, audit dependencies, and report their size.

No board execution is required. DOM consumption is measured with the same
fixture on the host; node layout is asserted separately in the ARM build.
Compiler .su files report individual frames, not complete call-chain peaks.
"""

import argparse
import csv
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess


def run(command, **kwargs):
    result = subprocess.run([str(part) for part in command], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)
    if result.returncode:
        raise RuntimeError(f"{shlex.join(map(str, command))}\n{result.stdout}")
    return result.stdout


def tool(compiler, name):
    candidate = compiler.with_name(compiler.name.replace("g++", name))
    result = str(candidate) if candidate.is_file() else shutil.which(f"arm-none-eabi-{name}")
    if not result:
        raise RuntimeError(f"Cannot find arm-none-eabi-{name}")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default="arm-none-eabi-g++")
    parser.add_argument("--host-compiler", default=os.environ.get("CXX", "c++"))
    args = parser.parse_args()
    source, output = args.source.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    compiler = Path(shutil.which(args.compiler) or args.compiler).absolute()
    size_tool, nm_tool = tool(compiler, "size"), tool(compiler, "nm")
    common = ["-Os", "-DNDEBUG", "-ffunction-sections", "-fdata-sections", "-fno-exceptions", "-fno-rtti"]
    arm = ["-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=soft"]
    # Hold all compiler and linker settings constant, including section GC.
    # Only library configuration changes between these rows.
    profiles = [
        ("full", "FULL", False, True, True, True, True, 32),
        ("full-size", "FULL", True, True, True, True, True, 32),
        ("compact32", "COMPACT32", False, True, True, True, True, 32),
        ("integer32", "INTEGER32", False, True, True, True, True, 32),
        ("integer32-size", "INTEGER32", True, True, True, True, True, 32),
        ("mcu32", "COMPACT32", True, True, True, False, False, 16),
        ("mcu-integer", "INTEGER32", True, True, True, False, False, 16),
        ("parse-only", "INTEGER32", True, True, False, False, False, 16),
        ("serialize-only", "INTEGER32", True, False, True, False, False, 16),
        ("construction-only", "INTEGER32", True, False, False, False, False, 16),
    ]
    rows = []
    for name, mode, optimize, parse, serialize, pretty, compat, depth in profiles:
        print(f"Measuring {name}", flush=True)
        directory = output / name
        flags = common + arm + ["-fstack-usage"]
        settings = dict(NUMBER_MODE=mode, OPTIMIZE_SIZE=optimize, ENABLE_PARSER=parse,
                        ENABLE_SERIALIZER=serialize, ENABLE_PRETTY=pretty,
                        ENABLE_CJSON_COMPAT=compat, MAX_PARSER_DEPTH=depth)
        run(["cmake", "-S", source, "-B", directory, "-G", "Ninja",
             "-DCMAKE_SYSTEM_NAME=Generic", "-DCMAKE_SYSTEM_PROCESSOR=arm",
             f"-DCMAKE_CXX_COMPILER={compiler}", "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
             "-DCMAKE_BUILD_TYPE=MinSizeRel", f"-DCMAKE_CXX_FLAGS={shlex.join(flags)}",
             "-DSLABJSON_BUILD_TESTS=OFF", "-DSLABJSON_ENABLE_LTO=OFF",
             *[f"-DSLABJSON_{key}={('ON' if value else 'OFF') if isinstance(value, bool) else value}"
               for key, value in settings.items()]])
        run(["cmake", "--build", directory, "--parallel", "4"])
        includes = [f"-I{directory / 'generated'}", f"-I{source / 'include'}"]
        stacks = []
        for path in sorted(directory.glob("CMakeFiles/slabjson.dir/src/*.su")):
            stacks.extend(path.read_text().splitlines())
        (directory / "stack-usage.txt").write_text("\n".join(stacks) + "\n")
        parser_frames = [int(line.split("\t")[1]) for line in stacks if "Parser::parse_document()" in line]
        operations = [("empty", 0), ("construct", 4)]
        if parse:
            operations.append(("parse", 1))
        if serialize:
            operations.append(("serialize", 2))
        if parse and serialize:
            operations.append(("roundtrip", 3))
        for operation, number in operations:
            elf = directory / f"{operation}.elf"
            run([compiler, "-std=c++20", *flags, *includes, f"-DFOOTPRINT_OPERATION={number}",
                 source / "benchmarks/footprint_main.cpp", directory / "libslabjson.a",
                 "--specs=nosys.specs", "-Wl,--gc-sections", f"-Wl,-Map={elf.with_suffix('.map')}",
                 "-o", elf], cwd=directory)
            values = run([size_tool, elf]).splitlines()[-1].split()
            symbols = run([nm_tool, "--defined-only", "--demangle", elf])
            allocation = bool(re.search(r"\b(?:malloc|calloc|realloc|_malloc_r|_calloc_r|_realloc_r|operator new)\b", symbols))
            floating = bool(re.search(r"std::(?:from_chars|to_chars)\([^\n]*\b(?:float|double)\b", symbols))
            if mode == "INTEGER32" and (allocation or floating):
                raise RuntimeError(f"Unexpected allocation/float conversion dependency in {name}/{operation}")
            dom_used = slab_capacity = node_bytes = input_capacity = output_capacity = 0
            if number:
                # Run the same fixture natively, using the ARM-generated config.
                host = directory / f"{operation}-host"
                sources = [source / "src" / f"{unit}.cpp" for unit in ("array", "object", "slab", "value")]
                if parse:
                    sources.append(source / "src/parse.cpp")
                if serialize:
                    sources.append(source / "src/serialize.cpp")
                run([args.host_compiler, "-std=c++20", *common, *includes,
                     f"-DFOOTPRINT_OPERATION={number}", "-DFOOTPRINT_HOST_PROBE",
                     source / "benchmarks/footprint_main.cpp", *sources, "-o", host])
                dom_used, slab_capacity, node_bytes, input_capacity, output_capacity = map(int, run([host]).split())
            baseline = next((row["text_rodata"] for row in rows if row["profile"] == name
                             and row["operation"] == "empty"), int(values[0]))
            rows.append(dict(profile=name, number_mode=mode, operation=operation,
                             text_rodata=int(values[0]), text_above_empty=int(values[0]) - baseline,
                             data=int(values[1]), bss=int(values[2]), dom_used=dom_used,
                             slab_capacity=slab_capacity, node_bytes=node_bytes,
                             output_capacity=output_capacity,
                             input_capacity=input_capacity,
                             parser_frame_bytes=max(parser_frames, default=0) if number in (1, 3) else 0,
                             parser_frame_array_bytes=8 * depth if number in (1, 3) else 0,
                             allocation_symbols=allocation, float_conversion_symbols=floating))
    full = next(row for row in rows if row["profile"] == "full" and row["operation"] == "roundtrip")
    integer = next(row for row in rows if row["profile"] == "mcu-integer" and row["operation"] == "roundtrip")
    if integer["text_rodata"] >= full["text_rodata"]:
        raise RuntimeError("Integer MCU roundtrip must be smaller than FULL")
    with (output / "footprint.csv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (output / "footprint.json").write_text(json.dumps(dict(
        compiler=run([compiler, "--version"]).splitlines()[0],
        flags=common + arm, linker_flags=["--specs=nosys.specs", "-Wl,--gc-sections"],
        notes=["Size is a static link measurement, not board execution.",
               "DOM consumption is measured on the host with matching configuration and asserted node sizes.",
               "Stack figures are individual compiler frames, excluding callees and standard library stack."],
        rows=rows), indent=2) + "\n")
    print(f"Reports: {output / 'footprint.csv'} and footprint.json", flush=True)


if __name__ == "__main__":
    main()
