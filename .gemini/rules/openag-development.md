# OpenAG Development & ImGui Integration Rules

When developing or integrating Dear ImGui/GUI components inside the OpenAG codebase, enforce the following constraints:

### 1. SDL2 Version Compatibility (SDL 2.0.0)
The local SDK headers in `external/SDL2/` are version 2.0.0. To avoid compilation and linking errors in SDL2-related backends:
- Do NOT call functions introduced after SDL 2.0.0 (like `SDL_GL_GetCurrentWindow`, `SDL_GL_GetCurrentContext`, `SDL_GetGlobalMouseState`, `SDL_GL_GetDrawableSize`, `SDL_GetRendererOutputSize`) without providing backward-compatibility wrappers.
- To retrieve the active `SDL_Window*`, loop through potential window IDs using `SDL_GetWindowFromID(i)` (up to 32) and select the first valid window.
- When initializing the ImGui SDL2 backend, pass `nullptr` for the `SDL_GLContext` since the backend does not actively use it.
- If integrating external code that relies on newer SDL2 APIs, define conditional wrappers at the top of the compilation unit:
  ```cpp
  #if !SDL_VERSION_ATLEAST(2,0,4)
  extern "C" {
      Uint32 SDLCALL SDL_GetGlobalMouseState(int *x, int *y) { return SDL_GetMouseState(x, y); }
      SDL_bool SDLCALL SDL_CaptureMouse(SDL_bool enabled) { return SDL_FALSE; }
  }
  #endif
  ```

### 2. ImGui Menu Interception
- Integrate the ImGui rendering logic via `cl_dll/imgui_helper.cpp` inside `HUD_VidInit`, `HUD_Redraw`, and `HUD_Key_Event`.
- When the ImGui menu is active, use `gEngfuncs.pfnSetMouseEnable(true)` and `SDL_ShowCursor(SDL_ENABLE)` to show the cursor, and block keyboard inputs by returning `0` in `HUD_Key_Event`.
