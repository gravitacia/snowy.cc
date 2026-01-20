#pragma once
#include "keybind/keybind.h"

namespace settings
{
	namespace visuals
	{
		inline bool box{ false };
		inline float box_color[4]{ 1.f, 1.f, 1.f, 1.f };

		inline bool name{ false };
		inline float name_color[4]{ 1.f, 1.f, 1.f, 1.f };
	}

	namespace silent_aim
	{
		inline bool enabled{ false };
		inline bool sticky_aim{ false };
		inline bool spoof_mouse{ true };
		inline keybind keybind{ "silentaimkeybind" };
		inline int keybind_mode{ 0 }; // 0 = hold, 1 = toggle, 2 = always
		inline int aim_part{ 0 }; // 0 = head, 1 = torso, 2 = closest
		inline bool knocked_check{ false };
	}
}