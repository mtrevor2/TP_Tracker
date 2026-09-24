"""Paths to optional external SDK and data-regeneration sources."""
import os
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
DUSKLIGHT = Path(os.environ.get("DUSKLIGHT_SOURCE_DIR", ROOT / ".deps/dusklight"))
RANDOMIZER = Path(os.environ.get("RANDOMIZER_SOURCE_DIR", DUSKLIGHT / "mods/randomizer"))
