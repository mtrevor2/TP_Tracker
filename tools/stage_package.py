"""Validate a built .dusk and keep a versioned copy in the repository's dist/."""
import argparse
import json
from pathlib import Path
import re
import shutil
import zipfile

PLATFORMS = {"windows-amd64", "linux-x86_64", "linux-aarch64", "macos-arm64",
             "macos-x86_64", "android-aarch64", "ios-arm64"}


def stage(source, destination, require_all=False):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    with zipfile.ZipFile(source) as package:
        manifest = json.loads(package.read("mod.json"))
        if manifest.get("id") != "com.mikey022.tp_randomizer_tracker":
            raise ValueError("Not a TPTracker package")
        version = manifest["version"]
        if not re.fullmatch(r"\d+\.\d+\.\d+", version):
            raise ValueError("Package must have a numeric release version")
        platforms = {name.split('/')[1] for name in package.namelist()
                     if name.startswith('lib/') and not name.endswith('/') and len(name.split('/')) == 3}
        if not platforms or not platforms <= PLATFORMS:
            raise ValueError(f"Unexpected native platforms: {sorted(platforms)}")
        if require_all and platforms != PLATFORMS:
            raise ValueError(f"Missing platforms: {sorted(PLATFORMS - platforms)}")
        if len(platforms) > 1 and platforms != PLATFORMS:
            raise ValueError("A multiplatform release must contain all seven supported platforms")
        label = "multiplatform" if platforms == PLATFORMS else next(iter(platforms))
        for name in package.namelist():
            if name.startswith('lib/') and not name.endswith('/') and package.getinfo(name).file_size == 0:
                raise ValueError(f"Empty native library: {name}")
        bad = package.testzip()
        if bad:
            raise ValueError(f"Damaged package entry: {bad}")
    destination.mkdir(parents=True, exist_ok=True)
    output = destination / f"TPTracker-{version}-{label}.dusk"
    if source != output:
        temporary = output.with_suffix('.dusk.tmp')
        shutil.copy2(source, temporary)
        temporary.replace(output)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    parser.add_argument("--dist", type=Path, default=Path(__file__).resolve().parents[1] / "dist")
    parser.add_argument("--require-all", action="store_true")
    args = parser.parse_args()
    print(stage(args.package, args.dist, args.require_all))
