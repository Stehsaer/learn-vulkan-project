#pragma once

#include "environment.hpp"
#include <vklib>

constexpr uint32_t environment_layers = 6;

struct Hdri_resource
{
	Image         environment, diffuse, brdf_lut;
	Image_view    environment_view, diffuse_view, brdf_lut_view;
	Image_sampler bilinear_sampler, mipmapped_sampler, lut_sampler;

	Descriptor_pool descriptor_pool;
	Descriptor_set  descriptor_set;

	void generate(
		const Environment&           env,
		const Image_view&            input_image,
		uint32_t                     resolution,
		const Descriptor_set_layout& layout
	)
	{
		create_sampler(env);
		generate_environment(env, input_image, resolution);
		generate_diffuse(env, 32);
		generate_specular(env, resolution);
		generate_brdf_lut(env, 256);
		generate_descriptors(env.device, layout);

		mipmapped_environment.explicit_destroy();
		mipmapped_environment_view.explicit_destroy();
		mipmapped_environment_sampler.explicit_destroy();
	}

	void generate_descriptors(const Device& device, const Descriptor_set_layout& layout);

  private:

	Image         mipmapped_environment;
	Image_view    mipmapped_environment_view;
	Image_sampler mipmapped_environment_sampler;

	struct Push_constant
	{
		glm::mat4 transformation;
	};

	void create_sampler(const Environment& env);
	void generate_environment(const Environment& env, const Image_view& input_image, uint32_t resolution);
	void generate_diffuse(const Environment& env, uint32_t resolution);
	void generate_specular(const Environment& env, uint32_t resolution);
	void generate_brdf_lut(const Environment& env, uint32_t resolution);
};