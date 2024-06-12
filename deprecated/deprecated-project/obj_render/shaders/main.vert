#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 frag_normal;
layout(location = 1) out vec2 frag_uv;

layout(binding = 0) uniform Uniform_buffer_object
{
	mat4 model, view, projection;
} ubo;

void main()
{
	gl_Position = ubo.projection * ubo.view * ubo.model * vec4(position, 1.0);

	vec4 transformed_normal = ubo.model * vec4(normal, 1.0);
	transformed_normal /= transformed_normal.w;
	frag_normal = transformed_normal.xyz;
	
	frag_uv = uv;
}
