#version 450

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color_out;

layout(binding = 0) uniform sampler2D position_tex;
layout(binding = 1) uniform sampler2D normal_tex;
layout(binding = 2) uniform sampler2D color_tex;
layout(binding = 3) uniform sampler2D pbr_tex;
layout(binding = 4) uniform sampler2D shadow_map;

layout(binding = 5) uniform Trans_mat {
	mat4 view_projection_mat;
	mat4 shadow_mat;
} transmat;

void main()
{
	vec3 position = texture(position_tex, uv).xyz;
	vec3 normal = texture(normal_tex, uv).xyz;
	vec3 color = texture(color_tex, uv).rgb;
	vec4 pbr = texture(pbr_tex, uv);
	

	color_out = vec4(color, 1.0);
}