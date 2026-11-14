# AGENT CONTEXT & RULES (READ FIRST)

## Project Goal
Build a high-performance, native Android app (Kotlin + Jetpack Compose + C++ NDK) from scratch. 
It is an "ST Pro-style" 2.5D Character Animator. It must run at 60FPS with no snapping.

## Core Features to Build
1. **C++ Native Engine:** All heavy math (bone calculations, interpolation, view blending) MUST be written in C++ (`app/src/main/cpp/`) and linked via JNI. Kotlin is only for UI and passing data.
2. **DragonBones Parser:** Parse `mecha_1004d_ske.json` and `mecha_1004d_tex.json` from the `assets/` folder.
3. **2.5D Smooth Blending:** Crossfade (alpha blend) between 8 directional views based on a slider. 
4. **Live Rigging:** Real-time X/Y offset sliders for specific bones (e.g., "hand_r").
5. **Smooth Keyframes:** Bezier/Cubic interpolation between keyframes (no snapping).

## Architecture Rules
- Kotlin (`MainActivity.kt`) -> JNI Bridge -> C++ (`native-lib.cpp`).
- Do NOT try to compile the APK inside this environment. We use GitHub Actions for that.
- If the context window hits 80%, IMMEDIATELY write a summary of your progress into `progress.md` and compact your memory.
