#include "vklib-instance.hpp"

namespace vklib
{
	std::vector<VkExtensionProperties> get_supported_extensions()
	{
		uint32_t ext_count;
		vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, nullptr);
		std::vector<VkExtensionProperties> ext(ext_count);
		vkEnumerateInstanceExtensionProperties(nullptr, &ext_count, ext.data());
		return ext;
	}

	std::vector<VkLayerProperties> get_supported_layers()
	{
		uint32_t layer_count;
		vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
		std::vector<VkLayerProperties> layers(layer_count);
		vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
		return layers;
	}
}  // namespace vklib

// vklib::Instance
namespace vklib
{
	Result<Instance, VkResult> Instance::create(
		const VkApplicationInfo&             app_info,
		std::span<const char*>               extensions,
		std::span<const char*>               layers,
		std::optional<VkAllocationCallbacks> allocator
	)
	{
		// Create Info
		VkInstanceCreateInfo create_info;
		create_info.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.flags                   = 0;
		create_info.pNext                   = nullptr;
		create_info.pApplicationInfo        = &app_info;
		create_info.enabledExtensionCount   = extensions.size();
		create_info.ppEnabledExtensionNames = extensions.data();
		create_info.enabledLayerCount       = layers.size();
		create_info.ppEnabledLayerNames     = layers.data();

		// Create Instance
		VkInstance instance;
		VkResult   result = vkCreateInstance(&create_info, allocator ? &allocator.value() : nullptr, &instance);

		// Result Handling
		if (result != VK_SUCCESS) return result;

#if VKLIB_ENABLE_CPU_ALLOCATOR
		return Instance(instance).set_allocator(allocator);
#else
		return Instance(instance);
#endif
	}

	void Instance::clean()
	{
		if (should_delete()) vkDestroyInstance(*content, allocator_ptr());
	}

}  // namespace vklib