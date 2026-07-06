#pragma once

#include "cl_entity.h"

struct ref_params_s;

namespace weapon_sway
{
	/**
	 * Initializes weapon sway CVars. Call once from V_Init().
	 */
	void init();

	/**
	 * Resets sway state (call on map change / vidinit).
	 */
	void reset();

	/**
	 * Applies weapon sway to the viewmodel entity.
	 * Call from V_CalcGunAngle() after viewmodel angles are set.
	 *
	 * @param pparams  Current ref params (frametime, viewangles, etc.)
	 * @param viewent  The viewmodel entity to modify angles on
	 */
	void apply(struct ref_params_s *pparams, struct cl_entity_s *viewent);
}
