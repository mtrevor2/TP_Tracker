"""Audit an iOS TPTracker package against a host executable without patching it.

This checks native imports and hook declarations, not runtime/SUMEE compatibility.
The host must be a thin arm64 Mach-O executable with a Dusklight symbol manifest.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import zipfile


def symbols(data):
    magic, cpu, _, kind, count, _, _, _ = struct.unpack_from("<8I", data)
    if magic != 0xFEEDFACF or cpu != 0x100000C:
        raise ValueError("Expected a thin arm64 Mach-O binary")
    offset, table = 32, None
    for _ in range(count):
        command, size = struct.unpack_from("<II", data, offset)
        if size < 8 or offset + size > len(data):
            raise ValueError("Invalid Mach-O load command")
        if command == 2:  # LC_SYMTAB
            table = struct.unpack_from("<4I", data, offset + 8)
        offset += size
    if table is None:
        raise ValueError("Missing Mach-O symbol table")
    symoff, count, stroff, _ = table
    found = {}
    for i in range(count):
        name, bits, _, desc, _ = struct.unpack_from("<IBBHQ", data, symoff + i * 16)
        if name and not bits & 0xE0:  # Ignore debug records.
            start = stroff + name
            found[data[start:data.index(0, start)].decode()] = (bits, desc >> 8)
    return kind, found


def check(package, host, symgen, report):
    package, host, symgen, report = map(lambda p: Path(p).resolve(), (package, host, symgen, report))
    if report in (package, host, symgen):
        raise ValueError("The report must not overwrite an input")
    host_data = host.read_bytes()
    with zipfile.ZipFile(package) as archive:
        manifest = json.loads(archive.read("mod.json"))
        if manifest.get("id") != "com.mikey022.tp_randomizer_tracker":
            raise ValueError("Expected a TPTracker package")
        native_data = archive.read("lib/ios-arm64/mod.so")
    native_kind, native_symbols = symbols(native_data)
    host_kind, host_symbols = symbols(host_data)
    if native_kind != 8 or host_kind != 2:
        raise ValueError("Expected an MH_BUNDLE mod and MH_EXECUTE host")
    imports = {name for name, (bits, ordinal) in native_symbols.items()
               if bits & 0x0F == 1 and ordinal == 255}
    if not imports:
        raise ValueError("No explicit imports from the host executable")
    exports = {name for name, (bits, _) in host_symbols.items() if bits & 0x0F == 15}
    missing = sorted(imports - exports)
    if missing:
        raise ValueError("Missing game imports: " + ", ".join(missing))
    if any(bits & 0x0F == 1 and ordinal in (0, 254) for bits, ordinal in native_symbols.values()):
        raise ValueError("Unscoped imports need a separate audit")
    with tempfile.TemporaryDirectory(prefix="tptracker-ios-audit-") as temporary:
        temporary = Path(temporary)
        native_path = temporary / "com.mikey022.tp_randomizer_tracker.so"
        native_path.write_bytes(native_data)
        meta_path, hook_path = temporary / "meta.json", temporary / "hooks.json"
        subprocess.run([str(symgen), "modmeta", "--check", "--out", str(meta_path), str(native_path)], check=True)
        metadata = json.loads(meta_path.read_text())["files"][0]
        by_id = lambda values: sorted(values, key=lambda value: value["service_id"])
        if (metadata["abi_version"] != manifest["abi"] or
                by_id(metadata["imports"]) != by_id(manifest["imports"])):
            raise ValueError("Native service requirements differ from the manifest")
        result = subprocess.run([str(symgen), "prepatch", "--check", "--binary", str(host),
                                 "--report", str(hook_path), str(native_path)])
        audit = json.loads(hook_path.read_text()) if hook_path.exists() else {"failures": ["No hook report"]}
        audit["binary"] = host.name
        for declaration in audit.get("declarations", []):
            declaration["mod_path"] = "lib/ios-arm64/mod.so"
        audit.update({"host_sha256": hashlib.sha256(host_data).hexdigest(),
                      "package_sha256": hashlib.sha256(package.read_bytes()).hexdigest(),
                      "game_imports_verified": len(imports), "device_tested": False,
                      "sumee_compatibility_verified": False,
                      "scope": "Static native imports and hooks only; host ABI and device testing still required."})
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8")
        if result.returncode or audit.get("failures") or any(d.get("failure") for d in audit.get("declarations", [])):
            raise ValueError(f"Hook compatibility audit failed; see {report}")
    if host.read_bytes() != host_data:
        raise ValueError("Host executable unexpectedly changed during the check")
    print(f"Verified {len(imports)} game imports and {len(audit['declarations'])} hook declarations. Report: {report}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--host", type=Path, required=True)
    parser.add_argument("--symgen", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    check(args.package, args.host, args.symgen, args.report)
