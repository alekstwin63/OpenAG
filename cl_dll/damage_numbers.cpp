#include "hud.h"
#include "cl_util.h"
#undef min
#undef max
#include "com_model.h"
#include "triangleapi.h"
#include "parsemsg.h"
#include "damage_numbers.h"
#include <vector>
#include <algorithm>
#include <cstdio>


// Global user message callback
int __MsgFunc_DmgDealt(const char *pszName, int iSize, void *pbuf)
{
	BEGIN_READ(pbuf, iSize);
	Vector origin;
	origin.x = READ_COORD();
	origin.y = READ_COORD();
	origin.z = READ_COORD();
	int damage = READ_SHORT();
	bool is_headshot = (READ_BYTE() != 0);

	damage_numbers::g_bServerSendsDamage = true;
	damage_numbers::add_damage(origin, damage, is_headshot);

	return 1;
}

namespace damage_numbers
{
	bool g_bServerSendsDamage = false;

	struct DamageNumber
	{
		Vector origin;
		int damage;
		float spawn_time;
		float lifetime;
		bool is_headshot;
	};

	std::vector<DamageNumber> g_DamageNumbers;
	cvar_t* cl_damage_numbers = nullptr;

	void init()
	{
		g_bServerSendsDamage = false; // Reset on map change
		cl_damage_numbers = CVAR_CREATE("cl_damage_numbers", "1", FCVAR_CLIENTDLL | FCVAR_ARCHIVE);
		HOOK_MESSAGE(DmgDealt);
	}

	void add_predicted_damage(const Vector& origin, int damage, bool is_headshot)
	{
		if (g_bServerSendsDamage)
			return;
		add_damage(origin, damage, is_headshot);
	}


	void add_damage(const Vector& origin, int damage, bool is_headshot)
	{
		if (!cl_damage_numbers || cl_damage_numbers->value == 0.0f)
			return;

		float current_time = gEngfuncs.GetClientTime();

		// Aggregate hits if they land very close in time (always stack)
		for (auto& dn : g_DamageNumbers)
		{
			if (current_time - dn.spawn_time < 0.15f && (dn.origin - origin).Length() < 150.0f)
			{
				dn.damage += damage;
				dn.is_headshot |= is_headshot;
				return;
			}
		}

		g_DamageNumbers.push_back({ origin, damage, current_time, 1.0f, is_headshot });
	}

	void update_and_draw()
	{
		if (!cl_damage_numbers || cl_damage_numbers->value == 0.0f)
		{
			g_DamageNumbers.clear();
			return;
		}

		float current_time = gEngfuncs.GetClientTime();


		// Clean expired entries
		g_DamageNumbers.erase(std::remove_if(g_DamageNumbers.begin(), g_DamageNumbers.end(), [current_time](const DamageNumber& dn) {
			return current_time - dn.spawn_time >= dn.lifetime;
		}), g_DamageNumbers.end());

		for (const auto& dn : g_DamageNumbers)
		{
			float age = current_time - dn.spawn_time;
			float life_ratio = age / dn.lifetime;
			float fade = 1.0f - life_ratio;
			if (fade < 0.0f) fade = 0.0f;

			Vector float_origin = dn.origin + Vector(0.0f, 0.0f, age * 32.0f);

			float screen[3];
			if (gEngfuncs.pTriAPI->WorldToScreen(const_cast<float*>(static_cast<const float*>(float_origin)), screen))
				continue;

			int x = XPROJECT(screen[0]);
			int y = YPROJECT(screen[1]);

			int r, g, b;
			if (dn.is_headshot)
			{
				r = (int)(255 * fade);
				g = (int)(40 * fade);
				b = (int)(40 * fade);
			}
			else
			{
				r = (int)(255 * fade);
				g = (int)(220 * fade);
				b = (int)(0);
			}

			// Shadow
			gHUD.DrawHudNumberCentered(x + 2, y + 2, dn.damage, 0, 0, 0);
			// Main number
			gHUD.DrawHudNumberCentered(x, y, dn.damage, r, g, b);
		}
	}
}
