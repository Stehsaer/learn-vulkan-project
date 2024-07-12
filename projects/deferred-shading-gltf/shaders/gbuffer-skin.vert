#version 450

layout(location = 0) out vec3 out_normal;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out vec3 out_tangent;

layout(location = 5) out float test_z;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_tangent;
layout(location = 4) in uvec4 in_joints; // NOTE: format = u16vec4
layout(location = 5) in vec4 in_weights;

layout(set = 0, binding = 0) uniform Camera_uniform
{
	mat4 view_projection_matrix;
} camera_uniform;

layout(set = 2, binding = 0) readonly buffer Joint_matrices
{
	mat4 data[];
} joints;

layout(push_constant) uniform Params {
    mat4 matrix;
} params;

void main()
{
	mat4 blend_matrix = 
		joints.data[in_joints.x] * in_weights.x +
		joints.data[in_joints.y] * in_weights.y +
		joints.data[in_joints.z] * in_weights.z +
		joints.data[in_joints.w] * in_weights.w;
	
	vec4 model_pos = blend_matrix * vec4(in_position, 1.0); // world space position
	gl_Position = camera_uniform.view_projection_matrix * model_pos; // clip space position
	
	vec4 trans_normal = blend_matrix * vec4(in_normal, 0.0); // world space normal
	out_normal = normalize(trans_normal.xyz);
	
	vec4 trans_tangent = blend_matrix * vec4(in_tangent, 0.0);
	out_tangent = normalize(trans_tangent.xyz);

	out_uv = in_uv; // uv coordinate
}