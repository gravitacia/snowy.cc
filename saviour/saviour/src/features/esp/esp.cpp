#include "esp.h"

#include <vector>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <settings.h>
#include <game/game.h>
#include <cache/cache.h>

namespace helper
{
	__forceinline void box(ImVec2& c1, ImVec2& c2, ImU32 color)
	{
		c1.x = std::round(c1.x);
		c1.y = std::round(c1.y);
		c2.x = std::round(c2.x);
		c2.y = std::round(c2.y);

		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		draw->Flags &= ImDrawListFlags_AntiAliasedLines;

		ImRect rect(c1.x, c1.y, c1.x + c2.x, c1.y + c2.y);
		ImVec2 shadow = { cosf(0.f) * 2.f, sinf(0.f) * 2.f };

		draw->AddRect(rect.Min, rect.Max, IM_COL32(0, 0, 0, color >> 24));
		draw->AddRect({ rect.Min.x - 1.f, rect.Min.y - 1.f }, { rect.Max.x + 1.f, rect.Max.y + 1.f }, color);
		draw->AddRect({ rect.Min.x - 2.f, rect.Min.y - 2.f }, { rect.Max.x + 2.f, rect.Max.y + 2.f }, IM_COL32(0, 0, 0, color >> 24));
	}
}

void esp::run()
{
	static math::vector3 corners[8] =
	{
		{-1, -1, -1}, {1, -1, -1}, {-1, 1, -1},{1, 1, -1},
		{-1, -1, 1}, {1, -1, 1}, {-1, 1, 1}, {1, 1, 1}
	};

	math::vector2 dims = game::visengine.get_dimensions();
	math::matrix4 view = game::visengine.get_viewmatrix();

	std::vector<cache::entity_t> snapshot;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		snapshot = cache::cached_players;
	}

	for (cache::entity_t& entity : snapshot)
	{
		bool valid = false;
		float left = FLT_MAX, top = FLT_MAX;
		float right = -FLT_MAX, bottom = -FLT_MAX;

		if (entity.instance.address == 0)
		{
			continue;
		}

		for (auto& parts : entity.parts)
		{
			rbx::primitive_t prim = parts.second.get_primitive();
			auto size = prim.get_size();
			auto pos = prim.get_position();
			auto rot = prim.get_rotation();

			if (size.x == 0 || size.y == 0 || size.z == 0)
			{
				continue;
			}

			for (auto& corner : corners)
			{
				math::vector3 world = pos + rot * math::vector3
				{
					corner.x * size.x * 0.5f,
					corner.y * size.y * 0.5f,
					corner.z * size.z * 0.5f
				};

				math::vector2 out{};
				if (game::visengine.world_to_screen(world, out, dims, view))
				{
					valid = true;
					left = min(left, out.x);
					top = min(top, out.y);
					right = max(right, out.x);
					bottom = max(bottom, out.y);
				}
			}
		}

		if (!valid || left >= right || top >= bottom)
		{
			continue;
		}

		ImVec2 c1(left, top);
		ImVec2 c2(right - left, bottom - top);

		if (settings::visuals::box)
		{
			helper::box(c1, c2, ImGui::ColorConvertFloat4ToU32
			(
				{
					settings::visuals::box_color[0],
					settings::visuals::box_color[1],
					settings::visuals::box_color[2],
					settings::visuals::box_color[3]
				}
			));
		}
	}
}