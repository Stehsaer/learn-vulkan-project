#version 450

vec2 positions[4] = vec2[](
    vec2(0.0, 0.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
	vec2(1.0, 1.0)
);

layout(location = 0) out vec2 debug_uv;

void main()
{
	gl_Position = vec4(positions[gl_VertexIndex] * 2 - 1, 0.0, 1.0);
	debug_uv = positions[gl_VertexIndex];
}