#version 450

// hard-coded cube vertices
const vec3[] vertices = vec3[](
	vec3(-1.0f,-1.0f,-1.0f),
    vec3(-1.0f,-1.0f, 1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(1.0f, 1.0f,-1.0f),
    vec3(-1.0f,-1.0f,-1.0f),
    vec3(-1.0f, 1.0f,-1.0f),
    vec3(1.0f,-1.0f, 1.0f),
    vec3(-1.0f,-1.0f,-1.0f),
    vec3(1.0f,-1.0f,-1.0f),
    vec3(1.0f, 1.0f,-1.0f),
    vec3(1.0f,-1.0f,-1.0f),
    vec3(-1.0f,-1.0f,-1.0f),
    vec3(-1.0f,-1.0f,-1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(-1.0f, 1.0f,-1.0f),
    vec3(1.0f,-1.0f, 1.0f),
    vec3(-1.0f,-1.0f, 1.0f),
    vec3(-1.0f,-1.0f,-1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(-1.0f,-1.0f, 1.0f),
    vec3(1.0f,-1.0f, 1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(1.0f,-1.0f,-1.0f),
    vec3(1.0f, 1.0f,-1.0f),
    vec3(1.0f,-1.0f,-1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(1.0f,-1.0f, 1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(1.0f, 1.0f,-1.0f),
    vec3(-1.0f, 1.0f,-1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(-1.0f, 1.0f,-1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(1.0f, 1.0f, 1.0f),
    vec3(-1.0f, 1.0f, 1.0f),
    vec3(1.0f,-1.0f, 1.0f)
);

layout(push_constant) uniform M
{
	mat4 mat;
} m;

layout(location = 0) out vec3 vpos;

void main()
{
	vpos = vertices[gl_VertexIndex];
	gl_Position = m.mat * vec4(vpos, 1.0);
}