# Wild Conquest — wcworkspace Round-Trip Asset Pipeline Specification (v2)

## 0. PRODUCT DIRECTION

`wcworkspace` is a lightweight asset interchange and round-trip pipeline for Wild Conquest.

### Division of Responsibility
- **Aseprite (ART)**:
  - Source pixel artwork for character parts.
  - Pixel cleanup & manual frame polish.
  - Final sprite sheet creation & export.
- **Unity (RIG + ANIMATION)**:
  - Skeleton, bones hierarchy, bone transforms.
  - 2D Animation Sprite Skin & rigging.
  - Animation keyframes, curves, and timing.
  - Pixel-perfect frame rendering.
- **wcworkspace (BRIDGE)**:
  - Transfers character parts from Aseprite to Unity.
  - Stores stable asset metadata with relative paths.
  - Receives rendered animation frames from Unity.
  - Makes frames available to Aseprite for pixel polish.
  - Receives polished sprite sheets back for final Unity visual import without breaking rigs.
  - Tracks source/output relationships with SHA-256 change detection.
  - Validates integrity and enforces overwrite protection.

---

## 1. TARGET WORKFLOW

```text
Aseprite (Draw Parts)
    ↓
Export Workspace (.wcworkspace)
    ↓
Unity Import (Build Hierarchy, SpriteRenderer/Skin)
    ↓
Create Skeleton & Rig in Unity
    ↓
Animate in Unity (Keyframes, Curves, Timing)
    ↓
Render Animation Frames (Wild Conquest -> Export Animation Frames)
    ↓
Write to wcworkspace (animations/<anim>/generated/)
    ↓
Aseprite receives animation frames (Wild Conquest -> Sync Animation from Workspace)
    ↓
Pixel Polish (Silhouettes, Clustered Pixels, Shading, Cleanup)
    ↓
Export final Sprite Sheet (Wild Conquest -> Export Polished Animation)
    ↓
Unity imports final Sprite Sheet (Wild Conquest -> Import Polished Animation)
    ↓
Game Runtime (Preserving Skeleton, Rig, Keyframes, Hierarchy)
```

---

## 2. WORKSPACE STRUCTURE (v2)

```text
Wolf_Assassin.wcworkspace/
├── workspace.json                  # Format v2, id, character, canvas, pixelScale, facing, source
├── source/
│   ├── character.json              # Part IDs, names, relative asset paths, pivots, zIndex, visibility
│   └── parts/                      # head.png, body.png, left_arm.png, etc.
├── animations/
│   └── attack/
│       ├── animation.json          # anim id, name, fps, frameCount, canvas, status
│       ├── generated/              # 000.png, 001.png, ...
│       ├── polished/               # 000.png, 001.png, ...
│       └── sprite-sheet/           # attack.png
└── metadata/
    └── manifest.json               # Hashes (sourceHash, generatedHash, polishedHash, spriteSheetHash)
```

---

## 3. METADATA SPECIFICATIONS

### `workspace.json`
```json
{
    "format": "wcworkspace",
    "version": 2,
    "workspaceId": "wolf_assassin",
    "character": {
        "id": "wolf_assassin",
        "name": "Wolf Assassin"
    },
    "canvas": {
        "width": 64,
        "height": 64
    },
    "pixelScale": 1,
    "facing": "right",
    "source": {
        "type": "aseprite"
    }
}
```

### `source/character.json`
```json
{
    "id": "wolf_assassin",
    "name": "Wolf Assassin",
    "parts": [
        {
            "id": "head",
            "name": "Head",
            "asset": "source/parts/head.png",
            "pivot": { "x": 32, "y": 16 },
            "zIndex": 10,
            "visible": true
        }
    ]
}
```

### `animations/<id>/animation.json`
```json
{
    "id": "attack",
    "name": "Attack",
    "fps": 12,
    "frameCount": 6,
    "canvas": {
        "width": 64,
        "height": 64
    },
    "source": "unity",
    "status": "generated"
}
```

### `metadata/manifest.json`
```json
{
    "version": 2,
    "lastModified": "2026-10-04T10:00:00Z",
    "sources": {
        "parts": {
            "head": "sha256_hash_here"
        }
    },
    "animations": {
        "attack": {
            "status": "generated",
            "generatedHash": "sha256_hash_here",
            "polishedHash": "",
            "spriteSheetHash": "",
            "frameCount": 6
        }
    }
}
```

---

## 4. PIXEL-PERFECT REQUIREMENTS
- Integer canvas dimensions (e.g. 64x64).
- Point filtering (no blur, no bilinear/trilinear).
- No anti-aliasing.
- Preserve full alpha channel.
- Camera and character framing fixed across all frames.

---

## 5. OVERWRITE PROTECTION & ROUND-TRIP SAFETY
- Unity frame export will NEVER overwrite existing `polished/` frames without explicit confirmation.
- Unity polished sprite sheet import updates texture slices/sprites for playback WITHOUT altering Bone hierarchy, SpriteSkin bindings, Animator Controller, or Animation Clips.

