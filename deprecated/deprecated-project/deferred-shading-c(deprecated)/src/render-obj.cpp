#include "application.hpp"
#include "stb_image.h"

Result<Image, img_loader::Load_result> img_loader::load_image_8bit(
	const App_environment& env,
	const std::string&     path,
	uint32_t               mip_level
)
{
	// Load Image
	std::shared_ptr<uint8_t> image_data;
	int                      width, height;

	{
		int      channel;
		uint8_t* data = stbi_load(path.c_str(), &width, &height, &channel, 4);

		if (data == nullptr) return Load_result::Read_failed;
		image_data = std::shared_ptr<uint8_t>(data, stbi_image_free);
	}

	int actual_mip_level;
	if (mip_level != 0)
		actual_mip_level = mip_level;
	else
	{
		actual_mip_level = (int)std::ceil(std::log2((float)std::min(width, height)));
	}

	// Create Image
	Image target_image;
	{
		auto result
			= Image::create(
				  env.allocator,
				  VK_IMAGE_TYPE_2D,
				  VkExtent3D(width, height, 1),
				  VK_FORMAT_R8G8B8A8_UNORM,
				  VK_IMAGE_TILING_OPTIMAL,
				  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				  VMA_MEMORY_USAGE_GPU_ONLY,
				  VK_SHARING_MODE_EXCLUSIVE,
				  actual_mip_level
			  )
			>> target_image;

		if (!result) return Load_result::Vk_failed;
	}

	// Transfer
	{
		Buffer staging_buffer;
		auto   result = Buffer::create<uint8_t>(
                          env.allocator,
                          width * height * 4,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          VMA_MEMORY_USAGE_CPU_TO_GPU
                      )
			>> staging_buffer;
		if (!result) return Load_result::Vk_failed;

		auto transfer_result = staging_buffer->allocation.transfer(image_data.get(), width * height * 4);
		if (transfer_result != VK_SUCCESS) return Load_result::Vk_failed;
	}

	// Command Buffer
	Command_buffer cmd_buffer;
	{
		auto result = Command_buffer::allocate_from(env.command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY) >> cmd_buffer;
		CHECK_RESULT(result, "Allocate Command Buffer for Texture Uploading Failed");
	}

	// Upload Image
	cmd_buffer.begin();
	{
		cmd_buffer.cmd.image_layout_transit(
			target_image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
		);
	}
}
