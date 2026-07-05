#include "imgui_helper.h"
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_opengl2.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include "hud.h"
#include "cl_util.h"
#include "keydefs.h"
#include "in_buttons.h"
#include "ammo.h"
#include "ammohistory.h"
#include "gameui.h"
#include "hud_servers_priv.h"

extern CHudServers *g_pServers;

extern "C" void HUD_UpdateCursorState();

unsigned int g_dwKeyPressButtons = 0;
static cvar_t* cl_keypress_overlay = nullptr;
cvar_t* cl_custom_hud = nullptr;
cvar_t* cl_custom_menu = nullptr;

bool g_ShowImGuiMenu = false;
bool g_ShowCrosshairEditor = false;
bool g_ShowAGSettings = false;
bool g_ShowPauseMenu = false;
bool g_ShowMainMenu = false;
bool g_ShowServerBrowser = false;
bool g_IsBackgroundMap = true;

static const char* g_BackgroundMapName = "maps/echo.bsp";

static bool g_ImGuiInitialized = false;
static bool g_EventWatchRegistered = false;

static int SDLCALL ImGui_SDLEventWatch(void* userdata, SDL_Event* event)
{
	if (g_ImGuiInitialized && (g_ShowImGuiMenu || g_ShowCrosshairEditor || g_ShowAGSettings || g_ShowPauseMenu || g_ShowMainMenu || g_ShowServerBrowser))
	{
		ImGui_ImplSDL2_ProcessEvent(event);
	}
	return 0;
}

void ImGuiHelper_UpdateInputState()
{
	bool anyVisible = g_ShowImGuiMenu || g_ShowCrosshairEditor || g_ShowAGSettings || g_ShowPauseMenu || g_ShowMainMenu || g_ShowServerBrowser;
	gEngfuncs.pfnSetMouseEnable(anyVisible);
	SDL_ShowCursor(anyVisible ? SDL_ENABLE : SDL_DISABLE);
	HUD_UpdateCursorState();
}

void ToggleImGuiMenu_Callback()
{
	g_ShowImGuiMenu = !g_ShowImGuiMenu;
	ImGuiHelper_UpdateInputState();
}

void ImGuiHelper_Init()
{
	gEngfuncs.pfnAddCommand("toggle_imgui", ToggleImGuiMenu_Callback);
	cl_keypress_overlay = gEngfuncs.pfnRegisterVariable("cl_keypress_overlay", "0", 1); // FCVAR_ARCHIVE is 1
	cl_custom_hud = gEngfuncs.pfnRegisterVariable("cl_custom_hud", "1", 1);
	cl_custom_menu = gEngfuncs.pfnRegisterVariable("cl_custom_menu", "1", 1);
}

void ImGuiHelper_VidInit()
{
	if (g_ImGuiInitialized)
	{
		ImGui_ImplOpenGL2_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		g_ImGuiInitialized = false;
	}

	SDL_Window* window = nullptr;
	for (Uint32 i = 1; i < 32; ++i)
	{
		window = SDL_GetWindowFromID(i);
		if (window != nullptr)
			break;
	}

	if (!window)
	{
		gEngfuncs.Con_Printf("ImGuiHelper: Failed to get SDL Window\n");
		return;
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// Use premium dark style
	ImGui::StyleColorsDark();
	
	        // Custom styling for premium aesthetic (deep space dark mode)
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Rounding & Spacing
        style.WindowRounding = 10.0f;
        style.FrameRounding = 5.0f;
        style.PopupRounding = 6.0f;
        style.GrabRounding = 4.0f;
        style.ScrollbarRounding = 8.0f;
        
        style.WindowPadding = ImVec2(16.0f, 16.0f);
        style.FramePadding = ImVec2(10.0f, 6.0f);
        style.ItemSpacing = ImVec2(12.0f, 8.0f);
        style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
        style.ScrollbarSize = 8.0f;
        style.GrabMinSize = 10.0f;
        
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;
        
        // Colors
        style.Colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.00f);
        
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.94f);
        style.Colors[ImGuiCol_ChildBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.00f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.98f);
        
        style.Colors[ImGuiCol_Border] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
        style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
        style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
        style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
        
        style.Colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
        style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.06f, 0.06f, 0.07f, 0.50f);
        
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.06f, 0.07f, 0.50f);
        style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
        style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.45f, 1.00f);
        
        // Active purple-blue accent
        style.Colors[ImGuiCol_CheckMark] = ImVec4(0.38f, 0.45f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.38f, 0.45f, 0.98f, 1.00f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.52f, 1.00f, 1.00f);
        
        // Button
        style.Colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.45f, 0.98f, 0.85f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.38f, 0.45f, 0.98f, 1.00f);
        
        // Header
        style.Colors[ImGuiCol_Header] = ImVec4(0.12f, 0.12f, 0.16f, 1.00f);
        style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.45f, 0.98f, 0.80f);
        style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.38f, 0.45f, 0.98f, 1.00f);
        
        style.Colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.16f, 0.20f, 1.00f);
        style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
        style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.38f, 0.45f, 0.98f, 1.00f);
        
        style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.20f, 0.25f, 0.20f);
        style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.38f, 0.45f, 0.98f, 0.67f);
        style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.38f, 0.45f, 0.98f, 0.95f);
        
        style.Colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.10f, 0.13f, 1.00f);
        style.Colors[ImGuiCol_TabHovered] = ImVec4(0.38f, 0.45f, 0.98f, 0.80f);
        style.Colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
        style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.08f, 0.10f, 0.97f);
        style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);

	ImGui_ImplSDL2_InitForOpenGL(window, nullptr);
	ImGui_ImplOpenGL2_Init();

	if (!g_EventWatchRegistered)
	{
		SDL_AddEventWatch(ImGui_SDLEventWatch, nullptr);
		g_EventWatchRegistered = true;
	}

	g_ImGuiInitialized = true;
	gEngfuncs.Con_Printf("ImGuiHelper: Successfully initialized Dear ImGui (OpenGL2 + SDL2)\n");
}

void ImGuiHelper_Shutdown()
{
	if (g_ImGuiInitialized)
	{
		if (g_EventWatchRegistered)
		{
			SDL_DelEventWatch(ImGui_SDLEventWatch, nullptr);
			g_EventWatchRegistered = false;
		}
		ImGui_ImplOpenGL2_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();
		g_ImGuiInitialized = false;
	}
}

static void GetInfoValue(const char* info, const char* key, char* out, size_t outSize)
{
	out[0] = '\0';
	if (!info || !key) return;
	char searchKey[128];
	snprintf(searchKey, sizeof(searchKey), "\\%s\\", key);
	const char* p = strstr(info, searchKey);
	if (p)
	{
		p += strlen(searchKey);
		const char* end = strchr(p, '\\');
		size_t len = end ? (end - p) : strlen(p);
		if (len >= outSize) len = outSize - 1;
		strncpy(out, p, len);
		out[len] = '\0';
	}
}

static bool CaseInsensitiveContains(const char* haystack, const char* needle)
{
	if (!haystack || !needle) return false;
	if (needle[0] == '\0') return true;

	size_t hLen = haystack ? strlen(haystack) : 0;
	size_t nLen = needle ? strlen(needle) : 0;
	if (hLen < nLen) return false;

	for (size_t i = 0; i <= hLen - nLen; ++i)
	{
		bool match = true;
		for (size_t j = 0; j < nLen; ++j)
		{
			char hc = haystack[i + j];
			char nc = needle[j];
			if (hc >= 'A' && hc <= 'Z') hc += 32;
			if (nc >= 'A' && nc <= 'Z') nc += 32;
			if (hc != nc)
			{
				match = false;
				break;
			}
		}
		if (match) return true;
	}
	return false;
}

void ImGuiHelper_Draw()
{
	if (!g_ImGuiInitialized)
		return;

	const char* mapName = gEngfuncs.pfnGetLevelName();
	bool hasMap = (mapName && mapName[0] != '\0');
	bool customMenuEnabled = cl_custom_menu ? (cl_custom_menu->value != 0.0f) : true;

	// Check if we're on the background map used for main menu rendering
	bool onBackgroundMap = false;
	if (hasMap && g_BackgroundMapName)
	{
		onBackgroundMap = (strcmp(mapName, g_BackgroundMapName) == 0);
	}
	g_IsBackgroundMap = onBackgroundMap;

	// Determine effective "in game" state
	bool inGame = hasMap && !onBackgroundMap;

	if ((onBackgroundMap || !hasMap) && customMenuEnabled)
	{
		g_ShowMainMenu = true;
		g_ShowPauseMenu = false;
	}
	else
	{
		g_ShowMainMenu = false;
	}

	static bool lastAnyVisible = false;
	bool anyVisible = g_ShowImGuiMenu || g_ShowCrosshairEditor || g_ShowAGSettings || g_ShowPauseMenu || g_ShowMainMenu || g_ShowServerBrowser;
	if (anyVisible != lastAnyVisible)
	{
		ImGuiHelper_UpdateInputState();
		lastAnyVisible = anyVisible;
	}

	ImGui_ImplOpenGL2_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();

	// 1. Fullscreen Main Menu (when disconnected)
	if (g_ShowMainMenu)
	{
		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
		ImGui::SetNextWindowSize(ImVec2((float)ScreenWidth, (float)ScreenHeight));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));

		ImGui::Begin("Main Menu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

		// Left-aligned Title "OPENAG"
		ImGui::SetCursorPos(ImVec2(80.0f, (float)ScreenHeight * 0.30f));
		ImGui::SetWindowFontScale(3.5f);
		ImGui::TextColored(ImVec4(0.38f, 0.45f, 0.98f, 1.00f), "OPENAG");
		ImGui::SetWindowFontScale(1.0f);

		// Subtitle
		ImGui::SetCursorPos(ImVec2(80.0f, (float)ScreenHeight * 0.30f + 65.0f));
		ImGui::SetWindowFontScale(1.2f);
		const char* sub = "Modern Dear ImGui Interface";
		ImGui::TextDisabled("%s", sub);
		ImGui::SetWindowFontScale(1.0f);

		// Navigation buttons (Left-aligned)
		float btn_w = 240.0f;
		float start_x = 80.0f;

		ImGui::SetCursorPos(ImVec2(start_x, (float)ScreenHeight * 0.30f + 120.0f));
		if (ImGui::Button("FIND SERVERS", ImVec2(btn_w, 42)))
		{
			g_ShowServerBrowser = true;
			if (g_pServers)
			{
				g_pServers->RequestList();
			}
		}

		ImGui::Spacing();
		ImGui::SetCursorPosX(start_x);
		if (ImGui::Button("AG SETTINGS", ImVec2(btn_w, 42)))
		{
			g_ShowAGSettings = true;
		}

		ImGui::Spacing();
		ImGui::SetCursorPosX(start_x);
		if (ImGui::Button("CROSSHAIR EDITOR", ImVec2(btn_w, 42)))
		{
			g_ShowCrosshairEditor = true;
		}

		ImGui::Spacing();
		ImGui::SetCursorPosX(start_x);
		if (ImGui::Button("QUIT GAME", ImVec2(btn_w, 42)))
		{
			gEngfuncs.pfnClientCmd("quit\n");
		}

		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);
	}

	// 2. Pause Menu Overlay (when in-game and g_ShowPauseMenu is true)
	if (g_ShowPauseMenu && inGame)
	{
		ImGui::SetNextWindowPos(ImVec2((float)ScreenWidth * 0.5f - 140.0f, (float)ScreenHeight * 0.5f - 170.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(280, 330));
		if (ImGui::Begin("Game Paused", &g_ShowPauseMenu, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
		{
			float btn_w = 248.0f;
			ImGui::Spacing();
			
			if (ImGui::Button("RESUME GAME", ImVec2(btn_w, 42)))
			{
				g_ShowPauseMenu = false;
				ImGuiHelper_UpdateInputState();
			}

			ImGui::Spacing();
			if (ImGui::Button("DISCONNECT", ImVec2(btn_w, 42)))
			{
				gEngfuncs.pfnClientCmd("disconnect; map echo\n");
				g_ShowPauseMenu = false;
			}

			ImGui::Spacing();
			if (ImGui::Button("AG SETTINGS", ImVec2(btn_w, 42)))
			{
				g_ShowAGSettings = true;
				g_ShowPauseMenu = false;
			}

			ImGui::Spacing();
			if (ImGui::Button("CROSSHAIR EDITOR", ImVec2(btn_w, 42)))
			{
				g_ShowCrosshairEditor = true;
				g_ShowPauseMenu = false;
			}

			ImGui::Spacing();
			if (ImGui::Button("QUIT TO DESKTOP", ImVec2(btn_w, 42)))
			{
				gEngfuncs.pfnClientCmd("quit\n");
			}
		}
		ImGui::End();
	}

	// 3. Custom HUD Overlay (when in-game and cl_custom_hud is enabled)
	bool customHudEnabled = cl_custom_hud ? (cl_custom_hud->value != 0.0f) : true;
	if (inGame && customHudEnabled && gHUD.m_Health.m_iHealth > 0 && !gHUD.m_iIntermission)
	{
		ImGuiWindowFlags hud_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
		
		// Bottom-Left Health & Armor panel
		ImGui::SetNextWindowPos(ImVec2(24.0f, (float)ScreenHeight - 110.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(240, 90));
		if (ImGui::Begin("HUD HealthArmor", nullptr, hud_flags))
		{
			// Render background plate manually for clean glassmorphism look
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			ImVec2 p_min = ImGui::GetWindowPos();
			ImVec2 p_max = ImVec2(p_min.x + 240.0f, p_min.y + 82.0f);
			draw_list->AddRectFilled(p_min, p_max, ImColor(12, 12, 14, 180), 8.0f);
			draw_list->AddRect(p_min, p_max, ImColor(40, 40, 48, 120), 8.0f, 0, 1.0f);

			ImGui::SetCursorPos(ImVec2(15.0f, 12.0f));

			int health = max(0, gHUD.m_Health.m_iHealth);
			int armor = max(0, gHUD.m_Battery.GetBattery());

			// Health bar
			char health_label[64];
			std::snprintf(health_label, sizeof(health_label), "HEALTH: %d", health);
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.20f, 0.85f, 0.40f, 0.90f)); // Mint green
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 0.60f));
			ImGui::ProgressBar(min(1.0f, health / 100.0f), ImVec2(210, 22), health_label);
			ImGui::PopStyleColor(2);

			ImGui::SetCursorPosX(15.0f);
			ImGui::Spacing();

			// Armor bar
			ImGui::SetCursorPosX(15.0f);
			char armor_label[64];
			std::snprintf(armor_label, sizeof(armor_label), "ARMOR: %d", armor);
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.20f, 0.60f, 1.00f, 0.90f)); // Electric blue
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 0.60f));
			ImGui::ProgressBar(min(1.0f, armor / 100.0f), ImVec2(210, 22), armor_label);
			ImGui::PopStyleColor(2);
		}
		ImGui::End();

		// Bottom-Right Ammo & Active Weapon panel
		ImGui::SetNextWindowPos(ImVec2((float)ScreenWidth - 264.0f, (float)ScreenHeight - 110.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(240, 90));
		if (ImGui::Begin("HUD AmmoWeapon", nullptr, hud_flags))
		{
			WEAPON* pw = gHUD.m_Ammo.GetActiveWeapon();
			if (pw)
			{
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				ImVec2 p_min = ImGui::GetWindowPos();
				ImVec2 p_max = ImVec2(p_min.x + 240.0f, p_min.y + 82.0f);
				draw_list->AddRectFilled(p_min, p_max, ImColor(12, 12, 14, 180), 8.0f);
				draw_list->AddRect(p_min, p_max, ImColor(40, 40, 48, 120), 8.0f, 0, 1.0f);

				auto CleanWeaponName = [](const char* rawName) -> const char* {
					if (!rawName) return "";
					if (strncmp(rawName, "weapon_", 7) == 0)
						return rawName + 7;
					return rawName;
				};

				// Drawing Weapon Name
				ImGui::SetCursorPos(ImVec2(15.0f, 12.0f));
				ImGui::SetWindowFontScale(1.1f);
				ImGui::TextColored(ImVec4(0.38f, 0.45f, 0.98f, 1.00f), "%s", CleanWeaponName(pw->szName));
				ImGui::SetWindowFontScale(1.0f);

				// Drawing Ammo counts
				int clip = pw->iClip;
				int reserve = gWR.CountAmmo(pw->iAmmoType);
				char ammo_label[64];
				if (pw->iAmmoType < 0)
				{
					std::snprintf(ammo_label, sizeof(ammo_label), "--");
				}
				else if (clip < 0)
				{
					std::snprintf(ammo_label, sizeof(ammo_label), "%d", reserve);
				}
				else
				{
					std::snprintf(ammo_label, sizeof(ammo_label), "%d / %d", clip, reserve);
				}

				ImGui::SetCursorPos(ImVec2(15.0f, 38.0f));
				ImGui::SetWindowFontScale(1.6f);
				ImGui::Text("%s", ammo_label);
				ImGui::SetWindowFontScale(1.0f);
			}
		}
		ImGui::End();
	}

	if (g_ShowImGuiMenu)
	{
		ImGui::SetNextWindowSize(ImVec2(500, 350), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("OpenAG - Dear ImGui Menu", &g_ShowImGuiMenu, ImGuiWindowFlags_NoCollapse))
		{
			ImGui::Text("Welcome to the modern OpenAG Settings UI!");
			ImGui::Separator();

			static bool autoBhop = false;
			ImGui::Checkbox("Enable Auto-Bhop (sv_autobhop)", &autoBhop);

			static float airAccelerate = 10.0f;
			ImGui::SliderFloat("Air Accelerate (sv_airaccelerate)", &airAccelerate, 1.0f, 100.0f);

			static int maxFps = 100;
			ImGui::SliderInt("Max FPS (fps_max)", &maxFps, 60, 500);

			ImGui::Separator();
			if (ImGui::Button("Close Menu"))
			{
				ToggleImGuiMenu_Callback();
			}
		}
		ImGui::End();
	}

	if (g_ShowCrosshairEditor)
	{
		ImGui::SetNextWindowSize(ImVec2(350, 480), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Crosshair Editor", &g_ShowCrosshairEditor, ImGuiWindowFlags_NoCollapse))
		{
			cvar_t* pCvarCross = gEngfuncs.pfnGetCvarPointer("cl_cross");
			bool enableCross = pCvarCross ? (pCvarCross->value != 0.0f) : false;
			if (ImGui::Checkbox("Enable Crosshair", &enableCross))
			{
				gEngfuncs.Cvar_SetValue("cl_cross", enableCross ? 1.0f : 0.0f);
			}

			ImGui::Separator();

			cvar_t* pCvarSize = gEngfuncs.pfnGetCvarPointer("cl_cross_size");
			int sizeVal = pCvarSize ? (int)pCvarSize->value : 10;
			if (ImGui::SliderInt("Size", &sizeVal, 0, 100))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_size", (float)sizeVal);
			}

			cvar_t* pCvarThickness = gEngfuncs.pfnGetCvarPointer("cl_cross_thickness");
			int thicknessVal = pCvarThickness ? (int)pCvarThickness->value : 2;
			if (ImGui::SliderInt("Thickness", &thicknessVal, 1, 20))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_thickness", (float)thicknessVal);
			}

			cvar_t* pCvarGap = gEngfuncs.pfnGetCvarPointer("cl_cross_gap");
			int gapVal = pCvarGap ? (int)pCvarGap->value : 3;
			if (ImGui::SliderInt("Gap", &gapVal, -20, 50))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_gap", (float)gapVal);
			}

			cvar_t* pCvarOutline = gEngfuncs.pfnGetCvarPointer("cl_cross_outline");
			int outlineVal = pCvarOutline ? (int)pCvarOutline->value : 0;
			if (ImGui::SliderInt("Outline", &outlineVal, 0, 10))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_outline", (float)outlineVal);
			}

			cvar_t* pCvarDotSize = gEngfuncs.pfnGetCvarPointer("cl_cross_dot_size");
			int dotSizeVal = pCvarDotSize ? (int)pCvarDotSize->value : 0;
			if (ImGui::SliderInt("Dot Size", &dotSizeVal, 0, 50))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_dot_size", (float)dotSizeVal);
			}

			cvar_t* pCvarCircleRadius = gEngfuncs.pfnGetCvarPointer("cl_cross_circle_radius");
			int circleRadiusVal = pCvarCircleRadius ? (int)pCvarCircleRadius->value : 0;
			if (ImGui::SliderInt("Circle Radius", &circleRadiusVal, 0, 100))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_circle_radius", (float)circleRadiusVal);
			}

			cvar_t* pCvarAlpha = gEngfuncs.pfnGetCvarPointer("cl_cross_alpha");
			int alphaVal = pCvarAlpha ? (int)pCvarAlpha->value : 200;
			if (ImGui::SliderInt("Alpha", &alphaVal, 0, 255))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_alpha", (float)alphaVal);
			}

			ImGui::Separator();
			ImGui::Text("Crosshair Color");

			cvar_t* pCvarColor = gEngfuncs.pfnGetCvarPointer("cl_cross_color");
			int r = 0, g = 255, b = 0;
			if (pCvarColor && pCvarColor->string)
			{
				std::sscanf(pCvarColor->string, "%d %d %d", &r, &g, &b);
			}
			
			float color[3] = { r / 255.0f, g / 255.0f, b / 255.0f };
			if (ImGui::ColorEdit3("Color", color))
			{
				r = (int)(color[0] * 255.0f + 0.5f);
				g = (int)(color[1] * 255.0f + 0.5f);
				b = (int)(color[2] * 255.0f + 0.5f);
				char cmd[128];
				std::snprintf(cmd, sizeof(cmd), "cl_cross_color \"%d %d %d\"\n", r, g, b);
				gEngfuncs.pfnClientCmd(cmd);
			}

			ImGui::Separator();
			ImGui::Text("Draw Lines");

			cvar_t* pTop = gEngfuncs.pfnGetCvarPointer("cl_cross_top_line");
			cvar_t* pBottom = gEngfuncs.pfnGetCvarPointer("cl_cross_bottom_line");
			cvar_t* pLeft = gEngfuncs.pfnGetCvarPointer("cl_cross_left_line");
			cvar_t* pRight = gEngfuncs.pfnGetCvarPointer("cl_cross_right_line");

			bool topVal = pTop ? (pTop->value != 0.0f) : true;
			bool bottomVal = pBottom ? (pBottom->value != 0.0f) : true;
			bool leftVal = pLeft ? (pLeft->value != 0.0f) : true;
			bool rightVal = pRight ? (pRight->value != 0.0f) : true;

			if (ImGui::Checkbox("Top", &topVal))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_top_line", topVal ? 1.0f : 0.0f);
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Bottom", &bottomVal))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_bottom_line", bottomVal ? 1.0f : 0.0f);
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Left", &leftVal))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_left_line", leftVal ? 1.0f : 0.0f);
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Right", &rightVal))
			{
				gEngfuncs.Cvar_SetValue("cl_cross_right_line", rightVal ? 1.0f : 0.0f);
			}

			ImGui::Separator();
			if (ImGui::Button("Close", ImVec2(-FLT_MIN, 0)))
			{
				g_ShowCrosshairEditor = false;
				ImGuiHelper_UpdateInputState();
			}
		}
		ImGui::End();

		if (!g_ShowCrosshairEditor)
		{
			ImGuiHelper_UpdateInputState();
		}
	}

	if (g_ShowAGSettings)
	{
		ImGui::SetNextWindowSize(ImVec2(350, 320), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("AG Settings", &g_ShowAGSettings, ImGuiWindowFlags_NoCollapse))
		{
			cvar_t* pTeammate = gEngfuncs.pfnGetCvarPointer("cl_forceteammate_enable");
			bool teammateVal = pTeammate ? (pTeammate->value != 0.0f) : true;
			if (ImGui::Checkbox("Force Teammate Models", &teammateVal))
			{
				gEngfuncs.Cvar_SetValue("cl_forceteammate_enable", teammateVal ? 1.0f : 0.0f);
				gEngfuncs.pfnClientCmd("cl_forceteammodel_list\n");
			}

			cvar_t* pEnemy = gEngfuncs.pfnGetCvarPointer("cl_forceenemy_enable");
			bool enemyVal = pEnemy ? (pEnemy->value != 0.0f) : true;
			if (ImGui::Checkbox("Force Enemy Models", &enemyVal))
			{
				gEngfuncs.Cvar_SetValue("cl_forceenemy_enable", enemyVal ? 1.0f : 0.0f);
				gEngfuncs.pfnClientCmd("cl_forcemodel_list\n");
			}

			cvar_t* pExplosions = gEngfuncs.pfnGetCvarPointer("cl_explosions_enable");
			bool explosionsVal = pExplosions ? (pExplosions->value != 0.0f) : true;
			if (ImGui::Checkbox("Enable Explosion Effects", &explosionsVal))
			{
				gEngfuncs.Cvar_SetValue("cl_explosions_enable", explosionsVal ? 1.0f : 0.0f);
			}

			cvar_t* pDiscord = gEngfuncs.pfnGetCvarPointer("cl_discord_rpc");
			bool discordVal = pDiscord ? (pDiscord->value != 0.0f) : true;
			if (ImGui::Checkbox("Discord Rich Presence", &discordVal))
			{
				gEngfuncs.Cvar_SetValue("cl_discord_rpc", discordVal ? 1.0f : 0.0f);
			}

			cvar_t* pTimers = gEngfuncs.pfnGetCvarPointer("cl_item_timers");
			bool timersVal = pTimers ? (pTimers->value != 0.0f) : true;
			if (ImGui::Checkbox("Item Spawn Timers", &timersVal))
			{
				gEngfuncs.Cvar_SetValue("cl_item_timers", timersVal ? 1.0f : 0.0f);
			}

			cvar_t* pDamage = gEngfuncs.pfnGetCvarPointer("cl_damage_numbers");
			bool damageVal = pDamage ? (pDamage->value != 0.0f) : true;
			if (ImGui::Checkbox("Show Damage Numbers", &damageVal))
			{
				gEngfuncs.Cvar_SetValue("cl_damage_numbers", damageVal ? 1.0f : 0.0f);
			}

			bool keypressVal = cl_keypress_overlay ? (cl_keypress_overlay->value != 0.0f) : false;
			if (ImGui::Checkbox("Keypress Overlay", &keypressVal))
			{
				gEngfuncs.Cvar_SetValue("cl_keypress_overlay", keypressVal ? 1.0f : 0.0f);
			}

			bool customHudVal = cl_custom_hud ? (cl_custom_hud->value != 0.0f) : true;
			if (ImGui::Checkbox("Custom ImGui HUD", &customHudVal))
			{
				gEngfuncs.Cvar_SetValue("cl_custom_hud", customHudVal ? 1.0f : 0.0f);
			}

			bool customMenuVal = cl_custom_menu ? (cl_custom_menu->value != 0.0f) : true;
			if (ImGui::Checkbox("Custom ImGui Menus", &customMenuVal))
			{
				gEngfuncs.Cvar_SetValue("cl_custom_menu", customMenuVal ? 1.0f : 0.0f);
			}

			ImGui::Separator();

			cvar_t* pFootstep = gEngfuncs.pfnGetCvarPointer("cl_footstep_volume");
			int footstepVal = pFootstep ? (int)(pFootstep->value * 100.0f) : 100;
			if (ImGui::SliderInt("Footstep Sound Volume", &footstepVal, 0, 200, "%d%%"))
			{
				gEngfuncs.Cvar_SetValue("cl_footstep_volume", (float)footstepVal / 100.0f);
			}

			ImGui::Separator();
			if (ImGui::Button("Close", ImVec2(-FLT_MIN, 0)))
			{
				g_ShowAGSettings = false;
				ImGuiHelper_UpdateInputState();
			}
		}
		ImGui::End();

		if (!g_ShowAGSettings)
		{
			ImGuiHelper_UpdateInputState();
		}
	}

	if (cl_keypress_overlay && cl_keypress_overlay->value != 0.0f)
	{
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings;
		bool interactive = g_ShowImGuiMenu || g_ShowCrosshairEditor || g_ShowAGSettings;
		if (!interactive)
		{
			flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs;
		}

		ImGui::SetNextWindowSize(ImVec2(190, 150), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Keypress Overlay", nullptr, flags))
		{
			auto DrawKey = [](const char* label, bool pressed, ImVec2 size) {
				ImVec4 col_bg = pressed ? ImVec4(0.38f, 0.45f, 0.98f, 1.00f) : ImVec4(0.12f, 0.12f, 0.16f, 0.60f);
				ImVec4 col_text = pressed ? ImVec4(1.00f, 1.00f, 1.00f, 1.00f) : ImVec4(0.60f, 0.60f, 0.65f, 0.80f);
				ImVec4 col_border = pressed ? ImVec4(0.45f, 0.52f, 1.00f, 1.00f) : ImVec4(0.20f, 0.20f, 0.25f, 0.40f);

				ImGui::PushStyleColor(ImGuiCol_Button, col_bg);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col_bg);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, col_bg);
				ImGui::PushStyleColor(ImGuiCol_Text, col_text);
				ImGui::PushStyleColor(ImGuiCol_Border, col_border);
				
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
				ImGui::Button(label, size);
				ImGui::PopStyleVar();
				
				ImGui::PopStyleColor(5);
			};

			bool w = (g_dwKeyPressButtons & IN_FORWARD) != 0;
			bool a = (g_dwKeyPressButtons & IN_MOVELEFT) != 0;
			bool s = (g_dwKeyPressButtons & IN_BACK) != 0;
			bool d = (g_dwKeyPressButtons & IN_MOVERIGHT) != 0;
			bool lmb = (g_dwKeyPressButtons & IN_ATTACK) != 0;
			bool rmb = (g_dwKeyPressButtons & IN_ATTACK2) != 0;
			bool space = (g_dwKeyPressButtons & IN_JUMP) != 0;
			bool ctrl = (g_dwKeyPressButtons & IN_DUCK) != 0;

			// Row 1: LMB [W] RMB
			DrawKey("LMB", lmb, ImVec2(50, 30));
			ImGui::SameLine();
			DrawKey("W", w, ImVec2(50, 30));
			ImGui::SameLine();
			DrawKey("RMB", rmb, ImVec2(50, 30));

			// Row 2: [A] [S] [D]
			DrawKey("A", a, ImVec2(50, 30));
			ImGui::SameLine();
			DrawKey("S", s, ImVec2(50, 30));
			ImGui::SameLine();
			DrawKey("D", d, ImVec2(50, 30));

			// Row 3: [Ctrl] [Space]
			DrawKey("Ctrl", ctrl, ImVec2(77, 30));
			ImGui::SameLine();
			DrawKey("Space", space, ImVec2(77, 30));
		}
		ImGui::End();
	}

	if (g_ShowServerBrowser)
	{
		ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2((float)ScreenWidth * 0.1f, (float)ScreenHeight * 0.1f), ImGuiCond_FirstUseEver);

		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.95f)); // Dark glass-like background
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

		if (ImGui::Begin("Server Browser", &g_ShowServerBrowser, ImGuiWindowFlags_NoCollapse))
		{
			static int activeTab = 0; // 0 = Internet, 1 = LAN

			// Refresh button, loading indicator, search filter
			if (ImGui::Button("REFRESH ALL"))
			{
				if (g_pServers)
				{
					if (activeTab == 0)
						g_pServers->RequestList();
					else
						g_pServers->RequestBroadcastList(1);
				}
			}

			ImGui::SameLine();
			if (g_pServers && g_pServers->IsRequesting())
			{
				ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "[ Refreshing servers... ]");
			}
			else
			{
				ImGui::TextDisabled("Idle");
			}

			// Local filter
			ImGui::SameLine(ImGui::GetWindowWidth() - 320.0f);
			ImGui::SetNextItemWidth(300.0f);
			static char searchFilter[128] = "";
			ImGui::InputTextWithHint("##Filter", "Search by server name or map...", searchFilter, sizeof(searchFilter));

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Tabs
			if (ImGui::Button("INTERNET", ImVec2(120, 30)))
			{
				if (activeTab != 0)
				{
					activeTab = 0;
					if (g_pServers) g_pServers->RequestList();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("LAN / BROADCAST", ImVec2(150, 30)))
			{
				if (activeTab != 1)
				{
					activeTab = 1;
					if (g_pServers) g_pServers->RequestBroadcastList(1);
				}
			}

			ImGui::Spacing();

			// Server list table
			if (ImGui::BeginTable("ServerTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, -50)))
			{
				ImGui::TableSetupColumn("Server Name", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthFixed, 150.0f);
				ImGui::TableSetupColumn("Players", ImGuiTableColumnFlags_WidthFixed, 80.0f);
				ImGui::TableSetupColumn("Ping", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 180.0f);
				ImGui::TableHeadersRow();

				if (g_pServers)
				{
					CHudServers::server_t* list = g_pServers->GetServersList();
					int index = 0;
					while (list)
					{
						char name[128] = "";
						char map[64] = "";
						char current[16] = "0";
						char maxPlayers[16] = "32";
						char address[128] = "";

						GetInfoValue(list->info, "hostname", name, sizeof(name));
						GetInfoValue(list->info, "map", map, sizeof(map));
						GetInfoValue(list->info, "current", current, sizeof(current));
						GetInfoValue(list->info, "max", maxPlayers, sizeof(maxPlayers));
						GetInfoValue(list->info, "address", address, sizeof(address));

						// Fallback to list->remote_address if address in info is empty
						if (address[0] == '\0')
						{
							strncpy(address, gEngfuncs.pNetAPI->AdrToString(&list->remote_address), sizeof(address));
						}

						// Filter search
						bool match = true;
						if (searchFilter[0] != '\0')
						{
							match = (CaseInsensitiveContains(name, searchFilter) || CaseInsensitiveContains(map, searchFilter));
						}

						if (match)
						{
							ImGui::TableNextRow();

							// Name
							ImGui::TableNextColumn();
							char label[256];
							snprintf(label, sizeof(label), "%s##row_%d", name[0] ? name : "Unnamed Server", index);

							bool selected = false;
							if (ImGui::Selectable(label, &selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
							{
								if (ImGui::IsMouseDoubleClicked(0))
								{
									// Connect to server!
									char cmd[256];
									snprintf(cmd, sizeof(cmd), "connect %s\n", address);
									gEngfuncs.pfnClientCmd(cmd);
									g_ShowServerBrowser = false;
								}
							}

							// Map
							ImGui::TableNextColumn();
							ImGui::Text("%s", map[0] ? map : "unknown");

							// Players
							ImGui::TableNextColumn();
							ImGui::Text("%s / %s", current, maxPlayers);

							// Ping
							ImGui::TableNextColumn();
							ImGui::Text("%d ms", list->ping);

							// Address
							ImGui::TableNextColumn();
							ImGui::Text("%s", address);
						}

						list = list->next;
						index++;
					}
				}
				ImGui::EndTable();
			}

			// Footer controls
			ImGui::Separator();
			ImGui::Spacing();
			if (ImGui::Button("CLOSE", ImVec2(100, 30)))
			{
				g_ShowServerBrowser = false;
			}
		}
		ImGui::End();

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}

	ImGui::Render();
	ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

int ImGuiHelper_HandleKey(int down, int keynum, const char *pszCurrentBinding)
{
	// Always allow toggleconsole to pass to the engine
	if (pszCurrentBinding && strcmp(pszCurrentBinding, "toggleconsole") == 0)
	{
		return 1;
	}

	// Toggle key: F8
	if (keynum == K_F8 && down)
	{
		ToggleImGuiMenu_Callback();
		return 0; // Handled
	}

	// Intercept Escape key
	if (keynum == K_ESCAPE && down)
	{
		// 1. If any editor or browser is visible, close it
		if (g_ShowImGuiMenu || g_ShowCrosshairEditor || g_ShowAGSettings || g_ShowServerBrowser)
		{
			g_ShowImGuiMenu = false;
			g_ShowCrosshairEditor = false;
			g_ShowAGSettings = false;
			g_ShowServerBrowser = false;
			ImGuiHelper_UpdateInputState();
			return 0; // Handled, block from engine
		}

		// 2. If pause menu is visible, close it
		if (g_ShowPauseMenu)
		{
			g_ShowPauseMenu = false;
			ImGuiHelper_UpdateInputState();
			return 0; // Handled, block from engine
		}

		// 3. Otherwise, if in-game and custom menu enabled, open pause menu
		const char* mapName = gEngfuncs.pfnGetLevelName();
		bool inGame = (mapName && mapName[0] != '\0');
		bool customMenuEnabled = cl_custom_menu ? (cl_custom_menu->value != 0.0f) : true;
		if (inGame && customMenuEnabled)
		{
			g_ShowPauseMenu = true;
			ImGuiHelper_UpdateInputState();
			return 0; // Handled, block from engine
		}
	}

	if (g_ShowImGuiMenu || g_ShowCrosshairEditor || g_ShowAGSettings || g_ShowPauseMenu || g_ShowMainMenu || g_ShowServerBrowser)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureKeyboard || g_ShowPauseMenu || g_ShowMainMenu || g_ShowServerBrowser)
		{
			return 0; // Handled, block from engine
		}
	}

	return 1; // Pass to engine
}
