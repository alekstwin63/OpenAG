#pragma once

#include "util_vector.h"

namespace damage_numbers
{
	extern bool g_bServerSendsDamage;
	void init();
	void reset();
	void add_damage(const Vector& origin, int damage, bool is_headshot);
	void add_predicted_damage(const Vector& origin, int damage, bool is_headshot);
	void update_and_draw();
}

