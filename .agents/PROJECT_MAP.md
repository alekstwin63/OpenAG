# OpenAG: Developer & Agent Codebase Guide

This document serves as a comprehensive architectural map and technical guide for OpenAG, designed to help incoming AI agents and developers quickly understand the project layout, key subsystems, custom additions, and code integration patterns.

---

## 1. Project Overview
**OpenAG** is a modern client/server mod for Half-Life (GoldSrc engine) based on the Adrenaline Gamer (AG) ruleset. It implements modern HUD systems via Dear ImGui, modern networking, Discord integration, and advanced rendering enhancements (e.g., motion blur, custom crosshairs, spawns display).

---

## 2. Directory Structure & Key Subsystems

- **/cl_dll/**: The Client-side library (compiles to `client.so` on Linux, `client.dll` on Windows). This is where all user interface, input handling, custom rendering, and ImGui overlays reside.
- **/dlls/**: The Server-side library (compiles to `hl.so`/`hl.dll`). Manages game rules, player logic, weapons physics, and server-side commands.
- **/game_shared/** & **/pm_shared/**: Shared logic between client and server (movement prediction physics, weapon characteristics, player states).
- **/external/**: External libraries compiled statically or linked (Dear ImGui, SDL2, Discord RPC, curl).
- **/build/**: Active Linux compilation directory.

---

## 3. Core Subsystem Architectures & Entry Points

### A. UI and Dear ImGui Integration
- **Main Helper**: [imgui_helper.cpp](file:///home/angel/OpenAG/cl_dll/imgui_helper.cpp)
  - Manages the lifecycle of the ImGui context, event handling (mouse/keyboard input capturing), style definitions (custom dark glassmorphic theme), and rendering of overlays.
  - **Key Variables**: `g_ShowImGuiMenu`, `g_ShowCrosshairEditor`, `g_ShowAGSettings`, `g_ShowPauseMenu`, `g_ShowServerBrowser`.
- **Initialization & Input Injection**: [cdll_int.cpp](file:///home/angel/OpenAG/cl_dll/cdll_int.cpp)
  - Hooks into engine initialization, frame redraw loops, and translates SDL2 events to ImGui input events.
  - Manages engine `GameUI` state changes to toggle the cursor and input focus.

### B. Custom Rendering Hooks
- **View Setup & Viewmodel**: [view.cpp](file:///home/angel/OpenAG/cl_dll/view.cpp)
  - Calculates viewangles, vieworg (position), viewmodel (weapon models) offset coordinates, and triggers weapon sway and motion blur calculations.
- **Model Rendering**: [StudioModelRenderer.cpp](file:///home/angel/OpenAG/cl_dll/StudioModelRenderer.cpp)
  - Overrides standard GoldSrc studio model drawing to adjust viewmodel FOV (`cl_viewmodel_fov`).
- **HUD Frame Redraw**: [hud_redraw.cpp](file:///home/angel/OpenAG/cl_dll/hud_redraw.cpp)
  - Main client-side drawing loop. Calls individual HUD elements and invokes ImGui new-frame routines.

---

## 4. Technical Details of Custom Additions

### 1. Refined Directional Motion Blur
- **Files**: [motion_blur.cpp](file:///home/angel/OpenAG/cl_dll/motion_blur.cpp) & [motion_blur.h](file:///home/angel/OpenAG/cl_dll/motion_blur.h)
- **Cvars**: `cl_motion_blur`, `cl_motion_blur_shutter`, `cl_motion_blur_alpha`, `cl_motion_blur_chromatic`.
- **Architecture**:
  - Automatically captures the frame buffer into a texture at the end of scene rendering.
  - In the subsequent frame, it calculates the camera's angular velocity vector ($\Delta\text{Pitch}$, $\Delta\text{Yaw}$).
  - Computes 4 offset samples (2 forward, 2 backward) along the velocity vector.
  - Blends these offsets as semi-transparent screen-space overlays (`cl_motion_blur_alpha`) on top of the original unshifted frame.
  - **OpenGL Safety**: Employs attributes pushing/popping (`glPushAttrib(GL_ALL_ATTRIB_BITS)`, `glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS)`) to avoid corrupting engine state.

### 2. Speedometer, Strafe Analyzer & Key Overlay
- **Files**: [hud_strafeguide.h](file:///home/angel/OpenAG/cl_dll/hud_strafeguide.h), [hud_strafeguide.cpp](file:///home/angel/OpenAG/cl_dll/hud_strafeguide.cpp), & [imgui_helper.cpp](file:///home/angel/OpenAG/cl_dll/imgui_helper.cpp)
- **Cvars**: `cl_speedometer`, `cl_speedometer_show_strafes`.
- **Architecture**:
  - `CHudStrafeGuide::Update` acts as the data collector. It reads prediction velocities from `ref_params_s` and computes current horizontal speed and frame acceleration.
  - Calculates mathematically optimal strafe angle $\theta_{opt} = \arccos(30.0 / \text{speed})$.
  - Measures the actual angle between velocity and input direction to categorize player performance:
    - `1 (PERFECT)`: Within $\sim 3^{\circ}$ of $\theta_{opt}$.
    - `2 (TURN FASTER)`: Turning angle is narrower than optimal.
    - `3 (TURN SLOWER)`: Turning angle is wider than optimal.
    - `4 (SPEED LOSS)`: Extremely wide turning causing deceleration.
    - `5 (RELEASE W/S)`: Warning state when pressing forward/back keys in mid-air.
  - Data is stored in a globally visible structure `strafe_data_t g_StrafeData`.
  - ImGui renders this data in a glassmorphic bottom-center HUD panel.

### 3. Viewmodel Position Offsets & FOV Customizations
- **Files**: [view.cpp](file:///home/angel/OpenAG/cl_dll/view.cpp), [StudioModelRenderer.cpp](file:///home/angel/OpenAG/cl_dll/StudioModelRenderer.cpp), & [imgui_helper.cpp](file:///home/angel/OpenAG/cl_dll/imgui_helper.cpp)
- **Cvars**: `cl_viewmodel_ofs_right` (X), `cl_viewmodel_ofs_forward` (Y), `cl_viewmodel_ofs_up` (Z), `cl_viewmodel_fov` (Viewmodel FOV).
- **Architecture**:
  - `V_CalcRefdef` (in `view.cpp`) reads these CVar values and applies them to the viewmodel entity's origin along the view's local coordinate vectors (`pparams->right`, `pparams->forward`, `pparams->up`).
  - `CStudioModelRenderer::SetViewmodelFovProjection` (in `StudioModelRenderer.cpp`) dynamically scales projection matrices for weapon models during rendering when `cl_viewmodel_fov` is non-zero.
  - Added full configuration sliders to the "FOV & Viewmodel Settings" ImGui panel.

### 4. Spawn Points Display System
- **Files**: [spawns.cpp](file:///home/angel/OpenAG/cl_dll/spawns.cpp), [spawns.h](file:///home/angel/OpenAG/cl_dll/spawns.h)
- **Cvars**: `cl_show_spawns`, `cl_show_spawns_only_active`, `cl_show_spawns_dist`.
- **Architecture**:
  - Parses the `.bsp` map file directly from disk upon level load to locate entity lump entries for `info_player_deathmatch`.
  - Dynamically updates active player coordinates to highlight active and cooldown spawn points.
  - Renders 3D boxes/positions at coordinates in the game world utilizing OpenGL drawing pipelines (`gEngfuncs.pTriangleAPI`).

### 5. HL2-style Weapon Sway (Bobbing)
- **Files**: [weapon_sway.cpp](file:///home/angel/OpenAG/cl_dll/weapon_sway.cpp), [weapon_sway.h](file:///home/angel/OpenAG/cl_dll/weapon_sway.h)
- **Cvars**: `cl_weapon_sway`, `cl_weapon_sway_pitch`, `cl_weapon_sway_yaw`, `cl_weapon_sway_roll`, `cl_weapon_sway_clamp`, `cl_weapon_sway_roll_factor`, `cl_weapon_sway_pos_yaw`, `cl_weapon_sway_pos_pitch`.
- **Architecture**:
  - Interpolates and smooths weapon model alignment angles relative to the player's view rotation rates.
  - Simulates dynamic weapon drag, roll rotation, and sway displacement.

### 6. AG Online Player Count Overlay
- **Files**: [imgui_helper.cpp](file:///home/angel/OpenAG/cl_dll/imgui_helper.cpp)
- **Architecture**:
  - Designed to run when the player is in the main menu (`g_pGameUI->IsGameUIVisible()`).
  - Triggers a background server query via `g_pServers->RequestList()` every 30 seconds.
  - Loops over the returned server list, parsing the `"current"` key from each server's info string, and displays the sum in a sleek top-right overlay window.

---

## 5. Development & Verification Guide

### Building
The project compiles via CMake. On Linux, run from the root:
```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Deployment (Copy Path)
The compiled binary `client.so` must be copied to the mod's `cl_dlls` directory:
```bash
cp build/client.so /mnt/Trash/@home/angeloo123/Games/steamapps/common/Half-Life/ag/cl_dlls/
```

### Coding Rules for AI Agents
1. **Preserve Engine State**: GoldSrc uses a single-threaded OpenGL pipeline. Whenever writing raw OpenGL commands (like inside `motion_blur.cpp`), always push current state matrices and client attributes, and pop them immediately after finishing.
2. **CVar Lookups**: All CVars registered by other files must be accessed using `gEngfuncs.pfnGetCvarPointer("cvar_name")`. Ensure pointers are null-checked before accessing `->value` or `->string`.
3. **ImGui HUD Rendering**: All HUD overlays (speedometer, online player count, health/armor) must use `hud_flags` containing `ImGuiWindowFlags_NoInputs` to prevent blocking mouse look and movements in-game.
