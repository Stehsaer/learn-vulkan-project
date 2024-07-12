#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_texcoord;
layout(location = 2) in uvec4 in_joints; // NOTE: format = u16vec4
layout(location = 3) in vec4 in_weights;

layout(set = 0, binding = 0) uniform Shadow_uniform
{
	mat4 shadow_matrix;
} shadow_uniform;

layout(set = 2, binding = 0) readonly buffer Joint_matrices
{
	mat4 data[];
} joints;

layout(push_constant) uniform Model {
    mat4 matrix;
} model;

layout(location = 0) out vec2 out_uv;

void main()
{
	mat4 blend_matrix = 
		joints.data[in_joints.x] * in_weights.x +
		joints.data[in_joints.y] * in_weights.y +
		joints.data[in_joints.z] * in_weights.z +
		joints.data[in_joints.w] * in_weights.w;

	gl_Position = shadow_uniform.shadow_matrix * blend_matrix * vec4(in_position, 1.0);
	out_uv = in_texcoord;
}