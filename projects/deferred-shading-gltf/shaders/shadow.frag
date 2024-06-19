#version 450

layout(set = 1, binding = 0) uniform sampler2D albedo_texture;
layout(set = 1, binding = 1) uniform Mat_params
{
	vec3 emissive_factor;
	float alpha_cutoff;
} mat_params;

layout(location = 0) in vec2 out_uv;

layout(constant_id = 0) const bool alpha_cutoff_enabled = false;

void main()
{
	if(alpha_cutoff_enabled)
		if(texture(albedo_texture, out_uv).a < mat_params.alpha_cutoff) discard;
}