#version 450

layout(set = 1, binding = 0) uniform sampler2D albedo_texture;
layout(set = 1, binding = 1) uniform Mat_params
{
	vec3 emissive_factor;
	float alpha_cutoff;
} mat_params;

layout(location = 0) in vec2 out_uv;

layout(constant_id = 0) const bool alpha_cutoff_enabled = false;
layout(constant_id = 1) const bool alpha_blend_enabled = false;

const int bayer[64] = int[64](1, 49, 13, 61, 4, 52, 16, 64, 33, 17, 45, 29, 36, 20, 48, 32, 9, 57, 5, 53, 12, 60, 8, 56, 41, 25, 37, 21, 44, 28, 40, 24, 3, 51, 15, 63, 2, 50, 14, 62, 35, 19, 47, 31, 34, 18, 46, 30, 11, 59, 7, 55, 10, 58, 6, 54, 43, 27, 39, 23, 42, 26, 38, 22);

void main()
{
	if(alpha_cutoff_enabled)
		if(texture(albedo_texture, out_uv).a < mat_params.alpha_cutoff) discard;

	if(alpha_blend_enabled)
	{
		ivec2 fragcoord = ivec2(gl_FragCoord);
		int bayer_index = fragcoord.x % 8 * 8 + fragcoord.y % 8;
		
		if(texture(albedo_texture, out_uv).a * 64.0 < bayer[bayer_index]) discard;
	}

}