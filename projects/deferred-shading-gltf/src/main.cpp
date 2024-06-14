#include "binary-resource.hpp"
#include "logic.hpp"
#include <csignal>
#include <filesystem>
#include <thread>

void sig_handler_fpe(int sig_num)
{
	std::cerr << "[Error Report] Arithmatics Error! code=" << sig_num << '\n';
	exit(-1);
}

void sig_handler_sigsev(int sig_num)
{
	std::cerr << "[Error Report] Segment Fault! code=" << sig_num << '\n';
	exit(-2);
}

const char* cut_filename(const char* filename)
{
	while (true)
	{
		const char* const find1 = std::strstr(filename, "src/");
		const char* const find2 = std::strstr(filename, "src\\");

		if (find1 == nullptr && find2 == nullptr) return filename;

		if (find1 != nullptr) filename = find1 + 4;

		if (find2 != nullptr) filename = find2 + 4;
	}
}

int main(int argc [[maybe_unused]], char** argv [[maybe_unused]])
{
	// std::signal(SIGSEGV, sig_handler_sigsev);
	// std::signal(SIGFPE, sig_handler_fpe);

	std::shared_ptr<Core> shared_resource;

	auto show_error_msgbox = [](const std::string& title, const std::string& content, const std::shared_ptr<Core>& shared_resource)
	{
		if (shared_resource)
			if (shared_resource->env.window.is_valid())
			{
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), content.c_str(), shared_resource->env.window);
			}
	};

	try
	{
		shared_resource = std::make_shared<Core>();

		std::shared_ptr<Application_logic_base> current_logic = std::make_shared<App_idle_logic>(shared_resource);
		while (current_logic)
		{
			current_logic = current_logic->work();
		}

		terminate_sdl();

		shared_resource = nullptr;

		return EXIT_SUCCESS;
	}
	catch (const General_exception& err)
	{
		std::cerr << "General Error: " << err.what() << '\n';

		show_error_msgbox(
			"General Error",
			std::format("General Error caught:\n[Line {} at {}]\n{}", err.loc.line(), cut_filename(err.loc.file_name()), err.msg),
			shared_resource
		);
	}
	catch (const vk::IncompatibleDriverError& err)
	{
		std::cerr << "Incompatible Driver!" << '\n';

		show_error_msgbox("Incompatible Driver", std::format("Incompatible Driver:\n{}", err.what()), shared_resource);
	}
	catch (const vk::DeviceLostError& err)
	{
		std::cerr << "Device Lost!" << '\n';

		show_error_msgbox("Device Lost", std::format("Vulkan Device Lost:\n{}", err.what()), shared_resource);
	}
	catch (const vk::SystemError& err)
	{
		std::cerr << "Vulkan Error: " << err.what() << '\n';

		show_error_msgbox("Vulkan Error", std::format("Vulkan Error caught:\n{}", err.what()), shared_resource);
	}
	catch (const std::exception& err)
	{
		std::cerr << "Unknown Error: " << err.what() << '\n';

		show_error_msgbox("Unknown Error", std::format("Unknown caught:\n{}", err.what()), shared_resource);
	}

#if NDEBUG
	system("pause");
#endif

	return EXIT_FAILURE;
}
