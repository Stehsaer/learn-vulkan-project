#version 450

layout(location = 0) out vec4 out_position;
layout(location = 1) out vec4 out_normal;
layout(location = 2) out vec4 out_color;
layout(location = 3) out vec4 out_pbr;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(binding = 1) uniform sampler2D albedo_texture;
layout(binding = 2) uniform sampler2D pbr_texture;

void main()
{
	out_position = vec4(in_position, 1.0);
	out_normal = vec4(in_normal, 1.0);
	out_color = texture(albedo_texture, in_uv);
	out_pbr = texture(pbr_texture, in_uv);
}