#version 450
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec2 naive_uv;
layout(location = 0) out vec4 luminance_out;
layout(location = 1) out float brightness_out;

/* SET 0: Gbuffer Input & Parameters */

layout(set = 0, binding = 0) uniform sampler2D depth_tex;
layout(set = 0, binding = 1) uniform sampler2D normal_tex;
layout(set = 0, binding = 2) uniform sampler2D color_tex;
layout(set = 0, binding = 3) uniform sampler2D pbr_tex;
layout(set = 0, binding = 4) uniform sampler2DShadow shadow_map[3];
layout(set = 0, binding = 5) uniform sampler2D emissive_tex;

layout(std140, set = 0, binding = 6) uniform Params {
	mat4 view_projection_inv;
	mat4 shadow[3];

	vec4 shadow_sizes[3]; // XY: uv size per texel; ZW: size of the shadow view

	vec3 camera_position;
	vec3 sunlight_pos;
	vec3 sunlight_color;

	float emissive_brightness;
	float skybox_brightness;
} params;

/* SET 1: Skybox & IBL */

layout(set = 1, binding = 0) uniform samplerCube skybox_cube;
layout(set = 1, binding = 1) uniform samplerCube diffuse_cube;
layout(set = 1, binding = 2) uniform sampler2D brdf_lut;

#include "pbr.glsl"
#include "gltf-pbr.glsl"

const float gamma = 2.2;
const vec3 rgb_brightness_coeff = vec3(0.21, 0.72, 0.07);

const float min_log_luminance = -10, max_log_luminance = 14;

vec2 poisson_disk[4] = vec2[](
	vec2(-1,-2),
	vec2(-1, 1),
	vec2( 1,-1),
	vec2( 1, 1)
);

float sample_shadowmap(in int index, in vec2 uv, in float depth)
{
	float sampled_depth = texture(shadow_map[index], vec3(uv, depth)).r;
	//float is_shadow = step(depth, sampled_depth);
	return sampled_depth;
}

vec2 rotate(in vec2 v, in float theta)
{
	return mat2(vec2(cos(theta), sin(theta)), vec2(-sin(theta), cos(theta))) * v;
}

float random(in vec2 co)
{
    // Hashing function to generate a pseudo-random number based on a 2D coordinate
    float a = 12.9898;
    float b = 78.233;
    float c = 43758.5453;
    float dt= dot(co.xy, vec2(a,b));
    float sn= mod(dt,3.14);
    return fract(sin(sn) * c);
}

float get_shadow(in vec3 position)
{
	int index = 0; vec4 shadow_coord;

	// Select shadow index
	while(index < 2)
	{
		shadow_coord = params.shadow[index] * vec4(position, 1.0);
		shadow_coord /= shadow_coord.w;
		if(abs(shadow_coord.x) > 1.0 || abs(shadow_coord.y) > 1.0) index++;
		else 
			break;
	}

	// Calculate Shadow
	shadow_coord = params.shadow[index] * vec4(position, 1.0);
	shadow_coord /= shadow_coord.w;
	vec2 shadow_uv = (shadow_coord.xy * vec2(1.0, -1.0) + vec2(1.0)) / 2.0;
	float depth = shadow_coord.z;

	float rotate_offset = random(shadow_uv * 100) * PI * 2;
	vec2 sample_delta = params.shadow_sizes[index].xy;

	float shadow_accumulate = sample_shadowmap(index, shadow_uv, depth);
	int num = 1;

// #pragma unroll_loop_start
	for(int i = 0; i < 4; i++)
	{
		vec2 uv = shadow_uv + rotate(poisson_disk[i] * sample_delta, rotate_offset + i * PI / 6.0);
		if(uv.x > 1.0 || uv.x < 0.0 || uv.y > 1.0 || uv.y < 0.0) continue;

		shadow_accumulate += sample_shadowmap(index, uv, depth);
		num++;
	}
// #pragma unroll_loop_end

	return min(shadow_accumulate / num * 1.3, 1.0);
}

void main()
{
	// Convert UV coordinate
	vec2 uv = naive_uv;
	uv.y = 1.0 - uv.y;

	// View & Position
	float fragment_depth = texture(depth_tex, uv).r;
	vec4 projection_space = vec4(naive_uv * 2 - vec2(1), fragment_depth, 1.0);
	vec4 world_space = params.view_projection_inv * projection_space;
	vec3 position = world_space.xyz / world_space.w;
	vec3 view_direction = normalize(position - params.camera_position);

	// Normal & Emissive Values
	vec4 normal_sampled = texture(normal_tex, uv);
	vec3 normal = normalize(normal_sampled.xyz);
	float emissive_strength = normal_sampled.a;
	vec3 emissive = pow(texture(emissive_tex, uv).rgb, vec3(gamma)) * emissive_strength;

	// Color & PBR
	vec3 color = pow(texture(color_tex, uv).rgb, vec3(gamma));
	vec3 pbr = texture(pbr_tex, uv).xyz;

	pbr.g = mix(0.04, 1, pbr.g); // map roughness to avoid artifacts

	// depth == 1.0, skybox emitted
	if(projection_space.z == 1.0) 
	{
		vec3 env_color = textureLod(skybox_cube, view_direction, 0.0).rgb * params.skybox_brightness;
		float brightness = dot(env_color, rgb_brightness_coeff);
		
		luminance_out = vec4(env_color, brightness);
		brightness_out = log2(brightness);
		return;
	}

	float is_shadow = get_shadow(position);

	vec3 light_dir = normalize(params.sunlight_pos);

	vec3 accumulated_light = vec3(0.0);

	// Direct light
	accumulated_light += gltf_calculate_pbr(light_dir, params.sunlight_color, -view_direction, normal, color, pbr) * is_shadow;

	// Ambient light (IBL From Sky)
	{
		const float max_lod = 5.0;

		vec3 reflect_dir = reflect(view_direction, normal);
		vec3 prefiltered_color = textureLod(skybox_cube, reflect_dir, pbr.g * max_lod).rgb;

		vec2 lut = texture(brdf_lut, vec2(max(0.0, dot(normal, view_direction)))).rg;

		vec3 F0 = vec3(0.04);
		F0 = mix(F0, color, pbr.b);

		vec3 F = fresnel_roughness(max(0.0, dot(normal, reflect_dir)), F0, pbr.g);
		vec3 specular = prefiltered_color * (F * lut.x + lut.y);

		vec3 kD = (1.0 - F) * (1.0 - pbr.b);

		accumulated_light += (texture(diffuse_cube, normal).rgb * kD * color + specular * mix(color, F0, pbr.b)) * pbr.r * params.skybox_brightness;
	} 

	// Emissive
	accumulated_light += emissive * params.emissive_brightness;

	float brightness = dot(rgb_brightness_coeff, accumulated_light);
	float log_brightness = log2(brightness);
	log_brightness = clamp(log_brightness, min_log_luminance, max_log_luminance);
	
	luminance_out = vec4(accumulated_light, brightness);
	brightness_out = log_brightness;
}