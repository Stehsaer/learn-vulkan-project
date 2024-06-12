#include "application.hpp"

void App_swapchain::create_swapchain(const App_environment& env)
{
	const auto available_surface_formats = env.physical_device.getSurfaceFormatsKHR(env.surface);
	const auto capabilities              = env.physical_device.getSurfaceCapabilitiesKHR(env.surface);
	auto       available_present_modes   = env.physical_device.getSurfacePresentModesKHR(env.surface);

	// select surface format
	surface_format = [&available_surface_formats]()
	{
		for (const auto& surface_format : available_surface_formats)
		{
			if (surface_format.format == vk::Format::eR8G8B8A8Unorm
				&& surface_format.colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear)
				return surface_format;
		}
		return available_surface_formats[0];
	}();

	// select image count
	extent.width = std::clamp(
		capabilities.currentExtent.width,
		capabilities.minImageExtent.width,
		capabilities.maxImageExtent.width
	);
	extent.height = std::clamp(
		capabilities.currentExtent.height,
		capabilities.minImageExtent.height,
		capabilities.maxImageExtent.height
	);

	// select present mode
	const static std::map<vk::PresentModeKHR, int> present_mode_ranking = {
		{vk::PresentModeKHR::eMailbox,                 0  },
		{vk::PresentModeKHR::eFifoRelaxed,             1  },
		{vk::PresentModeKHR::eFifo,                    2  },
		{vk::PresentModeKHR::eImmediate,               3  },
		{vk::PresentModeKHR::eSharedContinuousRefresh, 100},
		{vk::PresentModeKHR::eSharedDemandRefresh,     100}
	};

	std::ranges::sort(
		available_present_modes,
		[](auto x, auto y)
		{
			return present_mode_ranking.at(x) < present_mode_ranking.at(y);
		}
	);
	auto target_present_mode = available_present_modes[0];

	// select image count
	image_count = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);

	auto queue_families    = std::to_array({env.g_family_idx, env.p_family_idx});
	auto queue_family_span = std::span(queue_families.begin(), env.g_family_idx == env.p_family_idx ? 1 : 2);

	swapchain = Swapchain(
		env.device,
		env.surface,
		surface_format,
		target_present_mode,
		extent,
		image_count,
		queue_family_span,
		capabilities.currentTransform,
		swapchain
	);
}

void App_swapchain::create_images(const App_environment& env)
{
	// get images
	image_handles = swapchain.get_images();

	// create views
	image_views.clear();
	for (auto i : Range(image_count))
		image_views.emplace_back(
			env.device,
			image_handles[i],
			surface_format.format,
			vk::ImageViewType::e2D,
			vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)
		);
}

void App_swapchain::create(App_environment& env)
{
	create_swapchain(env);
	create_images(env);

	env.command_buffer = Command_buffer::allocate_multiple_from(env.command_pool, image_count);
}