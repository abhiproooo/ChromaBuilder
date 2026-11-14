# Chroma Toons Rebuild Notes

## Target

- APK: `/root/MyRebuiltApp/Chroma_Toons_App-199.apk`
- Size: 174,453,591 bytes; 802 archive entries
- Package: `bhootiyadreamstv.moboapp.chromatoons`
- Version: `versionName="199"`, `versionCode="212"`
- SDK: `minSdkVersion="24"`, `targetSdkVersion="36"`, `compileSdkVersion="36"`
- Orientation: user landscape
- Application class: `com.pairip.application.Application` (PairIP-protected wrapper)
- jadx: `/root/tools/jadx/bin/jadx` version 1.5.6
- Java: OpenJDK 25
- Existing decompile: `/root/MyRebuiltApp/temp_decompiled/` (`sources` 124M, `resources` 118M)
- Rebuild target: `/root/MyRebuiltApp/app/src/main/java/com/example/rebuilt/`

## Critical Architecture Finding

The application logic is Unity IL2CPP, not Java/Kotlin.

- `temp_decompiled/sources/bhootiyadreamstv/moboapp/chromatoons/` contains only `R.java`.
- The remaining `com/*.java` files (12,994) are dependency wrappers: RevenueCat, Google Billing, AppLovin, Unity, PairIP, and other SDKs.
- The APK contains `assets/bin/Data/Managed/Metadata/global-metadata.dat`, `data.unity3d`, and `resources.resource`, confirming Unity IL2CPP.
- The base APK has no `lib/` directory and no `libil2cpp.so`.
- Manifest contains `requiredSplitTypes="base__abi"`, so the native ABI payload is in a separate split that is not present.
- Consequence: original C# method bodies cannot be decompiled or patched from this APK alone. The Java layer is not the premium logic.

## IL2CPP Metadata

Extracted file:

- `/tmp/chroma/assets/bin/Data/Managed/Metadata/global-metadata.dat`
- Metadata version: 31
- Types: 9,998
- Methods: 79,380
- String literals: 12,670

Useful parser offsets observed while inspecting:

- String table: offset 469,972; count 1,387,300
- Type definitions: offset 7,730,188; 88 bytes per type; 9,998 types
- Methods: offset 2,152,700; 36 bytes per method
- Fields: offset 6,543,772; 12 bytes per field
- String literal entries: offset 256; 8 bytes per entry; literal data offset 101,616

## Premium / Subscription Logic

Primary class:

- `RevenueCatManager` (metadata type index 5220, global namespace)

Fields observed:

- `encodedGoogleApiKey`
- `encodedEntitlementId`
- `rawApiKeyToEncode`
- `rawEntitlementToEncode`
- `resBtn`
- `subBtn`
- `pnl`
- `enableListObj`
- `Sts`
- `Exp`
- `pur`
- `buytone`

Methods observed:

- `Awake`
- `Start`
- `cya`
- `OnApplicationFocus`
- `cyb`
- `BuyProSubscription`
- `cyc`
- `RestorePurchases`
- `cyd` through `cyp` (obfuscated)

Related class:

- `RevenueCatIDManager` (type index 5141): `Start`, `GetAndDisplayUserId`, `cnq`

Premium UI strings in Unity assets:

- `Subscribe To Unlock Pro Features & Ad Free Experience`
- `For Pro Subscribers Only!`
- `For Paid Subscribers Only!`

Interpretation:

- `Sts`, `Exp`, and `pur` are likely subscription status, expiry, and purchase flags.
- `enableListObj` is likely a list of pro-only UI objects.
- The real entitlement gate is in the IL2CPP `RevenueCatManager`, not in Java.
- Patching the original APK would require the ABI split containing `libil2cpp.so`, then patching the entitlement return path with Ghidra/Il2CppDumper, or intercepting RevenueCat `CustomerInfo.Entitlements.active` at runtime.
- The rebuild instead uses a local `PremiumManager` whose checks always succeed.

## Rig / Pivot / IK Findings

Core rig namespace:

- `ChromaToons.CharacterAnimation`

Key types:

- `ChromaRigEditor` (type 5465, 107 methods)
- `ChromaRigEditorUI` (type 5467, 17 methods)
- `ChromaRigTimeline` (type 5477, 35 methods)
- `CharacterPartTransformEditor` (type 5443, 38 methods)
- `SpritePartPicker` (type 5446)
- `ChromaRigAnimationPlayer` (type 5447)
- `ChromaRigGrid` (5469), `ChromaRigLayerScroll` (5470), `ChromaRigOverlay` (5471), `ChromaRigSafeArea` (5472), `ChromaRigTimelineResize` (5478)

Rig data types:

- `RigAnimationClip`: `format`, `version`, `fps`, `frames`, `name`, `rigType`, `bones`, `choices`, `meshVisibility`
- `RigDefinition`: `characterType`, `customRigId`, `bones`, `mouthShapes`, `leftPalmShapes`, `rightPalmShapes`, `openFace`, `closedEyes`, `eyeBackground`, `eyeballs`, `replaceFaceWhenClosed`
- `RigTransformKey`: `frame`, `channels`, `position`, `rotation`, `scale`
- `RigMeshTrack`: `path`, `defaultVisible`, `keys`
- `RigMeshKey`: `frame`, `visible`
- `RigTool`: `Move`, `Rotate`, `Scale`
- `RigChoice`: `Mouth`, `LeftPalm`, `RightPalm`, `Eyes`
- `RigChannel`: `Position`, `Rotation`, `Scale`, `All`

Editor methods include:

- `SelectCharacter`
- `PreviousFrame`, `NextFrame`
- `TogglePlay`, `SetLoop`, `SetAutoKey`
- `ToolMove`, `ToolRotate`, `ToolScale`
- `AddSelectedKey`, `AddPoseKey`
- `SetMouth`, `SetLeftPalm`, `SetRightPalm`, `SetEyes`
- `ResetPose`, `FlipSelectedBone`
- `Undo`, `Redo`
- `SaveLocal`, `OpenLocalFiles`
- `ImportDocument`, `ExportDocument`

Pivot behavior:

- No explicit `pivot` field or string was found in metadata or `data.unity3d`.
- Each rig bone carries a rest transform and animation tracks store per-frame position/rotation/scale.
- Sprite parts are bound to bones; the effective pivot is the bone rest position plus the bound part's local offset.
- `CharacterPartTransformEditor` and `ChromaRigEditorUI` (`PositionEdited`, `RotationEdited`, `ScaleEdited`) expose live part/bone transform editing.
- For a per-part X/Y pivot adjustment, extend the bone rest position or bound-part local offset and apply it before the IK solve.

IK-related types/strings found:

- `SetIKMode` (type 5101)
- `Camera_IK`
- `Pos_ik`
- `BestBoneT` (44 methods)
- `BestBoneTDemo1`
- `DitzelGames.FastIK.FastIKFabric` (FABRIK) and `FastIKLook`
- `Precise2DRetargeter`
- `TwoDRetargeter`
- `BonePair`
- `BoneLink`
- `RuntimeBoneData`
- `CharacterBoneOffsetLatePass`
- UI toggles: `RHandIKToggle`, `tRHandIK`, `ikhand1`, `ikhand2R`, `ikleg1`, `ikleg2`
- Offset/limit strings: `LowerHandY`, `LHandDrop`, `RHandDrop`, `TrimHand`
- `Bone Editing Overlay`, `Bone Guides`, `Bone Tools`

Current IK handling is therefore a mix of legacy scene IK components, FABRIK, custom retargeters, and hand/leg IK toggles. The rig editor itself mainly edits bone tracks; IK is applied by runtime components and target transforms.

## Character View Angles

`RigCharacterType` values:

- `Side`
- `NewSide`
- `Front`

No `Back` value exists.

Asset-string evidence from `data.unity3d`:

- `SIdeChar/sidecharcreator1`
- `FrontCharacter2`
- `SideCharInitializer`
- `SideCharPanel`
- `FrontCharInitializer`
- `Load New Side Character`
- `Add side`

No `BackCharacter`, `BackChar`, or dedicated back-view asset string was found.

Conclusion:

- The build supports Front and Side/NewSide character views.
- It does not contain a dedicated Back-view rig/asset set.
- Strings such as `P3Dback` and `Walk_looking Back` are effects/animation names, not alternate back-view characters.

## Animation Format

Primary format is a custom JSON rig clip:

- `RigAnimationClip` with `format`, `version`, `fps`, `frames`, `name`, `rigType`, `bones`, `choices`, `meshVisibility`
- Bone tracks use bone paths and transform keys.
- Mouth, palm, and eye choices are separate tracks.
- Mesh visibility is a separate track.
- Loading/saving is handled by `ChromaRigEditor.SaveLocal`, `OpenLocalFiles`, `ImportDocument`, and `ExportDocument`.
- Unity strings `Custom Animation Library`, `JSON Character List`, `LoadCustomAnim`, `JsonRigIdentity`, `RigJson`, and `CustomRigBuilder` reinforce JSON as the content format.

DragonBones:

- DragonBones runtime types and strings are present.
- One sample asset set exists at `assets/Resources/mdd/jj/mecha_1004d_ske.json`, `mecha_1004d_tex.json`, `mecha_1004d_tex.png`.
- This appears to be a bundled sample/legacy runtime, not the primary rig-editor format.
- No Spine runtime types were found. `Ellen_Skeleton` and similar strings are humanoid bone names, not a Spine 2D integration.

Import difficulty:

- Moderate for the custom JSON format: match `rigType`, bone paths, `fps`, frame count, and rest transforms.
- Hard part is matching bone names/paths and choices to the target `RigDefinition`.
- `Precise2DRetargeter` and `TwoDRetargeter` can help map external skeletons.
- DragonBones would need conversion into the app's JSON schema or a dedicated importer.
- No Spine importer support is present.

Related cloud/media types:

- `ChromaSceneSaveLoad` (35 methods)
- `ChromaSceneArchive`, `ChromaSceneCatalog`, `ChromaSceneDocument`
- `ChromaCloudJsonCatalog`
- `ChromaZipCharacterDownload`
- `TimelineAnimationClipCache`
- `MediaProject`, `MediaEditorExport`, `ChromaTimelineExport`
- `LipsyncController`
- `BackgroundForegroundScale`

## Build Environment Status

- `app/src/main/java/com/example/rebuilt/` was empty before this rebuild scaffold.
- Gradle is not installed.
- `/usr/lib/android-sdk` contains only `platform-tools`; no `build-tools` or `platforms`.
- UnityPy is not installed; `pip install UnityPy` timed out after 120 seconds.
- Python `lz4.block` is unavailable.
- Therefore the rebuild can be authored but cannot be compile-verified on this host without installing Gradle and Android SDK platform/build-tools.

## Rebuild Decision

- Create a plain Android SDK Java app with no AndroidX or third-party dependencies.
- Use `android.app.Activity` and framework widgets only.
- Implement `PremiumManager` so `isPremium()` and all feature checks return `true`.
- Keep the UI as a clean base shell for rig/editor/animation features.
- Do not attempt to reimplement the full Unity renderer or recover IL2CPP method bodies.
- Document that this is a native replacement scaffold, not a byte-for-byte decompilation.

## Verification Limits

- Premium control flow is inferred from metadata names, fields, and UI strings; method bodies remain unavailable.
- View-angle support is inferred from enum values and asset strings; not runtime-verified.
- Pivot/IK behavior is inferred from class names and editor fields; exact solve order remains unknown.
- Animation format is inferred from class fields and asset strings; no sample custom clip was extracted and validated.
- The rebuilt project has not been compiled in this environment.
