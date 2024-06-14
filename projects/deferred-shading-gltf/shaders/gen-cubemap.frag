#version 450

layout(location = 0) in vec3 vpos;

layout(set = 0, binding = 0) uniform sampler2D hdri_sample;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_mipmapped;

const vec2 inv_atan = vec2(0.1591, 0.3183);

vec2 sample_sphere(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= inv_atan;
    uv += 0.5;
    return uv;
}

void main()
{
	vec2 uv = sample_sphere(normalize(vpos));
	vec4 sampled = texture(hdri_sample, uv);
	out_color = out_mipmapped = sampled;
}