#include "vklib-shader.hpp"

namespace vklib
{
	Result<Shader_module, VkResult> Shader_module::create(const Device& device, const std::vector<uint8_t>& code)
	{
		VkShaderModuleCreateInfo create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		create_info.pCode                    = reinterpret_cast<const uint32_t*>(code.data());
		create_info.codeSize                 = code.size();

		VkShaderModule handle;
		auto           result = vkCreateShaderModule(*device, &create_info, device.allocator_ptr(), &handle);

		if (result != VK_SUCCESS)
			return result;
		else
			return Shader_module(handle, device);
	}

	void Shader_module::clean()
	{
		if (should_delete()) vkDestroyShaderModule(*parent, *content, parent.allocator_ptr());
	}

	VkPipelineShaderStageCreateInfo Shader_module::stage_info(VkShaderStageFlagBits stage, const char* entry_name) const
	{
		VkPipelineShaderStageCreateInfo info = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		info.stage                           = stage;
		info.module                          = *content;
		info.pName                           = entry_name;
		return info;
	}

}