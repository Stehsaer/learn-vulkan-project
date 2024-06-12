#pragma once

#include "vklib-device.hpp"

namespace vklib
{
	class Render_pass : public Base_copyable_parented<VkRenderPass, Device>
	{
		using Base_copyable_parented<VkRenderPass, Device>::Base_copyable_parented;

	  public:

		static Result<Render_pass, VkResult> create(
			const Device&                      device,
			std::span<VkAttachmentDescription> attachments,
			std::span<VkSubpassDescription>    subpasses,
			std::span<VkSubpassDependency>     dependencies
		);

		void clean() override;
		~Render_pass() override { clean(); }
	};
}