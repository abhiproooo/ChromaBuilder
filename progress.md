# Progress

## Phase 1 (done)
- C++ NDK project, JNI bridge, GitHub Actions APK build.
- DragonBones JSON/atlas parser (`json.*`, `dragonbones.*`).
- Assets: `mecha_1004d_ske.json`, `mecha_1004d_tex.json`, `mecha_1004d_tex.png`.

## Phase 2 (current)
### Native renderer
- `renderer.h` / `renderer.cpp`
  - `Matrix2D` (row-vector, `concat(parent)` = `this * parent`).
  - `BoneTransform` (`toMatrix`/`fromMatrix`, DragonBones semantics).
  - `DragonBonesRenderer::buildFrame` -> `SpriteCommand` list.
  - Bone inheritance flags, offsets (local space, applied before children).
  - Slot draw order, image displays, recursive nested armatures.
  - Nested display matrix: `displayLocal * bone * parentArmature`.
- Parsed extras: `DisplayData.pivotX/pivotY`, `SlotData.alpha`, `BoneData.inheritTranslation/inheritReflection`.
- Sprite packing (15 floats):
  `[atlasX, atlasY, atlasW, atlasH, x, y, w, h, a, b, c, d, tx, ty, alpha]`.

### JNI (`native-lib.cpp`)
- `setBoneOffset`, `clearBoneOffsets`, `getRenderSpriteData`, `getArmatureBounds`.
- Engine version: `ChromaToons Native Engine 2.1.0 (DragonBones renderer)`.

### Kotlin UI (`MainActivity.kt`)
- Compose Canvas, dark theme, fit-to-AABB camera + grid.
- Bone dropdown, Right Hand X/Y sliders (-80..80), reset.
- Default bone `hand_r`; `Matrix.setValues([a,c,tx,b,d,ty,0,0,1])`.
- Assets decoded once; texture drawn with `nativeCanvas` + `concat`.

## Verification
- Host build `clang++ -std=c++20 -Wall -Wextra -Werror`: passes.
- Main armature renders 16 sprites; `/tmp/opencode/render.png` visually correct.
- Offset test: `hand_r (+60,-20)` moves hand image + attached right weapon; left weapon and parents unchanged.
- Android build not possible locally (no SDK/Gradle); use GitHub Actions.

## Next
- Commit + push to trigger `.github/workflows/build.yml`, then fix any Kotlin/Compose or NDK errors.
- Run on device: confirm live hand_r/weapon movement.
