#pragma once
#include <memory>
#include <cstdint>
#include <sdk/sdk.h>
#include <sdk/math/math.h>
#include <cache/cache.h>

namespace rbx
{
	namespace silent
	{
		void silent_aim_1();
		void silent_aim_2();
		void initialize();
	}
}
inline std::unique_ptr<rbx::instance_t> g_mouseservice{};

inline bool g_silent_data_ready{ false };
inline bool g_silent_found_target{ false };
inline bool g_silent_target_needs_reset{ false };
inline math::vector2 g_silent_partpos{};
inline std::uint64_t g_silent_cached_position_x{ 0 };
inline std::uint64_t g_silent_cached_position_y{ 0 };
inline cache::entity_t g_silent_cached_target{};
inline cache::entity_t g_silent_cached_last_target{};
inline rbx::instance_t g_silent_aim_instance{};

inline bool g_silent_aim_enabled{ true };
inline bool g_silent_sticky_aim{ false };
inline bool g_silent_knock_check{ false };
inline bool g_silent_auto_switch{ false };
inline bool g_silent_spoof_mouse{ true };
inline bool g_silent_aim_locked{ false };
inline bool g_silent_aim_key_was_pressed{ false };

struct c_silent_help final {
	std::uint64_t address = 0;
	static std::uint64_t cached_input_object;

	c_silent_help() = default;
	c_silent_help(std::uint64_t addr) : address(addr) {}

	void set_frame_pos_x(std::uint64_t position);
	void set_frame_pos_y(std::uint64_t position);
	void initialize_mouse_service(std::uint64_t address);
	void write_mouse_position(std::uint64_t address, float x, float y);
};

