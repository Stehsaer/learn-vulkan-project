#include "render-target.hpp"

#pragma region "Shadow RT"

void Shadow_rt::create(
	const Environment&                     env,
	const Render_pass&                     render_pass,
	const Descriptor_pool&                 pool,
	const Descriptor_set_layout&           layout,
	const std::array<uint32_t, csm_count>& shadow_map_size
)
{
	for (auto i : Range(csm_count))
	{
		const auto resolution = shadow_map_size[i];

		// Images

		shadow_images[i] = Image(
			env.allocator,
			vk::ImageType::e2D,
			vk::Extent3D(resolution, resolution, 1),
			Shadow_pipeline::shadow_map_format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eDepthStencilAttachment,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive
		);

		shadow_image_views[i] = Image_view(
			env.device,
			shadow_images[i],
			Shadow_pipeline::shadow_map_format,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}
		);

		// Framebuffer
		shadow_framebuffers[i] = Framebuffer(env.device, render_pass, {shadow_image_views[i]}, vk::Extent3D(resolution, resolution, 1));

		// Buffer
		shadow_matrix_uniform[i] = Buffer(
			env.allocator,
			sizeof(Shadow_pipeline::Shadow_uniform),
			vk::BufferUsageFlagBits::eUniformBuffer,
			vk::SharingMode::eExclusive,
			VMA_MEMORY_USAGE_CPU_TO_GPU
		);

		// Descriptor Set
		shadow_matrix_descriptor_set[i] = Descriptor_set::create_multiple(env.device, pool, {layout})[0];

		env.debug_marker.set_object_name(shadow_images[i], std::format("Shadow Depth (Index {})", i))
			.set_object_name(shadow_image_views[i], std::format("Shadow Depth View (Index {})", i))
			.set_object_name(shadow_framebuffers[i], std::format("Shadow Framebuffer (Index {})", i))
			.set_object_name(shadow_matrix_uniform[i], std::format("Shadow Matrix Uniform Buffer (Index {})", i))
			.set_object_name(shadow_matrix_descriptor_set[i], std::format("Shadow Matrix Descriptor Set (Index {})", i));
	}
}

std::array<Descriptor_buffer_update<>, csm_count> Shadow_rt::update_uniform(const std::array<Shadow_pipeline::Shadow_uniform, csm_count>& data)
{
	for (auto i : Range(csm_count)) shadow_matrix_uniform[i] << std::span(&data[i], 1);

	std::array<Descriptor_buffer_update<>, csm_count> ret;

	for (auto i : Range(csm_count))
		ret[i] = Descriptor_buffer_update<>{
			shadow_matrix_descriptor_set[i],
			0,
			vk::DescriptorBufferInfo(shadow_matrix_uniform[i], 0, sizeof(Shadow_pipeline::Shadow_uniform))
		};

	return ret;
}

#pragma endregion

#pragma region "Gbuffer RT"

void Gbuffer_rt::create(const Environment& env, const Render_pass& render_pass, const Descriptor_pool& pool, const Descriptor_set_layout& layout)
{
	const auto extent = vk::Extent3D(env.swapchain.extent, 1);

	/* Images */

	auto create_color_attachment = [=](vk::Format format) -> std::tuple<Image, Image_view>
	{
		auto img = Image(
			env.allocator,
			vk::ImageType::e2D,
			extent,
			format,
			vk::ImageTiling::eOptimal,
			vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
			VMA_MEMORY_USAGE_GPU_ONLY,
			vk::SharingMode::eExclusive
		);

		auto view = Image_view(env.device, img, format, vk::ImageViewType::e2D, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

		return {img, view};
	};

	std::tie(normal, normal_view)     = create_color_attachment(Gbuffer_pipeline::normal_format);
	std::tie(albedo, albedo_view)     = create_color_attachment(Gbuffer_pipeline::color_format);
	std::tie(pbr, pbr_view)           = create_color_attachment(Gbuffer_pipeline::pbr_format);
	std::tie(emissive, emissive_view) = create_color_attachment(Gbuffer_pipeline::emissive_format);

	depth = Image(
		env.allocator,
		vk::ImageType::e2D,
		extent,
		Gbuffer_pipeline::depth_format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled,
		VMA_MEMORY_USAGE_GPU_ONLY,
		vk::SharingMode::eExclusive
	);

	depth_view = Image_view(
		env.device,
		depth,
		Gbuffer_pipeline::depth_format,
		vk::ImageViewType::e2D,
		{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}
	);

	// Framebuffer
	framebuffer = Framebuffer(env.device, render_pass, {normal_view, albedo_view, pbr_view, emissive_view, depth_view}, extent);

	// Uniform
	camera_uniform_buffer = Buffer(
		env.allocator,
		sizeof(Gbuffer_pipeline::Camera_uniform),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::SharingMode::eExclusive,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	camera_uniform_descriptor_set = Descriptor_set::create_multiple(env.device, pool, {layout})[0];

	env.debug_marker.set_object_name(normal, "Gbuffer Normal")
		.set_object_name(albedo, "Gbuffer Albedo")
		.set_object_name(pbr, "Gbuffer PBR")
		.set_object_name(emissive, "Gbuffer Emissive")
		.set_object_name(depth, "Gbuffer Depth")
		.set_object_name(normal_view, "Gbuffer Normal View")
		.set_object_name(albedo_view, "Gbuffer Albedo View")
		.set_object_name(pbr_view, "Gbuffer PBR View")
		.set_object_name(emissive_view, "Gbuffer Emissive View")
		.set_object_name(depth_view, "Gbuffer Depth View")
		.set_object_name(camera_uniform_buffer, "Gbuffer Camera Uniform Buffer")
		.set_object_name(framebuffer, "Gbuffer Framebuffer");
}

Descriptor_buffer_update<> Gbuffer_rt::update_uniform(const Gbuffer_pipeline::Camera_uniform& data)
{
	camera_uniform_buffer << std::span(&data, 1);

	return {
		camera_uniform_descriptor_set,
		0,
		{camera_uniform_buffer, 0, sizeof(Gbuffer_pipeline::Camera_uniform)}
	};
}

#pragma endregion

#pragma region "Lighting RT"

void Lighting_rt::create(const Environment& env, const Render_pass& render_pass, const Descriptor_pool& pool, const Descriptor_set_layout& layout)
{
	const auto extent = vk::Extent3D(env.swapchain.extent, 1);

	luminance = Image(
		env.allocator,
		vk::ImageType::e2D,
		extent,
		Lighting_pipeline::luminance_format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eStorage,
		VMA_MEMORY_USAGE_GPU_ONLY,
		vk::SharingMode::eExclusive
	);

	brightness = Image(
		env.allocator,
		vk::ImageType::e2D,
		extent,
		vk::Format::eR16Sfloat,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage,
		VMA_MEMORY_USAGE_GPU_ONLY,
		vk::SharingMode::eExclusive
	);

	luminance_view
		= Image_view(env.device, luminance, Lighting_pipeline::luminance_format, vk::ImageViewType::e2D, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

	brightness_view = Image_view(env.device, brightness, vk::Format::eR16Sfloat, vk::ImageViewType::e2D, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

	framebuffer = Framebuffer(env.device, render_pass, {luminance_view, brightness_view}, extent);

	const auto sampler_create_info = vk::SamplerCreateInfo()
										 .setAddressModeU(vk::SamplerAddressMode::eRepeat)
										 .setAddressModeV(vk::SamplerAddressMode::eRepeat)
										 .setAddressModeW(vk::SamplerAddressMode::eRepeat)
										 .setMipmapMode(vk::SamplerMipmapMode::eNearest)
										 .setAnisotropyEnable(false)
										 .setCompareEnable(false)
										 .setMinLod(0.0)
										 .setMaxLod(1.0)
										 .setMinFilter(vk::Filter::eNearest)
										 .setMagFilter(vk::Filter::eNearest);
	input_sampler = Image_sampler(env.device, sampler_create_info);

	vk::SamplerCreateInfo shadow_map_sampler_create_info;
	shadow_map_sampler_create_info.setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
		.setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
		.setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
		.setMipmapMode(vk::SamplerMipmapMode::eNearest)
		.setMagFilter(vk::Filter::eLinear)
		.setMinFilter(vk::Filter::eLinear)
		.setMaxLod(1.0)
		.setMinLod(0.0)
		// Hardware PCF
		.setCompareEnable(true)
		.setCompareOp(vk::CompareOp::eLessOrEqual)
		.setAnisotropyEnable(false)
		.setMaxAnisotropy(1.0)
		.setUnnormalizedCoordinates(false);
	shadow_map_sampler = Image_sampler(env.device, shadow_map_sampler_create_info);

	transmat_buffer = Buffer(
		env.allocator,
		sizeof(Lighting_pipeline::Params),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::SharingMode::eExclusive,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	input_descriptor_set = Descriptor_set::create_multiple(env.device, pool, {layout})[0];

	env.debug_marker.set_object_name(luminance, "Lighting Luminance")
		.set_object_name(brightness, "Lighting Brightness")
		.set_object_name(luminance_view, "Lighting Luminance View")
		.set_object_name(brightness_view, "Lighting Brightness View")
		.set_object_name(framebuffer, "Lighting Framebuffer")
		.set_object_name(input_sampler, "Lighting Sampler")
		.set_object_name(shadow_map_sampler, "Lighting Shadow Map Sampler")
		.set_object_name(transmat_buffer, "Lighting TransMat Uniform Buffer")
		.set_object_name(input_descriptor_set, "Lighting Input Descriptor Set");
}

std::array<Descriptor_image_update<>, 5> Lighting_rt::link_gbuffer(const Gbuffer_rt& gbuffer)
{
	const vk::DescriptorImageInfo normal_info{input_sampler, gbuffer.normal_view, vk::ImageLayout::eShaderReadOnlyOptimal},
		albedo_info{input_sampler, gbuffer.albedo_view, vk::ImageLayout::eShaderReadOnlyOptimal},
		pbr_info{input_sampler, gbuffer.pbr_view, vk::ImageLayout::eShaderReadOnlyOptimal},
		emissive_info{input_sampler, gbuffer.emissive_view, vk::ImageLayout::eShaderReadOnlyOptimal},
		depth_info{input_sampler, gbuffer.depth_view, vk::ImageLayout::eShaderReadOnlyOptimal};

	std::array<Descriptor_image_update<>, 5> ret;

	for (auto& pak : ret) pak.set = input_descriptor_set;

	ret[0].binding = 0, ret[0].info = depth_info;
	ret[1].binding = 1, ret[1].info = normal_info;
	ret[2].binding = 2, ret[2].info = albedo_info;
	ret[3].binding = 3, ret[3].info = pbr_info;
	ret[4].binding = 5, ret[4].info = emissive_info;

	return ret;
}

Descriptor_image_update<csm_count> Lighting_rt::link_shadow(const Shadow_rt& shadow)
{
	Descriptor_image_update<csm_count> ret;
	ret.set     = input_descriptor_set;
	ret.binding = 4;

	for (auto i : Range(csm_count)) ret.info[i] = {shadow_map_sampler, shadow.shadow_image_views[i], vk::ImageLayout::eShaderReadOnlyOptimal};

	return ret;
}

Descriptor_buffer_update<> Lighting_rt::update_uniform(const Lighting_pipeline::Params& data)
{
	transmat_buffer << std::span(&data, 1);

	Descriptor_buffer_update<> ret;
	ret.set     = input_descriptor_set;
	ret.binding = 6;
	ret.info    = {transmat_buffer, 0, sizeof(Lighting_pipeline::Params)};

	return ret;
}

#pragma endregion

#pragma region "Auto Exposure RT"

void Auto_exposure_compute_rt::create(const Environment& env, const Descriptor_pool& pool, const Auto_exposure_compute_pipeline& pipeline, uint32_t count)
{
	const auto result_staging_buffer = Buffer(
		env.allocator,
		sizeof(Auto_exposure_compute_pipeline::Exposure_result),
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferSrc,
		vk::SharingMode::eExclusive,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	const Auto_exposure_compute_pipeline::Exposure_result init_result{0, 0};
	result_staging_buffer << std::span(&init_result, 1);

	const auto medium_staging_buffer = Buffer(
		env.allocator,
		sizeof(Auto_exposure_compute_pipeline::Exposure_medium),
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferSrc,
		vk::SharingMode::eExclusive,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	Auto_exposure_compute_pipeline::Exposure_medium init_medium;
	std::fill_n(init_medium.pixel_count, 256, 0);
	medium_staging_buffer << std::span(&init_medium, 1);

	out_buffer = Buffer(
		env.allocator,
		sizeof(Auto_exposure_compute_pipeline::Exposure_result),
		vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::SharingMode::eExclusive,
		VMA_MEMORY_USAGE_GPU_ONLY
	);

	medium_buffer = Buffer(
		env.allocator,
		sizeof(Auto_exposure_compute_pipeline::Exposure_medium),
		vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
		vk::SharingMode::eExclusive,
		VMA_MEMORY_USAGE_GPU_ONLY
	);

	const std::vector<vk::DescriptorSetLayout> luminance_avg_layout(count, pipeline.luminance_avg_descriptor_set_layout);

	luminance_avg_descriptor_sets = Descriptor_set::create_multiple(env.device, pool, luminance_avg_layout);

	lerp_descriptor_set = Descriptor_set::create_multiple(env.device, pool, {pipeline.lerp_descriptor_set_layout})[0];

	const auto command_buffer = Command_buffer(env.command_pool);
	command_buffer.begin(true);
	command_buffer.copy_buffer(out_buffer, result_staging_buffer, 0, 0, sizeof(Auto_exposure_compute_pipeline::Exposure_result));
	command_buffer.copy_buffer(medium_buffer, medium_staging_buffer, 0, 0, sizeof(Auto_exposure_compute_pipeline::Exposure_medium));
	command_buffer.end();

	const auto cmd_buf_list = Command_buffer::to_array({command_buffer});
	env.t_queue.submit(vk::SubmitInfo().setCommandBuffers(cmd_buf_list));
	env.device->waitIdle();

	env.debug_marker.set_object_name(medium_buffer, "Auto Exposure Medium Buffer")
		.set_object_name(out_buffer, "Auto Exposure Output Buffer")
		.set_object_name(lerp_descriptor_set, "Auto Exposure Lerp Input Descriptor Set");

	for (auto i : Range(count))
	{
		env.debug_marker.set_object_name(
			luminance_avg_descriptor_sets[i],
			std::format("Auto Exposure Luminance Input Descriptor Set (Index {})", i)
		);
	}
}

std::tuple<
	std::vector<Descriptor_image_update<1, vk::DescriptorType::eStorageImage>>,
	std::vector<Descriptor_buffer_update<1, vk::DescriptorType::eStorageBuffer>>>
Auto_exposure_compute_rt::link_lighting(const std::vector<const Lighting_rt*>& rt)
{
	std::vector<Descriptor_image_update<1, vk::DescriptorType::eStorageImage>>   ret1;
	std::vector<Descriptor_buffer_update<1, vk::DescriptorType::eStorageBuffer>> ret2;
	ret1.reserve(rt.size());
	ret2.reserve(rt.size());

	for (auto i : Range(rt.size()))
	{
		ret1.emplace_back(luminance_avg_descriptor_sets[i], 0, vk::DescriptorImageInfo({}, rt[i]->brightness_view, vk::ImageLayout::eGeneral));

		ret2.emplace_back(
			luminance_avg_descriptor_sets[i],
			1,
			vk::DescriptorBufferInfo(medium_buffer, 0, sizeof(Auto_exposure_compute_pipeline::Exposure_medium))
		);
	}

	return {ret1, ret2};
}

std::array<Descriptor_buffer_update<1, vk::DescriptorType::eStorageBuffer>, 2> Auto_exposure_compute_rt::link_self()
{
	return {
		{{lerp_descriptor_set, 0, vk::DescriptorBufferInfo(medium_buffer, 0, sizeof(Auto_exposure_compute_pipeline::Exposure_medium))},
		 {lerp_descriptor_set, 1, vk::DescriptorBufferInfo(out_buffer, 0, sizeof(Auto_exposure_compute_pipeline::Exposure_result))}}
	};
}

#pragma endregion

#pragma region "Bloom RT"

void Bloom_rt::create(const Environment& env, const Descriptor_pool& pool, const Bloom_pipeline& pipeline)
{
	const auto extent = env.swapchain.extent;

	bloom_downsample_chain = Image(
		env.allocator,
		vk::ImageType::e2D,
		vk::Extent3D{extent, 1},
		vk::Format::eR16G16B16A16Sfloat,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
		VMA_MEMORY_USAGE_GPU_ONLY,
		vk::SharingMode::eExclusive,
		bloom_downsample_count
	);

	extents[0] = extent;
	for (auto i : Range(bloom_downsample_count))
	{
		downsample_chain_view[i] = Image_view(
			env.device,
			bloom_downsample_chain,
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eColor, i, 1, 0, 1}
		);

		if (i != 0)
			extents[i] = vk::Extent2D(extents[i - 1].width == 1 ? 1 : extents[i - 1].width / 2, extents[i - 1].height == 1 ? 1 : extents[i - 1].height / 2);
	}

	bloom_upsample_chain = Image(
		env.allocator,
		vk::ImageType::e2D,
		vk::Extent3D{extents[1], 1},
		vk::Format::eR16G16B16A16Sfloat,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
		VMA_MEMORY_USAGE_GPU_ONLY,
		vk::SharingMode::eExclusive,
		bloom_downsample_count - 2
	);

	upsample_chain_sampler = [=]
	{
		const auto sampler_create_info = vk::SamplerCreateInfo()
											 .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
											 .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
											 .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
											 .setMipmapMode(vk::SamplerMipmapMode::eNearest)
											 .setAnisotropyEnable(false)
											 .setCompareEnable(false)
											 .setMinLod(0)
											 .setMaxLod(1)
											 .setMinFilter(vk::Filter::eLinear)
											 .setMagFilter(vk::Filter::eLinear)
											 .setUnnormalizedCoordinates(false);

		return Image_sampler(env.device, sampler_create_info);
	}();

	for (auto i : Range(bloom_downsample_count - 2))
	{
		upsample_chain_view[i] = Image_view(
			env.device,
			bloom_upsample_chain,
			vk::Format::eR16G16B16A16Sfloat,
			vk::ImageViewType::e2D,
			{vk::ImageAspectFlagBits::eColor, i, 1, 0, 1}
		);
	}

	bloom_filter_descriptor_set = Descriptor_set::create_multiple(env.device, pool, {pipeline.bloom_filter_descriptor_set_layout})[0];

	// Create => bloom_blur_descriptor_sets
	{
		const std::vector<vk::DescriptorSetLayout> bloom_blur_layouts(bloom_downsample_count - 2, pipeline.bloom_blur_descriptor_set_layout);

		bloom_blur_descriptor_sets = Descriptor_set::create_multiple(env.device, pool, bloom_blur_layouts);
	}

	// Create => bloom_acc_descriptor_sets
	{
		const std::vector<vk::DescriptorSetLayout> bloom_acc_layouts(bloom_downsample_count - 2, pipeline.bloom_acc_descriptor_set_layout);

		bloom_acc_descriptor_sets = Descriptor_set::create_multiple(env.device, pool, bloom_acc_layouts);
	}

	env.debug_marker.set_object_name(bloom_upsample_chain, "Bloom Upsample Image Chain")
		.set_object_name(bloom_downsample_chain, "Bloom Downsample Image Chain");
}

std::tuple<std::array<Descriptor_image_update<1, vk::DescriptorType::eStorageImage>, 2>, Descriptor_buffer_update<>> Bloom_rt::link_bloom_filter(
	const Lighting_rt&              lighting,
	const Auto_exposure_compute_rt& exposure
)
{
	const auto link_lighting = Descriptor_image_update<1, vk::DescriptorType::eStorageImage>{
		bloom_filter_descriptor_set,
		0,
		{{}, lighting.luminance_view, vk::ImageLayout::eGeneral}
	};

	const auto link_self = Descriptor_image_update<1, vk::DescriptorType::eStorageImage>{
		bloom_filter_descriptor_set,
		1,
		{{}, downsample_chain_view[0], vk::ImageLayout::eGeneral}
	};

	const auto link_uniform = Descriptor_buffer_update<>{
		bloom_filter_descriptor_set,
		2,
		{exposure.out_buffer, 0, sizeof(Auto_exposure_compute_pipeline::Exposure_result)}
	};

	return {
		{link_lighting, link_self},
		link_uniform
	};
}

std::array<
	std::tuple<Descriptor_image_update<1, vk::DescriptorType::eStorageImage>, Descriptor_image_update<1, vk::DescriptorType::eStorageImage>>,
	bloom_downsample_count - 2>
Bloom_rt::link_bloom_blur()
{
	std::array<
		std::tuple<Descriptor_image_update<1, vk::DescriptorType::eStorageImage>, Descriptor_image_update<1, vk::DescriptorType::eStorageImage>>,
		bloom_downsample_count - 2>
		ret;

	for (auto i : Range(bloom_downsample_count - 2))
	{
		ret[i] = {
			{bloom_blur_descriptor_sets[i], 0, {{}, downsample_chain_view[i + 1], vk::ImageLayout::eGeneral}},
			{bloom_blur_descriptor_sets[i], 1, {{}, upsample_chain_view[i], vk::ImageLayout::eGeneral}      }
		};
	}

	return ret;
}

std::array<
	std::tuple<
		Descriptor_image_update<>,
		Descriptor_image_update<1, vk::DescriptorType::eStorageImage>,
		Descriptor_image_update<1, vk::DescriptorType::eStorageImage>>,
	bloom_downsample_count - 2>
Bloom_rt::link_bloom_acc()
{
	std::array<
		std::tuple<
			Descriptor_image_update<>,
			Descriptor_image_update<1, vk::DescriptorType::eStorageImage>,
			Descriptor_image_update<1, vk::DescriptorType::eStorageImage>>,
		bloom_downsample_count - 2>
		ret;

	for (auto i : Range(bloom_downsample_count - 2))
	{
		const auto src_view = i == bloom_downsample_count - 3 ? downsample_chain_view[bloom_downsample_count - 1] : upsample_chain_view[i + 1];

		ret[i] = {
			{bloom_acc_descriptor_sets[i], 0, {upsample_chain_sampler, src_view, vk::ImageLayout::eShaderReadOnlyOptimal}},
			{bloom_acc_descriptor_sets[i], 1, {{}, upsample_chain_view[i], vk::ImageLayout::eGeneral}                    },
			{bloom_acc_descriptor_sets[i], 2, {{}, downsample_chain_view[i + 1], vk::ImageLayout::eGeneral}              }
		};
	}

	return ret;
}

#pragma endregion

#pragma region "Composite RT"

void Composite_rt::create(
	const Environment& env,

	const Render_pass&           render_pass,
	const Descriptor_pool&       pool,
	const Descriptor_set_layout& layout
)
{
	composite_output = Image(
		env.allocator,
		vk::ImageType::e2D,
		vk::Extent3D{env.swapchain.extent.width, env.swapchain.extent.height, 1},
		env.swapchain.surface_format.format,
		vk::ImageTiling::eOptimal,
		vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
		VMA_MEMORY_USAGE_GPU_ONLY,
		vk::SharingMode::eExclusive
	);

	image_view = Image_view(
		env.device,
		composite_output,
		env.swapchain.surface_format.format,
		vk::ImageViewType::e2D,
		{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}
	);

	framebuffer = Framebuffer(env.device, render_pass, {image_view}, vk::Extent3D(env.swapchain.extent, 1));

	const auto sampler_create_info = vk::SamplerCreateInfo()
										 .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
										 .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
										 .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
										 .setMipmapMode(vk::SamplerMipmapMode::eNearest)
										 .setAnisotropyEnable(false)
										 .setCompareEnable(false)
										 .setMinLod(0.0)
										 .setMaxLod(1.0)
										 .setMinFilter(vk::Filter::eNearest)
										 .setMagFilter(vk::Filter::eNearest);
	input_sampler = Image_sampler(env.device, sampler_create_info);

	params_buffer = Buffer(
		env.allocator,
		sizeof(Composite_pipeline::Exposure_param),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::SharingMode::eExclusive,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	descriptor_set = Descriptor_set::create_multiple(env.device, pool, {layout})[0];

	env.debug_marker.set_object_name(composite_output, "Composite Output Texture")
		.set_object_name(params_buffer, "Composite Params Buffer")
		.set_object_name(descriptor_set, "Composite Input Descriptor Set")
		.set_object_name(framebuffer, "Composite Framebuffer")
		.set_object_name(input_sampler, "Composite Input Sampler");
}

Descriptor_image_update<> Composite_rt::link_lighting(const Lighting_rt& lighting)
{
	return {
		descriptor_set,
		0,
		{input_sampler, lighting.luminance_view, vk::ImageLayout::eShaderReadOnlyOptimal}
	};
}

Descriptor_buffer_update<> Composite_rt::link_auto_exposure(const Auto_exposure_compute_rt& rt)
{
	return {
		descriptor_set,
		2,
		{rt.out_buffer, 0, sizeof(Auto_exposure_compute_pipeline::Exposure_result)}
	};
}

Descriptor_image_update<> Composite_rt::link_bloom(const Bloom_rt& bloom)
{
	return {
		descriptor_set,
		3,
		{bloom.upsample_chain_sampler, bloom.upsample_chain_view[0], vk::ImageLayout::eShaderReadOnlyOptimal}
	};
}

Descriptor_buffer_update<> Composite_rt::update_uniform(const Composite_pipeline::Exposure_param& data)
{
	params_buffer << std::span(&data, 1);

	return {
		descriptor_set,
		1,
		{params_buffer, 0, sizeof(Composite_pipeline::Exposure_param)}
	};
}

#pragma endregion

#pragma region "Fxaa RT"

void Fxaa_rt::create(
	const Environment&           env,
	const Render_pass&           render_pass,
	const Descriptor_pool&       pool,
	const Descriptor_set_layout& layout,
	uint32_t                     idx
)
{
	image_view = env.swapchain.image_views[idx];

	framebuffer = Framebuffer(env.device, render_pass, {image_view}, vk::Extent3D(env.swapchain.extent, 1));

	const auto sampler_create_info = vk::SamplerCreateInfo()
										 .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
										 .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
										 .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
										 .setMipmapMode(vk::SamplerMipmapMode::eNearest)
										 .setAnisotropyEnable(false)
										 .setCompareEnable(false)
										 .setMinLod(0.0)
										 .setMaxLod(0.0)
										 .setMinFilter(vk::Filter::eLinear)
										 .setMagFilter(vk::Filter::eLinear)
										 .setUnnormalizedCoordinates(true);
	input_sampler = Image_sampler(env.device, sampler_create_info);

	params_buffer = Buffer(
		env.allocator,
		sizeof(Fxaa_pipeline::Params),
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::SharingMode::eExclusive,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	descriptor_set = Descriptor_set::create_multiple(env.device, pool, {layout})[0];

	env.debug_marker.set_object_name(params_buffer, "Fxaa Params Buffer")
		.set_object_name(descriptor_set, "Fxaa Input Descriptor Set")
		.set_object_name(framebuffer, "Fxaa Framebuffer")
		.set_object_name(input_sampler, "Fxaa Input Sampler");
}

Descriptor_image_update<> Fxaa_rt::link_composite(const Composite_rt& composite)
{
	return {
		descriptor_set,
		0,
		{input_sampler, composite.image_view, vk::ImageLayout::eShaderReadOnlyOptimal}
	};
}

Descriptor_buffer_update<> Fxaa_rt::update_uniform(const Fxaa_pipeline::Params& param)
{
	params_buffer << std::span(&param, 1);

	return {
		descriptor_set,
		1,
		{params_buffer, 0, sizeof(Fxaa_pipeline::Params)}
	};
}

#pragma endregion

void Render_target_set::create(const Environment& env, const Pipeline_set& pipeline, uint32_t idx)
{
	Pool_size_calculator pool_size_calculator;
	pool_size_calculator.add(Shadow_pipeline::descriptor_pool_size)
		.add(Gbuffer_pipeline::descriptor_pool_size)
		.add(Lighting_pipeline::descriptor_pool_size)
		.add(Bloom_pipeline::descriptor_pool_size)
		.add(Composite_pipeline::descriptor_pool_size)
		.add(Fxaa_pipeline::descriptor_pool_size);

	const auto descriptor_pool_sizes = pool_size_calculator.get();

	shared_pool = Descriptor_pool(env.device, descriptor_pool_sizes, 1024);

	shadow_rt.create(
		env,
		pipeline.shadow_pipeline.render_pass,
		shared_pool,
		pipeline.shadow_pipeline.descriptor_set_layout_shadow_matrix,
		shadow_map_res
	);

	gbuffer_rt.create(env, pipeline.gbuffer_pipeline.render_pass, shared_pool, pipeline.gbuffer_pipeline.descriptor_set_layout_camera);

	lighting_rt.create(env, pipeline.lighting_pipeline.render_pass, shared_pool, pipeline.lighting_pipeline.gbuffer_input_layout);

	bloom_rt.create(env, shared_pool, pipeline.bloom_pipeline);

	composite_rt.create(env, pipeline.composite_pipeline.render_pass, shared_pool, pipeline.composite_pipeline.descriptor_set_layout);

	fxaa_rt.create(env, pipeline.fxaa_pipeline.render_pass, shared_pool, pipeline.fxaa_pipeline.descriptor_set_layout, idx);
}

void Render_target_set::link(const Environment& env, const Auto_exposure_compute_rt& auto_exposure_rt)
{
	std::vector<vk::WriteDescriptorSet> write_sets;
	write_sets.reserve(128);

	const auto lighting_link_gbuffer = lighting_rt.link_gbuffer(gbuffer_rt);
	for (const auto& item : lighting_link_gbuffer) write_sets.push_back(item.write_info());

	const auto lighting_link_shadow = lighting_rt.link_shadow(shadow_rt);
	write_sets.push_back(lighting_link_shadow.write_info());

	const auto composite_link_lighting = composite_rt.link_lighting(lighting_rt);
	write_sets.push_back(composite_link_lighting.write_info());

	const auto composite_link_auto_exposure = composite_rt.link_auto_exposure(auto_exposure_rt);
	write_sets.push_back(composite_link_auto_exposure.write_info());

	const auto composite_link_bloom = composite_rt.link_bloom(bloom_rt);
	write_sets.push_back(composite_link_bloom.write_info());

	const auto [bloom_link_filter_image, bloom_link_filter_buffer] = bloom_rt.link_bloom_filter(lighting_rt, auto_exposure_rt);
	write_sets.push_back(bloom_link_filter_image[0].write_info());
	write_sets.push_back(bloom_link_filter_image[1].write_info());
	write_sets.push_back(bloom_link_filter_buffer.write_info());

	const auto bloom_link_blur = bloom_rt.link_bloom_blur();
	for (const auto& item : bloom_link_blur)
	{
		write_sets.push_back(std::get<0>(item).write_info());
		write_sets.push_back(std::get<1>(item).write_info());
	}

	const auto bloom_link_acc = bloom_rt.link_bloom_acc();
	for (const auto& item : bloom_link_acc)
	{
		write_sets.push_back(std::get<0>(item).write_info());
		write_sets.push_back(std::get<1>(item).write_info());
		write_sets.push_back(std::get<2>(item).write_info());
	}

	const auto fxaa_link_composite = fxaa_rt.link_composite(composite_rt);
	const auto fxaa_link_buffer    = fxaa_rt.update_uniform(Fxaa_pipeline::Params(env.swapchain.extent));

	write_sets.push_back(fxaa_link_composite.write_info());
	write_sets.push_back(fxaa_link_buffer.write_info());

	env.device->updateDescriptorSets(write_sets, {});
}

void Render_target_set::update(
	const Environment&                                            env,
	const std::array<Shadow_pipeline::Shadow_uniform, csm_count>& shadow_matrices,
	const Gbuffer_pipeline::Camera_uniform&                       gbuffer_camera,
	const Lighting_pipeline::Params&                              lighting_param,
	const Composite_pipeline::Exposure_param&                     composite_exposure_param
)
{
	const auto shadow_update = shadow_rt.update_uniform(shadow_matrices);

	std::array<vk::WriteDescriptorSet, csm_count> shadow_update_info;
	for (auto i : Range(csm_count)) shadow_update_info[i] = shadow_update[i].write_info();

	const auto gbuffer_update      = gbuffer_rt.update_uniform(gbuffer_camera);
	const auto gbuffer_update_info = gbuffer_update.write_info();

	const auto lighting_update      = lighting_rt.update_uniform(lighting_param);
	const auto lighting_update_info = lighting_update.write_info();

	const auto composite_update      = composite_rt.update_uniform(composite_exposure_param);
	const auto composite_update_info = composite_update.write_info();

	const auto update_info = utility::join_array(shadow_update_info, std::to_array({gbuffer_update_info, lighting_update_info, composite_update_info}));

	env.device->updateDescriptorSets(update_info, {});
}

void Render_targets::create(const Environment& env, const Pipeline_set& pipeline)
{
	/* Auto Exposure RT */

	Pool_size_calculator pool_calculator;
	pool_calculator.add(Auto_exposure_compute_pipeline::unvaried_descriptor_pool_size)
		.add(Auto_exposure_compute_pipeline::descriptor_pool_size, env.swapchain.image_count);

	const auto pool_sizes = pool_calculator.get();

	shared_pool = Descriptor_pool(env.device, pool_sizes, 16);

	auto_exposure_rt.create(env, shared_pool, pipeline.auto_exposure_pipeline, env.swapchain.image_count);

	/* Render Target Sets */

	render_target_set.clear();
	render_target_set.resize(env.swapchain.image_count);

	for (auto i : Range(env.swapchain.image_count))
	{
		render_target_set[i].create(env, pipeline, i);
		render_target_set[i].link(env, auto_exposure_rt);
	}

	link(env);

	create_sync_objects(env);
}

void Render_targets::create_sync_objects(const Environment& env)
{
	acquire_semaphore     = Semaphore(env.device);
	render_done_semaphore = Semaphore(env.device);
	next_frame_fence      = Fence(env.device, vk::FenceCreateFlagBits::eSignaled);
}

void Render_targets::link(const Environment& env)
{
	const auto lighting_rts = [this]
	{
		std::vector<const Lighting_rt*> ret;
		ret.reserve(render_target_set.size());
		for (const auto& set : render_target_set) ret.push_back(&set.lighting_rt);
		return ret;
	}();

	const auto auto_exposure_link_lighting = auto_exposure_rt.link_lighting(lighting_rts);
	const auto auto_exposure_link_self     = auto_exposure_rt.link_self();

	std::vector<vk::WriteDescriptorSet> auto_exposure_write_infos;

	for (const auto& item : std::get<0>(auto_exposure_link_lighting)) auto_exposure_write_infos.push_back(item.write_info());

	for (const auto& item : std::get<1>(auto_exposure_link_lighting)) auto_exposure_write_infos.push_back(item.write_info());

	for (const auto& item : auto_exposure_link_self) auto_exposure_write_infos.push_back(item.write_info());

	env.device->updateDescriptorSets(auto_exposure_write_infos, {});
}