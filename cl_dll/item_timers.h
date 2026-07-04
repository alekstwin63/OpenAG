#pragma once

#include <vector>
#include <string>
#include "util_vector.h"

namespace item_timers
{
	struct GroundItem
	{
		int index;
		Vector origin;
		std::string model_name;
		bool was_visible;
		float last_seen_time;
	};

	struct ActiveTimer
	{
		Vector origin;
		float respawn_time;
		std::string model_name;
		std::string label;
	};

	void init();
	void add_entity_hook(struct cl_entity_s* ent, const char* modelname);
	void update_and_draw();
}
