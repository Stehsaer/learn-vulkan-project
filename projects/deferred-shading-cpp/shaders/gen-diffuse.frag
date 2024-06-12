#version 450

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

const float PI = 3.1415926;
const float step_theta = 0.005, step_phi = 0.01;
const float weight_mul = 1.0 / floor(0.5 * PI / step_theta) / floor(2 * PI / step_phi);
const int sample_count = 100000;

precision highp float;

void main()
{
	vec3 normal = normalize(vpos);
	normal.y = -normal.y;

	vec3 up = vec3(0, 1, 0);
	vec3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));

	vec3 luminance_sum = vec3(0.0);

	// for(float theta = 0; theta < 0.5 * PI; theta += step_theta)
	// {
	// 	for(float phi = 0; phi < 2 * PI; phi += step_phi)
	// 	{
	// 		vec3 dir = sphere_coord(theta, phi, normal, up, right);
	// 		luminance_sum += texture(hdri, dir).rgb * 0.5 * sin(2 * theta);
	// 	}
	// }

	// vec3 luminance_out = luminance_sum * weight_mul;

	for(int i = 0; i < sample_count; i++)
	{
		vec2 pt = Hammersley(i + 1, sample_count);
		pt *= vec2(0.5 * PI, 2 * PI);

		vec3 dir = sphere_coord(pt.x, pt.y, normal, up, right);

		float lod = log2(1 / ((1 - cos(pt.x)) + 0.01));

		luminance_sum += textureLod(hdri, dir, lod).rgb * 0.5 * sin(2 * pt.x);
	}

	vec3 luminance_out = luminance_sum / (sample_count);

	out_color = vec4(luminance_out, 1.0);
}