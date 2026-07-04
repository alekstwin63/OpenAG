void SDL_SetRelativeMouseMode(void* x) {}
void SDL_GetRelativeMouseState(int* x, int* y) {}
int SDL_NumJoysticks(void) { return 0; }
int SDL_IsGameController(int x) { return 0; }
void* SDL_GameControllerOpen(int x) { return 0; }
const char* SDL_GameControllerName(void* x) { return ""; }
int SDL_GameControllerGetAxis(void* x, int y) { return 0; }
int SDL_GameControllerGetButton(void* x, int y) { return 0; }
void SDL_JoystickUpdate(void) {}
