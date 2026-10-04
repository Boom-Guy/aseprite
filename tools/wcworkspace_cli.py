#!/usr/bin/env python3
"""
Wild Conquest - wcworkspace Command Line Interface (v2)
Lightweight asset interchange and round-trip pipeline bridge.
"""

import os
import sys
import json
import hashlib
import argparse
from datetime import datetime, timezone
from pathlib import Path


def calculate_sha256(filepath: Path) -> str:
    """Calculate SHA-256 hash of a file."""
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def calculate_bytes_sha256(data: bytes) -> str:
    """Calculate SHA-256 hash of bytes."""
    return hashlib.sha256(data).hexdigest()


def get_current_iso_time() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def cmd_create(args):
    ws_path = Path(args.path)
    char_name = args.name or ws_path.stem.replace("_", " ").title()
    char_id = ws_path.stem.lower().replace(" ", "_")

    width, height = 64, 64
    if args.canvas:
        try:
            parts = args.canvas.lower().split("x")
            width, height = int(parts[0]), int(parts[1])
        except Exception:
            pass

    ws_path.mkdir(parents=True, exist_ok=True)
    (ws_path / "source" / "parts").mkdir(parents=True, exist_ok=True)
    (ws_path / "animations").mkdir(parents=True, exist_ok=True)
    (ws_path / "metadata").mkdir(parents=True, exist_ok=True)

    workspace_json = {
        "format": "wcworkspace",
        "version": 2,
        "workspaceId": char_id,
        "character": {
            "id": char_id,
            "name": char_name
        },
        "canvas": {
            "width": width,
            "height": height
        },
        "pixelScale": 1,
        "facing": "right",
        "source": {
            "type": "aseprite"
        }
    }
    with open(ws_path / "workspace.json", "w", encoding="utf-8") as f:
        json.dump(workspace_json, f, indent=4)

    character_json = {
        "id": char_id,
        "name": char_name,
        "parts": []
    }
    with open(ws_path / "source" / "character.json", "w", encoding="utf-8") as f:
        json.dump(character_json, f, indent=4)

    manifest_json = {
        "version": 2,
        "lastModified": get_current_iso_time(),
        "sources": {
            "parts": {}
        },
        "animations": {}
    }
    with open(ws_path / "metadata" / "manifest.json", "w", encoding="utf-8") as f:
        json.dump(manifest_json, f, indent=4)

    result = {
        "success": True,
        "message": f"Created workspace at {ws_path}",
        "workspaceId": char_id,
        "character": char_name,
        "canvas": f"{width}x{height}"
    }

    if args.json:
        print(json.dumps(result, indent=2))
    else:
        print(f"[OK] Created Wild Conquest Workspace: {ws_path}")
        print(f"     Character: {char_name} (ID: {char_id})")
        print(f"     Canvas: {width}x{height}")


def cmd_validate(args):
    ws_path = Path(args.path)
    errors = []
    warnings = []

    if not ws_path.is_dir():
        errors.append(f"Not a directory: {ws_path}")

    ws_file = ws_path / "workspace.json"
    char_file = ws_path / "source" / "character.json"
    manifest_file = ws_path / "metadata" / "manifest.json"

    canvas_w, canvas_h = 64, 64
    if not ws_file.exists():
        errors.append("workspace.json missing")
    else:
        try:
            with open(ws_file, "r", encoding="utf-8") as f:
                ws_data = json.load(f)
                if ws_data.get("format") != "wcworkspace":
                    errors.append(f"Invalid format: {ws_data.get('format')}")
                if ws_data.get("version") != 2:
                    warnings.append(f"Expected format version 2, got {ws_data.get('version')}")
                canvas = ws_data.get("canvas", {})
                canvas_w = canvas.get("width", 64)
                canvas_h = canvas.get("height", 64)
        except Exception as e:
            errors.append(f"Failed to parse workspace.json: {e}")

    parts_dir = ws_path / "source" / "parts"
    if not parts_dir.exists():
        errors.append("source/parts/ directory missing")

    if not char_file.exists():
        errors.append("source/character.json missing")
    else:
        try:
            with open(char_file, "r", encoding="utf-8") as f:
                char_data = json.load(f)
                parts = char_data.get("parts", [])
                for part in parts:
                    asset_rel = part.get("asset")
                    if not asset_rel:
                        errors.append(f"Part '{part.get('id')}' missing 'asset' path")
                    elif not (ws_path / asset_rel).exists():
                        errors.append(f"Part asset not found on disk: {asset_rel}")
        except Exception as e:
            errors.append(f"Failed to parse source/character.json: {e}")

    # Check animations
    anims_dir = ws_path / "animations"
    anim_count = 0
    if anims_dir.exists() and anims_dir.is_dir():
        for item in sorted(anims_dir.iterdir()):
            if item.is_dir():
                anim_count += 1
                anim_json = item / "animation.json"
                if not anim_json.exists():
                    warnings.append(f"Animation '{item.name}' missing animation.json")
                else:
                    try:
                        with open(anim_json, "r", encoding="utf-8") as f:
                            a_data = json.load(f)
                            if a_data.get("id") != item.name:
                                warnings.append(f"Animation ID mismatch: folder '{item.name}' vs json id '{a_data.get('id')}'")
                    except Exception as e:
                        errors.append(f"Error parsing {anim_json}: {e}")

                # Check frame sequence in generated
                gen_dir = item / "generated"
                if gen_dir.exists() and gen_dir.is_dir():
                    gen_frames = sorted([f.name for f in gen_dir.glob("*.png")])
                    for idx, fname in enumerate(gen_frames):
                        expected = f"{idx:03d}.png"
                        if fname != expected:
                            warnings.append(f"Animation '{item.name}' generated frames out of order: expected {expected}, found {fname}")

    valid = len(errors) == 0
    result = {
        "valid": valid,
        "path": str(ws_path),
        "errors": errors,
        "warnings": warnings,
        "animationCount": anim_count
    }

    if args.json:
        print(json.dumps(result, indent=2))
    else:
        if valid:
            print(f"[OK] Workspace is VALID: {ws_path}")
            print(f"     Animations found: {anim_count}")
            if warnings:
                print("     Warnings:")
                for w in warnings:
                    print(f"       ! {w}")
        else:
            print(f"[FAIL] Workspace has {len(errors)} error(s):")
            for e in errors:
                print(f"       ✖ {e}")
            if warnings:
                print("     Warnings:")
                for w in warnings:
                    print(f"       ! {w}")

    return 0 if valid else 1


def cmd_inspect(args):
    ws_path = Path(args.path)
    ws_file = ws_path / "workspace.json"
    char_file = ws_path / "source" / "character.json"
    manifest_file = ws_path / "metadata" / "manifest.json"

    data = {}
    if ws_file.exists():
        with open(ws_file, "r", encoding="utf-8") as f:
            data["workspace"] = json.load(f)
    if char_file.exists():
        with open(char_file, "r", encoding="utf-8") as f:
            data["character"] = json.load(f)

    anims = {}
    anims_dir = ws_path / "animations"
    if anims_dir.exists() and anims_dir.is_dir():
        for d in sorted(anims_dir.iterdir()):
            if d.is_dir():
                a_info = {"id": d.name}
                a_json = d / "animation.json"
                if a_json.exists():
                    with open(a_json, "r", encoding="utf-8") as f:
                        a_info.update(json.load(f))
                gen_frames = len(list((d / "generated").glob("*.png"))) if (d / "generated").exists() else 0
                pol_frames = len(list((d / "polished").glob("*.png"))) if (d / "polished").exists() else 0
                has_sheet = (d / "sprite-sheet" / f"{d.name}.png").exists()
                a_info["generatedFramesCount"] = gen_frames
                a_info["polishedFramesCount"] = pol_frames
                a_info["hasSpriteSheet"] = has_sheet
                anims[d.name] = a_info
    data["animations"] = anims

    if args.json:
        print(json.dumps(data, indent=2))
    else:
        ws_info = data.get("workspace", {})
        char_info = data.get("character", {})
        print(f"=== Wild Conquest Workspace: {ws_path.name} ===")
        print(f"Character:   {char_info.get('name', 'N/A')} (ID: {char_info.get('id', 'N/A')})")
        canvas = ws_info.get("canvas", {})
        print(f"Canvas:      {canvas.get('width', 64)}x{canvas.get('height', 64)}")
        print(f"Facing:      {ws_info.get('facing', 'right')}")
        parts = char_info.get("parts", [])
        print(f"Parts ({len(parts)}):")
        for p in parts:
            print(f"  - {p.get('name')} (ID: {p.get('id')}) [z: {p.get('zIndex')}] -> {p.get('asset')}")
        print(f"Animations ({len(anims)}):")
        for aid, a in anims.items():
            print(f"  - {aid}: status={a.get('status', 'unknown')}, gen={a.get('generatedFramesCount', 0)}, pol={a.get('polishedFramesCount', 0)}, sheet={'YES' if a.get('hasSpriteSheet') else 'NO'}")


def cmd_manifest(args):
    ws_path = Path(args.path)
    manifest_file = ws_path / "metadata" / "manifest.json"

    # Refresh manifest hashes
    source_parts = {}
    parts_dir = ws_path / "source" / "parts"
    if parts_dir.exists():
        for p in sorted(parts_dir.glob("*.png")):
            source_parts[p.stem] = calculate_sha256(p)

    animations = {}
    anims_dir = ws_path / "animations"
    if anims_dir.exists():
        for ad in sorted(anims_dir.iterdir()):
            if not ad.is_dir():
                continue
            anim_id = ad.name
            gen_dir = ad / "generated"
            pol_dir = ad / "polished"
            sheet_file = ad / "sprite-sheet" / f"{anim_id}.png"

            gen_hash = ""
            if gen_dir.exists():
                h = hashlib.sha256()
                for gf in sorted(gen_dir.glob("*.png")):
                    h.update(gf.read_bytes())
                gen_hash = h.hexdigest()

            pol_hash = ""
            if pol_dir.exists():
                h = hashlib.sha256()
                for pf in sorted(pol_dir.glob("*.png")):
                    h.update(pf.read_bytes())
                pol_hash = h.hexdigest()

            sheet_hash = calculate_sha256(sheet_file) if sheet_file.exists() else ""

            status = "not_generated"
            if sheet_hash and pol_hash:
                status = "polished"
            elif pol_hash:
                status = "polishing"
            elif gen_hash:
                status = "generated"

            frame_count = len(list(pol_dir.glob("*.png"))) if pol_dir.exists() and len(list(pol_dir.glob("*.png"))) > 0 else (len(list(gen_dir.glob("*.png"))) if gen_dir.exists() else 0)

            animations[anim_id] = {
                "status": status,
                "generatedHash": gen_hash,
                "polishedHash": pol_hash,
                "spriteSheetHash": sheet_hash,
                "frameCount": frame_count
            }

    manifest = {
        "version": 2,
        "lastModified": get_current_iso_time(),
        "sources": {
            "parts": source_parts
        },
        "animations": animations
    }

    manifest_file.parent.mkdir(parents=True, exist_ok=True)
    with open(manifest_file, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=4)

    if args.json:
        print(json.dumps(manifest, indent=2))
    else:
        print(f"[OK] Refreshed manifest for {ws_path}:")
        print(f"     Source parts tracked: {len(source_parts)}")
        print(f"     Animations tracked:   {len(animations)}")


def main():
    parser = argparse.ArgumentParser(description="Wild Conquest wcworkspace Tool")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # create
    p_create = subparsers.add_parser("create", help="Create a new .wcworkspace")
    p_create.add_argument("path", help="Path to .wcworkspace folder")
    p_create.add_argument("--name", help="Character name")
    p_create.add_argument("--canvas", help="Canvas dimensions, e.g. 64x64", default="64x64")
    p_create.add_argument("--json", action="store_true", help="Output JSON")
    p_create.set_defaults(func=cmd_create)

    # validate
    p_validate = subparsers.add_parser("validate", help="Validate a .wcworkspace")
    p_validate.add_argument("path", help="Path to .wcworkspace folder")
    p_validate.add_argument("--json", action="store_true", help="Output JSON")
    p_validate.set_defaults(func=cmd_validate)

    # inspect
    p_inspect = subparsers.add_parser("inspect", help="Inspect a .wcworkspace")
    p_inspect.add_argument("path", help="Path to .wcworkspace folder")
    p_inspect.add_argument("--json", action="store_true", help="Output JSON")
    p_inspect.set_defaults(func=cmd_inspect)

    # manifest
    p_manifest = subparsers.add_parser("manifest", help="Refresh and display manifest hashes")
    p_manifest.add_argument("path", help="Path to .wcworkspace folder")
    p_manifest.add_argument("--json", action="store_true", help="Output JSON")
    p_manifest.set_defaults(func=cmd_manifest)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main() or 0)

