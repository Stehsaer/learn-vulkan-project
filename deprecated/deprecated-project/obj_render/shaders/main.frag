#version 450

layout(location = 0) in vec3 frag_normal;
layout(location = 1) in vec2 frag_uv;

layout(location = 0) out vec4 color;

layout(binding = 1) uniform sampler2D obj_texture;

void main()
{
	color = texture(obj_texture, frag_uv) * max(0.1, dot(normalize(frag_normal), normalize(vec3(1, 0.5, 0.5))));
	color.a = 1.0;
	// color = pow(color, vec4(1.0 / 2.2));
}