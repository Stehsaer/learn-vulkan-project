#include "data-accessor.hpp"

namespace VKLIB_HPP_NAMESPACE::io::gltf
{
	void Texture::parse(const tinygltf::Image& tex, Loader_context& loader_context)
	{
		format = [](uint32_t pixel_type, uint32_t component_count)
		{
			switch (pixel_type)
			{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				switch (component_count)
				{
				case 1:
					return vk::Format::eR8Unorm;
				case 2:
					return vk::Format::eR8G8Unorm;
				case 3:
				case 4:
					return vk::Format::eR8G8B8A8Unorm;
				}
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				switch (component_count)
				{
				case 1:
					return vk::Format::eR16Unorm;
				case 2:
					return vk::Format::eR16G16Unorm;
				case 3:
				case 4:
					return vk::Format::eR16G16B16A16Unorm;
				}
				break;
			}

			// no format available
			throw error::Detailed_error(std::format("Failed to load texture, pixel_type={}, component={}", pixel_type, component_count)
			);
		}(tex.pixel_type, tex.component);

		Buffer         staging_buffer;
		vk::DeviceSize copy_size [[maybe_unused]];

		/* Create Image */

		width = tex.width, height = tex.height;
		component_count = tex.component == 3 ? 4 : tex.component;
		mipmap_levels   = algorithm::texture::log2_mipmap_level(width, height, 128);
		name            = tex.name;

		image = Image(
			loader_context.allocator,
			vk::ImageType::e2D,
			vk::Extent3D(width, height, 1),
			format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive,
			mipmap_levels
		);

		/* Transfer Data */

		if (tex.component == 3)
		{
			// 3 Component, converts to 4 Component

			// Unsigned Byte
			if (tex.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
			{
				std::vector<uint8_t> data(tex.width * tex.height * 4, 255);

				const uint8_t* const raw = tex.image.data();

				for (const auto i : Iota(tex.width * tex.height))
				{
					data[i * 4 + 0] = raw[i * 3 + 0];
					data[i * 4 + 1] = raw[i * 3 + 1];
					data[i * 4 + 2] = raw[i * 3 + 2];
				}

				staging_buffer = Buffer(
					loader_context.allocator,
					data.size(),
					vk::BufferUsageFlagBits::eTransferSrc,
					vk::SharingMode::eExclusive,
					VMA_MEMORY_USAGE_CPU_TO_GPU
				);

				copy_size = data.size();

				staging_buffer << data;
			}
			else if (tex.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)  // Unsigned Short
			{
				std::vector<uint16_t> data(tex.width * tex.height * 4, std::numeric_limits<uint16_t>::max());

				const auto* const raw = (const uint16_t*)tex.image.data();

				for (const auto i : Iota(tex.width * tex.height))
				{
					data[i * 4 + 0] = raw[i * 3 + 0];
					data[i * 4 + 1] = raw[i * 3 + 1];
					data[i * 4 + 2] = raw[i * 3 + 2];
				}

				staging_buffer = Buffer(
					loader_context.allocator,
					data.size() * sizeof(uint16_t),
					vk::BufferUsageFlagBits::eTransferSrc,
					vk::SharingMode::eExclusive,
					VMA_MEMORY_USAGE_CPU_TO_GPU
				);

				copy_size = data.size() * sizeof(uint16_t);

				staging_buffer << data;
			}
		}
		else
		{
			// non-3 components

			staging_buffer = Buffer(
				loader_context.allocator,
				tex.image.size(),
				vk::BufferUsageFlagBits::eTransferSrc,
				vk::SharingMode::eExclusive,
				VMA_MEMORY_USAGE_CPU_TO_GPU
			);

			copy_size = tex.image.size();

			staging_buffer << tex.image;
		}

		/* Submit */
		auto command_buffer = Command_buffer(loader_context.command_pool);

		command_buffer.begin();

		command_buffer.layout_transit(
			image,
			vk::ImageLayout::eUndefined,
			vk::ImageLayout::eTransferDstOptimal,
			{},
			vk::AccessFlagBits::eTransferWrite,
			vk::PipelineStageFlagBits::eTopOfPipe,
			vk::PipelineStageFlagBits::eTransfer,
			{vk::ImageAspectFlagBits::eColor, 0, mipmap_levels, 0, 1}
		);

		const vk::BufferImageCopy
			copy_info(0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0, 0, 0}, vk::Extent3D(width, height, 1));
		command_buffer.copy_buffer_to_image(image, staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

		algorithm::texture::generate_mipmap(
			command_buffer,
			image,
			width,
			height,
			mipmap_levels,
			0,
			1,
			vk::ImageLayout::eTransferDstOptimal,
			vk::ImageLayout::eShaderReadOnlyOptimal
		);

		command_buffer.end();

		loader_context.command_buffers.push_back(command_buffer);
		loader_context.staging_buffers.push_back(staging_buffer);
	}

	void Texture::generate(uint8_t value0, uint8_t value1, uint8_t value2, uint8_t value3, Loader_context& loader_context)
	{
		image = Image(
			loader_context.allocator,
			vk::ImageType::e2D,
			vk::Extent3D(1, 1, 1),
			vk::Format::eR8G8B8A8Unorm,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive
		);

		auto pixel_data = std::to_array({value0, value1, value2, value3});

		mipmap_levels = 1;
		width = 1, height = 1;
		component_count = 4;
		format          = vk::Format::eR8G8B8A8Unorm;
		name            = std::format("Generated Image ({}, {}, {}, {})", value0, value1, value2, value3);

		const Buffer staging_buffer(
			loader_context.allocator,
			4,
			vk::BufferUsageFlagBits::eTransferSrc,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		staging_buffer << pixel_data;

		auto command_buffer = Command_buffer(loader_context.command_pool);

		// Record command buffer
		command_buffer.begin();
		{
			command_buffer.layout_transit(
				image,
				vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferDstOptimal,
				{},
				vk::AccessFlagBits::eTransferWrite,
				vk::PipelineStageFlagBits::eTopOfPipe,
				vk::PipelineStageFlagBits::eTransfer,
				{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
			);

			const vk::BufferImageCopy
				copy_info(0, 0, 0, {vk::ImageAspectFlagBits::eColor, 0, 0, 1}, {0, 0, 0}, vk::Extent3D(width, height, 1));
			command_buffer.copy_buffer_to_image(image, staging_buffer, vk::ImageLayout::eTransferDstOptimal, {copy_info});

			command_buffer.layout_transit(
				image,
				vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eShaderRead,
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eAllGraphics,
				{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
			);
		}
		command_buffer.end();

		loader_context.command_buffers.push_back(command_buffer);
		loader_context.staging_buffers.push_back(staging_buffer);
	}

#pragma endregion

#pragma region "Texture View"

	Texture_view::Texture_view(
		const Loader_context&       context,
		const Texture&              texture,
		const tinygltf::Sampler&    sampler,
		const vk::ComponentMapping& components
	)
	{
		view = Image_view(
			context.device,
			texture.image,
			texture.format,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eColor, 0, texture.mipmap_levels, 0, 1},
			components
		);

		static const std::map<uint32_t, vk::SamplerAddressMode> wrap_mode_lut{
			{TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE,   vk::SamplerAddressMode::eClampToEdge   },
			{TINYGLTF_TEXTURE_WRAP_REPEAT,          vk::SamplerAddressMode::eRepeat        },
			{TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT, vk::SamplerAddressMode::eMirroredRepeat}
		};

		auto get_wrap_mode = [=](uint32_t gltf_mode)
		{
			const auto find = wrap_mode_lut.find(gltf_mode);
			if (find == wrap_mode_lut.end())
				throw Gltf_parse_error("Unknown Gltf wrap mode", std::format("{} is not a valid wrap mode", gltf_mode));

			return find->second;
		};

		static const std::map<uint32_t, std::tuple<vk::SamplerMipmapMode, vk::Filter>> min_filter_lut{
			{TINYGLTF_TEXTURE_FILTER_LINEAR,                 {vk::SamplerMipmapMode::eLinear, vk::Filter::eLinear}  },
			{TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR,   {vk::SamplerMipmapMode::eLinear, vk::Filter::eLinear}  },
			{TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST,  {vk::SamplerMipmapMode::eNearest, vk::Filter::eLinear} },
			{TINYGLTF_TEXTURE_FILTER_NEAREST,                {vk::SamplerMipmapMode::eLinear, vk::Filter::eNearest} },
			{TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR,  {vk::SamplerMipmapMode::eLinear, vk::Filter::eNearest} },
			{TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST, {vk::SamplerMipmapMode::eNearest, vk::Filter::eNearest}},
		};

		auto get_min = [=](uint32_t gltf_mode) -> std::tuple<vk::SamplerMipmapMode, vk::Filter>
		{
			const auto find = min_filter_lut.find(gltf_mode);
			if (find == min_filter_lut.end()) return {vk::SamplerMipmapMode::eLinear, vk::Filter::eLinear};

			return find->second;
		};

		static const std::map<uint32_t, vk::Filter> mag_filter_lut{
			{TINYGLTF_TEXTURE_FILTER_LINEAR,  vk::Filter::eLinear },
			{TINYGLTF_TEXTURE_FILTER_NEAREST, vk::Filter::eNearest}
		};

		auto get_mag = [=](uint32_t gltf_mode)
		{
			const auto find = mag_filter_lut.find(gltf_mode);
			if (find == mag_filter_lut.end()) return vk::Filter::eLinear;

			return find->second;
		};

		const auto [mipmap_mode, min_filter] = get_min(sampler.minFilter);
		const auto mag_filter                = get_mag(sampler.magFilter);

		const auto sampler_create_info = vk::SamplerCreateInfo()
											 .setAddressModeU(get_wrap_mode(sampler.wrapS))
											 .setAddressModeV(get_wrap_mode(sampler.wrapT))
											 .setAddressModeW(vk::SamplerAddressMode::eRepeat)
											 .setMipmapMode(mipmap_mode)
											 .setAnisotropyEnable(context.config.enable_anistropy)
											 .setMaxAnisotropy(context.config.max_anistropy)
											 .setCompareEnable(false)
											 .setMinLod(0)
											 .setMaxLod(texture.mipmap_levels)
											 .setMinFilter(min_filter)
											 .setMagFilter(mag_filter)
											 .setUnnormalizedCoordinates(false);

		this->sampler = Image_sampler(context.device, sampler_create_info);
	}

	Texture_view::Texture_view(const Loader_context& context, const Texture& texture, const vk::ComponentMapping& components)
	{
		view = Image_view(
			context.device,
			texture.image,
			texture.format,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eColor, 0, texture.mipmap_levels, 0, 1},
			components
		);

		const auto sampler_create_info = vk::SamplerCreateInfo()
											 .setAddressModeU(vk::SamplerAddressMode::eRepeat)
											 .setAddressModeV(vk::SamplerAddressMode::eRepeat)
											 .setAddressModeW(vk::SamplerAddressMode::eRepeat)
											 .setMipmapMode(vk::SamplerMipmapMode::eLinear)
											 .setAnisotropyEnable(context.config.enable_anistropy)
											 .setMaxAnisotropy(context.config.max_anistropy)
											 .setCompareEnable(false)
											 .setMinLod(0)
											 .setMaxLod(texture.mipmap_levels)
											 .setMinFilter(vk::Filter::eLinear)
											 .setMagFilter(vk::Filter::eLinear)
											 .setUnnormalizedCoordinates(false);

		this->sampler = Image_sampler(context.device, sampler_create_info);
	}

	Texture_view::Texture_view(
		const Loader_context&       context,
		const std::vector<Texture>& texture_list,
		const tinygltf::Model&      model,
		const tinygltf::Texture&    texture,
		const vk::ComponentMapping& components
	) :
		Texture_view(
			texture.sampler >= 0 ? Texture_view(context, texture_list[texture.source], model.samplers[texture.sampler], components)
								 : Texture_view(context, texture_list[texture.source], components)
		)
	{
	}
}