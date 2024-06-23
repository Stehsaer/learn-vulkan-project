#version 450

layout(location = 0) in vec3 in_position;

layout(set = 0, binding = 0) uniform Shadow_uniform
{
	mat4 shadow_matrix;
} shadow_uniform;

layout(push_constant) uniform Model {
    mat4 matrix;
} model;

void main()
{
	gl_Position = shadow_uniform.shadow_matrix * model.matrix * vec4(in_position, 1.0);
}