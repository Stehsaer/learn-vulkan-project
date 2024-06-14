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
	vec3 camera_position;
	mat4 shadow[3];
	vec3 sunlight_pos;
	vec3 sunlight_color;
	float emissive_brightness;
	float skybox_brightness;
} transmat;

/* SET 1: Skybox & IBL */

layout(set = 1, binding = 0) uniform samplerCube skybox_cube;
layout(set = 1, binding = 1) uniform samplerCube diffuse_cube;
layout(set = 1, binding = 2) uniform sampler2D brdf_lut;

#include "pbr.glsl"

const float gamma = 2.2;
const vec3 rgb_brightness_coeff = vec3(0.21, 0.72, 0.07);

const float min_log_luminance = -10, max_log_luminance = 14;

void main()
{
	vec2 uv = naive_uv;
	uv.y = 1.0 - uv.y;

	float fragment_depth = texture(depth_tex, uv).r;
	vec4 projection_space = vec4(naive_uv * 2 - vec2(1), fragment_depth , 1.0);
	vec4 world_space = transmat.view_projection_inv * projection_space;
	vec3 position = world_space.xyz / world_space.w;
	vec3 view_direction = position - transmat.camera_position;

	vec4 normal_sampled = texture(normal_tex, uv);
	vec3 normal = normal_sampled.xyz;
	float emissive_strength = normal_sampled.a;

	vec3 color = pow(texture(color_tex, uv).rgb, vec3(gamma));
	vec3 pbr = texture(pbr_tex, uv).xyz;

	vec3 emissive = pow(texture(emissive_tex, uv).rgb, vec3(gamma)) * emissive_strength;

	pbr.g = mix(0.005, 1, pbr.g);

	if(projection_space.z == 1.0) 
	{
		vec3 env_color = textureLod(skybox_cube, view_direction, 0.0).rgb * transmat.skybox_brightness;
		float brightness = dot(env_color, rgb_brightness_coeff);
		
		luminance_out = vec4(env_color, brightness);
		brightness_out = log2(brightness);
		return;
	}

	int index = 0; vec4 shadow_coord;

	while(index < 2)
	{
		shadow_coord = transmat.shadow[index] * vec4(position, 1.0);
		shadow_coord /= shadow_coord.w;
		if(abs(shadow_coord.x) > 1.0 || abs(shadow_coord.y) > 1.0) index++;
		else 
			break;
	}
	shadow_coord = transmat.shadow[index] * vec4(position, 1.0);
	shadow_coord /= shadow_coord.w;
	shadow_coord.xy = (shadow_coord.xy * vec2(1.0, -1.0) + vec2(1.0)) / 2.0;
	float is_shadow = texture(shadow_map[index], shadow_coord.xyz);

	// vec3[] dbg = vec3[](vec3(1,0,0), vec3(0,1,0), vec3(0,0,1));
	// luminance_out = vec4(is_shadow);
	// return;

	vec3 light_dir = normalize(transmat.sunlight_pos);
	vec3 view_dir = -normalize(position - transmat.camera_position);

	vec3 accumulated_light = vec3(0.0);

	// Direct light
	accumulated_light += calculate_pbr(light_dir, transmat.sunlight_color, view_dir, normal, color, pbr);
	accumulated_light *= is_shadow;
	accumulated_light *= pbr.r;

	// Ambient light

	{
		const float max_lod = 5.0;

		vec3 reflect_dir = reflect(view_direction, normal);
		vec3 prefiltered_color = textureLod(skybox_cube, reflect_dir, pbr.g * max_lod).rgb;
		vec2 lut = texture(brdf_lut, vec2(max(0.0, dot(normal, -view_direction)))).rg;

		vec3 F0 = vec3(0.04);
		F0 = mix(F0, color, pbr.b);

		vec3 F = fresnel_roughness(normal, reflect_dir, F0, pbr.g);
		vec3 specular = prefiltered_color * (F * lut.x + lut.y);

		vec3 kS = F;
		vec3 kD = 1.0 - kS;
		kD *= 1.0 - pbr.b;    

		accumulated_light += (texture(diffuse_cube, normal).rgb * kD + specular) * pbr.r * color * transmat.skybox_brightness;
	}

	// Emissive
	accumulated_light += emissive * transmat.emissive_brightness;

	float brightness = dot(rgb_brightness_coeff, accumulated_light);
	float log_brightness = log2(brightness);
	log_brightness = clamp(log_brightness, min_log_luminance, max_log_luminance);
	
	luminance_out = vec4(accumulated_light, brightness);
	brightness_out = log_brightness;
}