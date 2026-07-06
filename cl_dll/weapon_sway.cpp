#include "hud.h"
#include "cl_util.h"
#include "weapon_sway.h"
#include "cl_entity.h"
#include "in_defs.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace weapon_sway
{
	static cvar_t *cl_weapon_sway = nullptr;
	static cvar_t *cl_weapon_sway_pitch = nullptr;
	static cvar_t *cl_weapon_sway_yaw = nullptr;
	static cvar_t *cl_weapon_sway_roll = nullptr;

	static cvar_t *cl_weapon_sway_clamp = nullptr;
	static cvar_t *cl_weapon_sway_roll_factor = nullptr;
	static cvar_t *cl_weapon_sway_pos_yaw = nullptr;
	static cvar_t *cl_weapon_sway_pos_pitch = nullptr;

	// Current smoothed viewmodel angles
	static float sway_angles[3] = { 0.0f, 0.0f, 0.0f };
	static bool initialized = false;

	static float normalize_angle(float angle)
	{
		angle = fmodf(angle + 180.0f, 360.0f);
		if (angle < 0.0f)
			angle += 360.0f;
		return angle - 180.0f;
	}

	void init()
	{
		cl_weapon_sway       = gEngfuncs.pfnRegisterVariable("cl_weapon_sway", "1", FCVAR_ARCHIVE);
		cl_weapon_sway_pitch = gEngfuncs.pfnRegisterVariable("cl_weapon_sway_pitch", "6.0", FCVAR_ARCHIVE);
		cl_weapon_sway_yaw   = gEngfuncs.pfnRegisterVariable("cl_weapon_sway_yaw", "6.0", FCVAR_ARCHIVE);
		cl_weapon_sway_roll  = gEngfuncs.pfnRegisterVariable("cl_weapon_sway_roll", "4.0", FCVAR_ARCHIVE);

		cl_weapon_sway_clamp       = gEngfuncs.pfnRegisterVariable("cl_weapon_sway_clamp", "10.0", FCVAR_ARCHIVE);
		cl_weapon_sway_roll_factor = gEngfuncs.pfnRegisterVariable("cl_weapon_sway_roll_factor", "0.15", FCVAR_ARCHIVE);
		cl_weapon_sway_pos_yaw     = gEngfuncs.pfnRegisterVariable("cl_weapon_sway_pos_yaw", "0.02", FCVAR_ARCHIVE);
		cl_weapon_sway_pos_pitch   = gEngfuncs.pfnRegisterVariable("cl_weapon_sway_pos_pitch", "0.02", FCVAR_ARCHIVE);
	}

	void reset()
	{
		sway_angles[0] = 0.0f;
		sway_angles[1] = 0.0f;
		sway_angles[2] = 0.0f;
		initialized = false;
	}

	void apply(struct ref_params_s *pparams, struct cl_entity_s *viewent)
	{
		if (!cl_weapon_sway || cl_weapon_sway->value <= 0.0f)
			return;

		// Don't apply sway during demo playback or spectator mode
		if (pparams->demoplayback || pparams->spectator)
			return;

		const float frametime = pparams->frametime;

		// Target angles: what the camera is looking at (viewmodel should follow these)
		// V_CalcGunAngle sets viewent->angles based on pparams->viewangles + crosshair + idle
		// We want sway to lag behind the *viewangles* component, not the idle/bob part.
		float target[3];
		target[PITCH] = normalize_angle(-pparams->viewangles[PITCH]);
		target[YAW]   = normalize_angle(pparams->viewangles[YAW]);

		// Calculate yaw lag to determine turn roll target
		float diff_yaw = normalize_angle(target[YAW] - sway_angles[YAW]);
		target[ROLL]  = normalize_angle(diff_yaw * cl_weapon_sway_roll_factor->value);

		// Initialize on first frame
		if (!initialized)
		{
			sway_angles[PITCH] = target[PITCH];
			sway_angles[YAW]   = target[YAW];
			sway_angles[ROLL]  = target[ROLL];
			initialized = true;
			return;
		}

		const float speeds[3] = {
			cl_weapon_sway_pitch->value,
			cl_weapon_sway_yaw->value,
			cl_weapon_sway_roll->value
		};

		const float clamp_limit = cl_weapon_sway_clamp->value;

		// Exponential smoothing and delta application for each axis
		for (int i = 0; i < 3; i++)
		{
			float factor = 1.0f - expf(-speeds[i] * frametime);
			float diff = normalize_angle(target[i] - sway_angles[i]);

			// Clamp the delta to prevent weapon from swaying too far or going off-screen
			if (clamp_limit > 0.0f)
			{
				if (diff > clamp_limit) diff = clamp_limit;
				else if (diff < -clamp_limit) diff = -clamp_limit;
			}

			sway_angles[i] = normalize_angle(sway_angles[i] + diff * factor);

			// Apply sway as a delta on top of whatever V_CalcGunAngle already set.
			// The delta is: (sway - target), i.e. how far behind we're lagging.
			viewent->angles[i] += normalize_angle(sway_angles[i] - target[i]);
		}

		// Apply position offset (sway translation) based on angle lag (in world coordinates)
		float lag_yaw   = normalize_angle(sway_angles[YAW] - target[YAW]);
		float lag_pitch = normalize_angle(sway_angles[PITCH] - target[PITCH]);

		float pos_yaw_scale   = cl_weapon_sway_pos_yaw->value;
		float pos_pitch_scale = cl_weapon_sway_pos_pitch->value;

		for (int i = 0; i < 3; i++)
		{
			viewent->origin[i] += lag_yaw * pos_yaw_scale * pparams->right[i]
			                    + lag_pitch * pos_pitch_scale * pparams->up[i];
		}

		// Sync all angle and position copies so the renderer picks up the changes
		VectorCopy(viewent->angles, viewent->curstate.angles);
		VectorCopy(viewent->angles, viewent->latched.prevangles);
		VectorCopy(viewent->origin, viewent->curstate.origin);
		VectorCopy(viewent->origin, viewent->latched.prevorigin);
	}
}
