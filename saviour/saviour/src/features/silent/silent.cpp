#define NOMINMAX
#include <Windows.h>
#include <thread>
#include <vector>
#include <algorithm>
#include <immintrin.h>
#include <cmath>
#include <limits>

#include <memory/memory.h>
#include <sdk/sdk.h>
#include <sdk/offsets.h>
#include <cache/cache.h>
#include <game/game.h>
#include <settings.h>
#include "silent.h"

std::uint64_t c_silent_help::cached_input_object = 0;

static math::vector2 world_to_screen(math::vector3 world, math::vector2 dimensions, math::matrix4 viewmatrix)
{
	float clipX = world.x * viewmatrix.m[0][0] + world.y * viewmatrix.m[0][1] + world.z * viewmatrix.m[0][2] + viewmatrix.m[0][3];
	float clipY = world.x * viewmatrix.m[1][0] + world.y * viewmatrix.m[1][1] + world.z * viewmatrix.m[1][2] + viewmatrix.m[1][3];
	float clipZ = world.x * viewmatrix.m[2][0] + world.y * viewmatrix.m[2][1] + world.z * viewmatrix.m[2][2] + viewmatrix.m[2][3];
	float clipW = world.x * viewmatrix.m[3][0] + world.y * viewmatrix.m[3][1] + world.z * viewmatrix.m[3][2] + viewmatrix.m[3][3];

	if (clipW <= 1e-6f)
		return { -1.0f, -1.0f };

	float inv_w = 1.0f / clipW;
	float ndcX = clipX * inv_w;
	float ndcY = clipY * inv_w;

	return {
		(dimensions.x / 2.0f) * (ndcX + 1.0f),
		(dimensions.y / 2.0f) * (1.0f - ndcY)
	};
}

static float get_magnitude(const math::vector2& a, const math::vector2& b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return std::sqrt(dx * dx + dy * dy);
}


static rbx::part_t get_target_part(cache::entity_t& player, int aim_part)
{
	rbx::part_t target_part{};
	
	if (aim_part == 0)
	{
		auto head_it = player.parts.find("Head");
		if (head_it != player.parts.end())
			target_part = head_it->second;
	}
	else if (aim_part == 1)
	{
		auto torso_it = player.parts.find("UpperTorso");
		if (torso_it != player.parts.end())
			target_part = torso_it->second;
		else
		{
			auto torso_r6 = player.parts.find("Torso");
			if (torso_r6 != player.parts.end())
				target_part = torso_r6->second;
		}
	}
	else if (aim_part == 2)
	{
		POINT cursor_point;
		HWND rblxWnd = FindWindowA(nullptr, "Roblox");
		if (rblxWnd && GetCursorPos(&cursor_point) && ScreenToClient(rblxWnd, &cursor_point))
		{
			math::vector2 cursor = { static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y) };
			math::vector2 dimensions = game::visengine.get_dimensions();
			math::matrix4 viewmatrix = game::visengine.get_viewmatrix();
			
			float shortest_distance = (std::numeric_limits<float>::max)();
			
			for (auto& part_pair : player.parts)
			{
				rbx::primitive_t prim = part_pair.second.get_primitive();
				if (!prim.address)
					continue;
					
				math::vector3 part_position = prim.get_position();
				math::vector2 part_screen = world_to_screen(part_position, dimensions, viewmatrix);
				
				if (part_screen.x < 0 || part_screen.y < 0)
					continue;
					
				float distance = get_magnitude(part_screen, cursor);
				if (distance < shortest_distance)
				{
					shortest_distance = distance;
					target_part = part_pair.second;
				}
			}
		}
	}
	
	return target_part;
}

static bool is_player_knocked(cache::entity_t& player)
{
	if (player.instance.address == 0)
		return false;

	rbx::player_t player_instance(player.instance.address);
	rbx::model_instance_t model_instance = player_instance.get_model_instance();

	if (model_instance.address == 0)
		return false;

	rbx::instance_t body_effects = model_instance.find_first_child("BodyEffects");
	if (body_effects.address == 0)
		return false;

	rbx::instance_t ko = body_effects.find_first_child("K.O");
	if (ko.address == 0)
		return false;

	bool ko_value = memory->read<bool>(ko.address + Offsets::Misc::Value);
	return ko_value;
}


static cache::entity_t get_closest_player_from_cursor()
{
	POINT cursor_point;
	HWND rblxWnd = FindWindowA(nullptr, "Roblox");
	if (!rblxWnd)
		return {};

	if (!GetCursorPos(&cursor_point))
		return {};

	if (!ScreenToClient(rblxWnd, &cursor_point))
		return {};

	math::vector2 cursor = { static_cast<float>(cursor_point.x), static_cast<float>(cursor_point.y) };

	std::vector<cache::entity_t> players_snapshot;
	{
		std::lock_guard<std::mutex> lock(cache::mtx);
		players_snapshot = cache::cached_players;
	}

	if (players_snapshot.empty())
		return {};

	cache::entity_t closest_player{};
	float shortest_distance = (std::numeric_limits<float>::max)();

	math::vector2 dimensions = game::visengine.get_dimensions();
	math::matrix4 viewmatrix = game::visengine.get_viewmatrix();

	for (cache::entity_t& player : players_snapshot)
	{
		if (player.instance.address == 0)
			continue;

		if (player.instance.address == game::local_player.address)
			continue;

		rbx::part_t target_part = get_target_part(player, settings::silent_aim::aim_part);

		if (!target_part.address)
			continue;

		rbx::primitive_t prim = target_part.get_primitive();
		if (!prim.address)
			continue;

		math::vector3 part_position = prim.get_position();
		math::vector2 part_screen = world_to_screen(part_position, dimensions, viewmatrix);

		if (part_screen.x < 0 || part_screen.y < 0)
			continue;

		if (settings::silent_aim::knocked_check && is_player_knocked(player))
			continue;

		float distance_from_cursor = get_magnitude(part_screen, cursor);
		if (distance_from_cursor < shortest_distance)
		{
			shortest_distance = distance_from_cursor;
			closest_player = player;
		}
	}

	return closest_player;
}


static std::uint64_t get_current_input_object(std::uint64_t base_address)
{
	return memory->read<std::uint64_t>(base_address + Offsets::MouseService::InputObject + sizeof(std::shared_ptr<void*>));
}

void c_silent_help::set_frame_pos_x(std::uint64_t position)
{
	memory->write<std::uint64_t>(address + Offsets::Silent::FramePositionOffsetX, position);
}

void c_silent_help::set_frame_pos_y(std::uint64_t position)
{
	memory->write<std::uint64_t>(address + Offsets::Silent::FramePositionOffsetY, position);
}

void c_silent_help::initialize_mouse_service(std::uint64_t address)
{
	cached_input_object = get_current_input_object(address);

	if (cached_input_object && cached_input_object != 0xFFFFFFFFFFFFFFFF)
	{
		const char* base_pointer = reinterpret_cast<const char*>(cached_input_object);
		_mm_prefetch(base_pointer + Offsets::MouseService::MousePosition, _MM_HINT_T0);
		_mm_prefetch(base_pointer + Offsets::MouseService::MousePosition + sizeof(math::vector2), _MM_HINT_T0);
	}
}

void c_silent_help::write_mouse_position(std::uint64_t address, float x, float y) 
{
	cached_input_object = get_current_input_object(address);
	if (cached_input_object != 0 && cached_input_object != 0xFFFFFFFFFFFFFFFF) 
	{
		math::vector2 new_position = { x, y };
		memory->write<math::vector2>(cached_input_object + Offsets::MouseService::MousePosition, new_position);
	}
}

static bool should_silent_aim_be_active()
{
	if (!settings::silent_aim::enabled)
		return false;

	return g_silent_aim_locked;
}

static void update_silent_aim_key_state()
{
	settings::silent_aim::keybind.update();
	
	if (settings::silent_aim::keybind.key == 0)
		return;

	bool key_currently_pressed = settings::silent_aim::keybind.enabled;

	// Hold mode
	if (settings::silent_aim::keybind_mode == 0)
	{
		if (key_currently_pressed && !g_silent_aim_locked)
		{
			g_silent_aim_locked = true;
		}
		else if (!key_currently_pressed && g_silent_aim_locked)
		{
			g_silent_aim_locked = false;
			g_silent_cached_target = {};
			g_silent_found_target = false;
			g_silent_data_ready = false;
		}
	}
	// Toggle or Always
	else
	{
		if (key_currently_pressed && !g_silent_aim_key_was_pressed)
		{
			if (!g_silent_aim_locked)
			{
				g_silent_aim_locked = true;
			}
			else
			{
				g_silent_aim_locked = false;
				g_silent_cached_target = {};
				g_silent_found_target = false;
				g_silent_data_ready = false;
			}
		}
	}

	g_silent_aim_key_was_pressed = key_currently_pressed;
}

void rbx::silent::silent_aim_1()
{
	cache::entity_t target{};

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	HWND roblox_window = FindWindowA(0, "Roblox");

	for (;;)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		g_mouseservice = std::make_unique<rbx::instance_t>(game::datamodel.find_first_child_by_class("MouseService"));

		if (!g_mouseservice || !game::datamodel.address || !game::visengine.address)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		update_silent_aim_key_state();

		if (!should_silent_aim_be_active())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			g_silent_data_ready = false;
			if (g_silent_cached_target.instance.address != 0)
			{
				g_silent_cached_target = {};
			}
			target = {};
			g_silent_found_target = false;
			g_silent_target_needs_reset = false;
			continue;
		}

		if (!game::datamodel.address)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		rbx::instance_t players = game::datamodel.find_first_child_by_class("Players");
		if (players.address == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		rbx::instance_t local_player = rbx::instance_t(memory->read<std::uint64_t>(players.address + Offsets::Player::LocalPlayer));

		if (local_player.address == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		static int aim_instance_check_counter = 0;
		if (aim_instance_check_counter++ % 10 == 0)
		{
			rbx::instance_t player_gui = local_player.find_first_child("PlayerGui");
			if (player_gui.address != 0)
			{
				rbx::instance_t main_screen_gui = player_gui.find_first_child("MainScreenGui");
				if (main_screen_gui.address == 0)
					main_screen_gui = player_gui.find_first_child("Main Screen");

				if (main_screen_gui.address != 0)
				{
					g_silent_aim_instance = rbx::instance_t(main_screen_gui.find_first_child("Aim"));
				}
			}
		}

		bool always_mode = (settings::silent_aim::keybind_mode == 2);
		
		if (!g_silent_found_target || g_silent_cached_target.instance.address == 0)
		{
			target = get_closest_player_from_cursor();
			
			g_silent_cached_last_target = target;
			
			rbx::part_t target_part = get_target_part(target, settings::silent_aim::aim_part);
			g_silent_found_target = (target_part.address != 0);
			g_silent_cached_target = target;
		}
		else
		{
			if (always_mode || !settings::silent_aim::sticky_aim)
			{
				target = get_closest_player_from_cursor();
				
				g_silent_cached_last_target = target;
				
				rbx::part_t target_part = get_target_part(target, settings::silent_aim::aim_part);
				g_silent_found_target = (target_part.address != 0);
				g_silent_cached_target = target;
			}
			else
			{
				target = g_silent_cached_target;
			}
		}

		if (g_silent_found_target && g_silent_cached_target.instance.address != 0 && game::visengine.address)
		{
			if (settings::silent_aim::knocked_check && is_player_knocked(g_silent_cached_target))
			{
				g_silent_found_target = false;
				g_silent_cached_target = {};
				continue;
			}

			rbx::part_t target_part = get_target_part(g_silent_cached_target, settings::silent_aim::aim_part);
			
			if (target_part.address != 0)
			{
				rbx::primitive_t prim = target_part.get_primitive();
				if (prim.address)
				{
					math::vector3 part_3d = prim.get_position();
					math::matrix4 view = game::visengine.get_viewmatrix();
					math::vector2 dims = game::visengine.get_dimensions();

					g_silent_partpos = world_to_screen(part_3d, dims, view);
					POINT cursor_point;
					GetCursorPos(&cursor_point);
					if (roblox_window)
						ScreenToClient(roblox_window, &cursor_point);

					g_silent_cached_position_x = static_cast<std::uint64_t>(cursor_point.x);
					g_silent_cached_position_y = static_cast<std::uint64_t>(dims.y - std::abs(dims.y - static_cast<float>(cursor_point.y)) - 58);
					g_silent_data_ready = true;
				}
				else
				{
					g_silent_data_ready = false;
				}
			}
			else
			{
				g_silent_data_ready = false;
			}
		}
		else
		{
			g_silent_data_ready = false;
		}
	}
}

void rbx::silent::silent_aim_2()
{
	c_silent_help mouse_service_instance{};
	bool mouse_service_initialized = false;

	for (;;)
	{
		if (!g_mouseservice)
		{
			mouse_service_initialized = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (!should_silent_aim_be_active())
		{
			mouse_service_initialized = false;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (g_silent_cached_target.instance.address != 0)
				g_silent_target_needs_reset = true;
			continue;
		}

		if (g_silent_cached_target.instance.address != 0 && g_silent_data_ready && g_mouseservice && g_mouseservice->address != 0)
		{
			if (g_silent_partpos.x < 0.0f || g_silent_partpos.y < 0.0f ||
				g_silent_partpos.x > 10000.0f || g_silent_partpos.y > 10000.0f)
			{
				continue;
			}

			try
			{
				if (!mouse_service_initialized)
				{
					mouse_service_instance.initialize_mouse_service(g_mouseservice->address);
					mouse_service_initialized = true;
				}

				if (settings::silent_aim::spoof_mouse && g_silent_aim_instance.address != 0)
				{
					c_silent_help aim_helper(g_silent_aim_instance.address);
					aim_helper.set_frame_pos_x(g_silent_cached_position_x);
					aim_helper.set_frame_pos_y(g_silent_cached_position_y);
				}

				mouse_service_instance.write_mouse_position(g_mouseservice->address, g_silent_partpos.x, g_silent_partpos.y);
			}
			catch (...)
			{
				mouse_service_initialized = false;
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}
	}
}

void rbx::silent::initialize()
{
	std::thread(silent_aim_1).detach();
	std::thread(silent_aim_2).detach();
}
