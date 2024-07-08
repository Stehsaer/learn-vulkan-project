#version 450

layout(location = 0) out vec4 out_normal;
layout(location = 1) out vec4 out_color;
layout(location = 2) out vec4 out_pbr;
layout(location = 3) out vec4 out_emissive;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_tangent;

layout(constant_id = 0) const bool alpha_cutoff_enabled = false;
layout(constant_id = 1) const bool alpha_blend_enabled = false;

layout(set = 1, binding = 0) uniform sampler2D albedo_texture;
layout(set = 1, binding = 1) uniform sampler2D metalness_roughness_texture;
layout(set = 1, binding = 2) uniform sampler2D occlusion_texture;
layout(set = 1, binding = 3) uniform sampler2D normal_texture;
layout(set = 1, binding = 4) uniform sampler2D emissive_texture;
layout(set = 1, binding = 5) uniform Mat_params
{
	vec3 emissive_multiplier;
	vec2 metalness_roughness_multiplier;
	vec3 base_color_multiplier;
	float alpha_cutoff;
	float normal_scale;
	float occlusion_strength;
	float emissive_strength;
} mat_params;

const int bayer[64] = int[64](1, 49, 13, 61, 4, 52, 16, 64, 33, 17, 45, 29, 36, 20, 48, 32, 9, 57, 5, 53, 12, 60, 8, 56, 41, 25, 37, 21, 44, 28, 40, 24, 3, 51, 15, 63, 2, 50, 14, 62, 35, 19, 47, 31, 34, 18, 46, 30, 11, 59, 7, 55, 10, 58, 6, 54, 43, 27, 39, 23, 42, 26, 38, 22);

void main()
{
	vec4 color = texture(albedo_texture, in_uv);

	if(alpha_cutoff_enabled)
		if(color.w < mat_params.alpha_cutoff) discard;

	if(alpha_blend_enabled)
	{
		ivec2 fragcoord = ivec2(gl_FragCoord);
		int bayer_index = fragcoord.x % 8 * 8 + fragcoord.y % 8;
		
		if(color.w * 64.0 < bayer[bayer_index]) discard;
	}

	vec3 color_transformed = pow(pow(color.rgb, vec3(2.2)) * mat_params.base_color_multiplier, vec3(1 / 2.2));

	// Orthogontalize tange, Gram Schmidt Process
	vec3 bitangent	 = normalize(cross(in_tangent, in_normal));
	vec3 tangent	 = normalize(cross(in_normal, bitangent));
	vec3 normal		 = normalize(in_normal);

	vec3 sampled_normal_offset = normalize(texture(normal_texture, in_uv).xyz * 2.0 - 1.0);
	sampled_normal_offset.xy *= mat_params.normal_scale;
	mat3 TBN = mat3(tangent, bitangent, normal);
	vec3 mapped_normal = TBN * sampled_normal_offset;

	float occlusion = 1.0 + (texture(occlusion_texture, in_uv).r * mat_params.occlusion_strength - 1.0);
	vec2 roughness_metalness = texture(metalness_roughness_texture, in_uv).gb * mat_params.metalness_roughness_multiplier;

	if(!gl_FrontFacing) mapped_normal = -mapped_normal;

	out_emissive = vec4(texture(emissive_texture, in_uv).rgb * mat_params.emissive_multiplier, 1.0);
	out_normal	 = vec4(mapped_normal, mat_params.emissive_strength);
	out_color	 = vec4(color_transformed, 0.0);
	out_pbr		 = vec4(occlusion, roughness_metalness, 0.0);
}