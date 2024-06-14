#version 450

#extension GL_GOOGLE_include_directive : enable

#include "pbr.glsl"

layout(location = 0) in vec3 vpos;

layout(set = 0, binding = 0) uniform samplerCube hdri;

layout(location = 0) out vec4 out_color;

vec3 sphere_coord(float theta, float phi, vec3 normal, vec3 up, vec3 right)
{
	return normal * sin(theta) + right * cos(theta) * cos(phi) + up * cos(theta) * sin(phi);
}

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}

const uint sample_count = 4096;

layout(push_constant) uniform Params
{
	layout(offset = 64) float roughness;
	float resolution;
} params;

vec3 sample_ggx(vec2 xi, vec3 normal, vec3 up, vec3 right, float roughness)
{
	float r2 = roughness * roughness;

	float phi = 2 * PI * xi.x;

	float cos_theta = sqrt((1.0 - xi.y) / (1.0 + (r2 * r2 - 1.0) * xi.y));
	float sin_theta = sqrt(1 - cos_theta * cos_theta);

	vec3 h = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);
	
	return mat3(right, up, normal) * h;
}

void main()
{
	vec3 normal = normalize(vpos);
	normal.y = -normal.y;

	vec3 up = vec3(0, 1, 0);
	vec3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));

	vec3 luminance_sum = vec3(0);
	float weight_sum = 0;

	for(uint i = 0; i < sample_count; i++)
	{
		vec2 xi = Hammersley(i, sample_count);
		vec3 h = sample_ggx(xi, normal, up, right, params.roughness);
		vec3 light = normalize(-reflect(h, normal));

		float ggx = NDF_GGXTR(normal, h, params.roughness);
		float pdf = ggx / 4.0 + 0.0001; 

		float saTexel  = 4.0 * PI / (6.0 * params.resolution * params.resolution);
		float saSample = 1.0 / (float(sample_count) * pdf + 0.0001);
		float mipLevel = params.roughness == 0.0 ? 0.0 : log2(saSample / saTexel);

		float n_dot_l = dot(normal, light);

		if(n_dot_l > 0)
		{
			luminance_sum += n_dot_l * textureLod(hdri, light, mipLevel).rgb;
			weight_sum += n_dot_l;
		}
	}

	vec3 weighted_result = luminance_sum / weight_sum;
	out_color = vec4(weighted_result, 1.0);
}