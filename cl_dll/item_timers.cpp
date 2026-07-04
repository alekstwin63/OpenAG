#include "hud.h"
#include "cl_util.h"
#undef min
#undef max
#include "com_model.h"
#include "triangleapi.h"
#include "item_timers.h"
#include "pm_defs.h"
#include "pmtrace.h"
#include "pm_shared.h"
#include "event_api.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

extern vec3_t v_origin;

namespace item_timers
{
	std::vector<GroundItem> g_GroundItems;
	std::vector<ActiveTimer> g_ActiveTimers;
	cvar_t* cl_item_timers = nullptr;

	void init()
	{
		cl_item_timers = CVAR_CREATE("cl_item_timers", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
	}

	bool is_spawnable_item(const char* model_name)
	{
		if (!model_name) return false;
		if (std::strncmp(model_name, "models/w_", 9) != 0) return false;

		// Ignore flying weapons/items, projectiles, and static weapon boxes
		if (std::strstr(model_name, "w_grenade") || std::strstr(model_name, "w_rocket") || 
			std::strstr(model_name, "w_rpgrocket") || std::strstr(model_name, "w_weaponbox") ||
			std::strstr(model_name, "w_satchel") || std::strstr(model_name, "w_tripmine") ||
			std::strstr(model_name, "w_sqk"))
		{
			return false;
		}

		return true;
	}

	void add_entity_hook(struct cl_entity_s* ent, const char* modelname)
	{
		if (!cl_item_timers || cl_item_timers->value == 0.0f)
			return;

		if (!ent || !modelname || !is_spawnable_item(modelname))
			return;

		float current_time = gEngfuncs.GetClientTime();

		auto it = std::find_if(g_GroundItems.begin(), g_GroundItems.end(), [ent](const GroundItem& item) {
			return item.index == ent->index;
		});

		if (it == g_GroundItems.end())
		{
			g_GroundItems.push_back({ ent->index, ent->origin, modelname, true, current_time });
		}
		else
		{
			// If it was marked as invisible (picked up), but now it's back, remove timer (auto-correct)
			if (!it->was_visible)
			{
				g_ActiveTimers.erase(std::remove_if(g_ActiveTimers.begin(), g_ActiveTimers.end(), [&](const ActiveTimer& t) {
					return (t.origin - ent->origin).Length() < 48.0f;
				}), g_ActiveTimers.end());
			}

			it->origin = ent->origin;
			it->model_name = modelname;
			it->was_visible = true;
			it->last_seen_time = current_time;
		}
	}

	void update_and_draw()
	{
		if (!cl_item_timers || cl_item_timers->value == 0.0f)
		{
			g_ActiveTimers.clear();
			g_GroundItems.clear();
			return;
		}

		float current_time = gEngfuncs.GetClientTime();

		// 1. Detect picked up items (items that were visible but didn't update in the current frame)
		cl_entity_t* local = gEngfuncs.GetLocalPlayer();
		if (local)
		{
			for (auto& item : g_GroundItems)
			{
				// Item was visible but hasn't been seen in HUD_AddEntity this frame (threshold: 100ms)
				if (item.was_visible && (current_time - item.last_seen_time > 0.1f))
				{
					// Perform line-of-sight check to determine if the item was picked up or just culled by PVS/occlusion.
					// If there is a clear line of sight (or it is very close), it's a real pickup.
					Vector start(v_origin[0], v_origin[1], v_origin[2]);
					Vector end = item.origin + Vector(0.0f, 0.0f, 16.0f);

					pmtrace_t tr;
					gEngfuncs.pEventAPI->EV_PlayerTrace(start, end, PM_WORLD_ONLY, -1, &tr);

					float dist_to_target = (tr.endpos - end).Length();
					bool clear_los = (tr.fraction == 1.0f || dist_to_target < 24.0f);

					// Also, if the player is extremely close (e.g. within 120 units), consider it a pickup regardless of trace
					bool is_very_close = (local->origin - item.origin).Length() < 120.0f;

					if (clear_los || is_very_close)
					{
						float delay = 20.0f; // Default for weapons and standard ammo (20s)
						
						if (item.model_name.find("w_medkit") != std::string::npos ||
							item.model_name.find("w_battery") != std::string::npos ||
							item.model_name.find("w_longjump") != std::string::npos ||
							item.model_name.find("w_argrenade") != std::string::npos ||
							item.model_name.find("w_rpgclip") != std::string::npos)
						{
							delay = 30.0f; // Health, Armor, Longjump, and Heavy ammo (30s)
						}

						ActiveTimer timer;
						timer.origin = item.origin;
						timer.respawn_time = current_time + delay;
						timer.model_name = item.model_name;

						// Parse clean label name
						char label_buf[32];
						if (item.model_name.find("w_medkit") != std::string::npos) std::strcpy(label_buf, "Health");
						else if (item.model_name.find("w_battery") != std::string::npos) std::strcpy(label_buf, "Armor");
						else if (item.model_name.find("w_longjump") != std::string::npos) std::strcpy(label_buf, "Longjump");
						else
						{
							const char* p = std::strrchr(item.model_name.c_str(), '_');
							if (p)
							{
								std::strncpy(label_buf, p + 1, sizeof(label_buf));
								label_buf[sizeof(label_buf) - 1] = '\0';
								char* dot = std::strchr(label_buf, '.');
								if (dot) *dot = '\0';
							}
							else
							{
								std::strcpy(label_buf, "Item");
							}
						}
						timer.label = label_buf;

						// Make sure we don't insert duplicate timer at the exact same location
						auto double_timer = std::find_if(g_ActiveTimers.begin(), g_ActiveTimers.end(), [&](const ActiveTimer& t) {
							return (t.origin - timer.origin).Length() < 16.0f;
						});

						if (double_timer == g_ActiveTimers.end())
						{
							g_ActiveTimers.push_back(timer);
						}
					}

					item.was_visible = false;
				}
			}
		}

		// 2. Update & render countdowns
		g_ActiveTimers.erase(std::remove_if(g_ActiveTimers.begin(), g_ActiveTimers.end(), [current_time](const ActiveTimer& t) {
			return current_time >= t.respawn_time;
		}), g_ActiveTimers.end());

		for (const auto& timer : g_ActiveTimers)
		{
			float screen[3];
			if (gEngfuncs.pTriAPI->WorldToScreen(const_cast<float*>(static_cast<const float*>(timer.origin)), screen))
				continue; // behind player

			float x = XPROJECT(screen[0]);
			float y = YPROJECT(screen[1]);

			float remaining = timer.respawn_time - current_time;
			char text_buf[64];
			std::snprintf(text_buf, sizeof(text_buf), "%s: %.1fs", timer.label.c_str(), remaining);

			int w, h;
			gEngfuncs.pfnDrawConsoleStringLen(text_buf, &w, &h);
			gHUD.DrawConsoleStringWithColorTags(x - w / 2, y - h / 2, text_buf, false, 0, 255, 0); // Green color
		}
	}
}
