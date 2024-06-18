#version 450

layout(location = 0) out vec4 out_normal;
layout(location = 1) out vec4 out_color;
layout(location = 2) out vec4 out_pbr;
layout(location = 3) out vec4 out_emissive;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_tangent;

layout(set = 1, binding = 0) uniform sampler2D albedo_texture;
layout(set = 1, binding = 1) uniform sampler2D metalness_roughness_texture;
layout(set = 1, binding = 2) uniform sampler2D occlusion_texture;
layout(set = 1, binding = 3) uniform sampler2D normal_texture;
layout(set = 1, binding = 4) uniform sampler2D emissive_texture;
layout(set = 1, binding = 5) uniform Mat_params
{
	vec3 emissive_strength;
} mat_params;

const float gamma = 2.2;

void main()
{
	vec4 color = texture(albedo_texture, in_uv);

	if(color.w < 0.8) discard;


	// Orthogontalize tange, Gram Schmidt Process
	vec3 bitangent = normalize(cross(in_tangent, in_normal));
	vec3 tangent = normalize(cross(in_normal, bitangent));
	vec3 normal = normalize(in_normal);

	vec3 sampled_normal_offset = normalize((texture(normal_texture, in_uv).xyz - vec3(0.5, 0.5, 0.0)) * vec3(2.0, 2.0, 1.0));
	mat3 TBN = mat3(tangent, bitangent, normal);

	vec3 mapped_normal = TBN * sampled_normal_offset;

	if(!gl_FrontFacing) mapped_normal = -mapped_normal;

	out_emissive = vec4(texture(emissive_texture, in_uv).rgb, 1.0);

	out_normal = vec4(mapped_normal, mat_params.emissive_strength);
	out_color = vec4(color.xyz, 0.0);
	out_pbr = vec4(texture(occlusion_texture, in_uv).r, texture(metalness_roughness_texture, in_uv).rg, 0.0);
}