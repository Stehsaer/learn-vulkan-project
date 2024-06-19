#version 450

layout(location = 0) out vec3 out_position;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_uv;
layout(location = 3) out vec3 out_tangent;

layout(location = 5) out float test_z;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;
layout(location = 3) in vec3 in_tangent;

layout(set = 0, binding = 0) uniform Camera_uniform
{
	mat4 view_projection_matrix;
} camera_uniform;

layout(push_constant) uniform Params {
    mat4 matrix;
} params;

void main()
{
	vec4 model_pos = params.matrix * vec4(in_position, 1.0); // world space position
	model_pos /= model_pos.w;
	gl_Position = camera_uniform.view_projection_matrix * model_pos; // clip space position
	out_position = model_pos.xyz;
	
	vec4 trans_normal = params.matrix * vec4(in_normal, 0.0); // world space normal
	out_normal = normalize(trans_normal.xyz);
	vec4 trans_tangent = params.matrix * vec4(in_tangent, 0.0);
	out_tangent = normalize(trans_tangent.xyz);

	out_uv = in_uv; // uv coordinate
}