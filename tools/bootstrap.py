"""Fetch pinned external build headers; never builds the Dusklight game."""
import argparse
import json
import subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[1]
pins = json.loads((root / "dependencies.json").read_text())
parser = argparse.ArgumentParser()
parser.add_argument("--with-randomizer", action="store_true", help="Also fetch sources for optional data regeneration")
args = parser.parse_args()
destination = root / ".deps/dusklight"
def git(*args):
    subprocess.run(["git", "-C", str(destination), *args], check=True)
if not destination.exists():
    destination.mkdir(parents=True)
    git("init")
    git("remote", "add", "origin", pins["repository"])
if subprocess.check_output(["git", "-C", str(destination), "status", "--porcelain"], text=True).strip():
    raise SystemExit("External dependency has local changes; preserve them before updating.")
git("fetch", "--depth=1", "origin", pins["revision"])
git("checkout", "--detach", "FETCH_HEAD")
modules = ["extern/aurora"]
if args.with_randomizer:
    modules.append("mods/randomizer")
git("submodule", "update", "--init", "--depth=1", "--", *modules)
print("Pinned SDK ready at", destination)
