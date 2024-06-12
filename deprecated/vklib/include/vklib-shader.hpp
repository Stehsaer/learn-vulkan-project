#pragma once

#include "vklib-device.hpp"

namespace vklib
{
	class Shader_module : public Base_copyable_parented<VkShaderModule, Device>
	{
		using Base_copyable_parented<VkShaderModule, Device>::Base_copyable_parented;

	  public:
		static Result<Shader_module, VkResult> create(
			const Device& device, const std::vector<uint8_t>& code
		);

		void clean() override;
		~Shader_module() override { clean(); }

		VkPipelineShaderStageCreateInfo stage_info(
			VkShaderStageFlagBits stage, const char* entry_name = "main"
		) const;
	};
}