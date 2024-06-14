#version 450

layout(set = 1, binding = 0) uniform sampler2D albedo_texture;
layout(location = 0) in vec2 out_uv;

void main()
{
	if(texture(albedo_texture, out_uv).a < 0.8) discard;
}