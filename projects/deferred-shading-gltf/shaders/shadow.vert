#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;

layout(set = 0, binding = 0) uniform Shadow_uniform
{
	mat4 shadow_matrix;
} shadow_uniform;

layout(push_constant) uniform Model {
    mat4 matrix;
} model;

layout(location = 0) out vec2 out_uv;

void main()
{
	gl_Position = shadow_uniform.shadow_matrix * model.matrix * vec4(in_position, 1.0);
	out_uv = in_texcoord;
}