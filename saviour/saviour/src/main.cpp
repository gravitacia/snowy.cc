#include <cstdint>
#include <chrono>
#include <thread>

#include <memory/memory.h>
#include <sdk/offsets.h>
#include <sdk/sdk.h>
#include <game/game.h>
#include <cache/cache.h>
#include <render/render.h>
#include <features/silent/silent.h>

std::int32_t main()
{
	static const char* BINARY_NAME = "RobloxPlayerBeta.exe";

	if (!memory->find_process_id(BINARY_NAME))
	{
		printf("unable to find Roblox!\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	if (!memory->attach_to_process(BINARY_NAME))
	{
		printf("unable to attach to Roblox!\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	if (!memory->find_module_address(BINARY_NAME))
	{
		printf("unable to find main module address!\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	printf("base -> 0x%llx\n", memory->get_module_address());

	std::uint64_t fake_datamodel{ memory->read<std::uint64_t>(memory->get_module_address() + Offsets::FakeDataModel::Pointer) };
	game::datamodel = rbx::instance_t(memory->read<std::uint64_t>(fake_datamodel + Offsets::FakeDataModel::RealDataModel));
	printf("datamodel -> 0x%llx\n", game::datamodel.address);

	game::visengine = { memory->read<std::uint64_t>(memory->get_module_address() + Offsets::VisualEngine::Pointer) };
	printf("visengine -> 0x%llx\n", game::visengine.address);

	game::players = { game::datamodel.find_first_child_by_class("Players") };
	printf("players -> 0x%llx\n", game::players.address);

	game::local_player = { memory->read<std::uint64_t>(game::players.address + Offsets::Player::LocalPlayer) };
	printf("local_player -> 0x%llx\n", game::local_player.address);

	printf("\n%s, %s\n", game::datamodel.get_name().c_str(), game::datamodel.get_class_name().c_str());

	std::thread(cache::run).detach();
	rbx::silent::initialize();

	if (!render->create_window())
	{
		printf("failed to create window\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	if (!render->create_device())
	{
		printf("failed to create device\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}
	
	if (!render->create_imgui())
	{
		printf("failed to create imgui\n");
		std::this_thread::sleep_for(std::chrono::seconds(5));
		return 1;
	}

	while (true)
	{
		render->start_render();

		render->render_visuals();

		if (render->running)
		{
			render->render_menu();
		}

		render->end_render();
	}
}