#include "vklib-surface.hpp"

// vklib::Surface
namespace vklib
{
	Result<Surface, VkResult> Surface::create(const Instance& instance, const Window& window)
	{
		VkSurfaceKHR surface;
		auto         result = glfwCreateWindowSurface(*instance, *window, instance.allocator_ptr(), &surface);

		if (result != VK_SUCCESS)
			return result;
		else
			return Surface(surface, instance);
	}

	void Surface::clean()
	{
		if (should_delete()) vkDestroySurfaceKHR(*parent, *content, parent.allocator_ptr());
	}
}  // namespace vklib
