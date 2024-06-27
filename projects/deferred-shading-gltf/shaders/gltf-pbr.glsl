precision highp float;

#ifndef PI_DEFINED
const float PI = 3.1415926;
#define PI_DEFINED
#endif

float gltf_microfacet_distribution(float n_dot_h, float alpha)
{
	float nominator = alpha * alpha * n_dot_h;
	float denominator = PI * pow(n_dot_h * n_dot_h * (alpha * alpha - 1.0) + 1.0, 2.0);

	return nominator / denominator;
}

float gltf_mask_shadowing(float vdot, float alpha)
{
	float nominator = mix(0.0001, 2.0, vdot);
	float alpha2 = alpha * alpha;
	float denominator = vdot + sqrt(alpha2 + (1.0 - alpha2) * vdot * vdot);

	return nominator / denominator;
}

float gltf_geometry_occlusion(float n_dot_l, float n_dot_v, float alpha)
{
	return gltf_mask_shadowing(n_dot_l, alpha) * gltf_mask_shadowing(n_dot_v, alpha);
}

vec3 gltf_calculate_pbr(vec3 light_dir, vec3 light_intensity, vec3 view_dir, vec3 normal_dir, vec3 albedo, vec3 pbr)
{
	float roughness = pbr.g;
	float metalness = pbr.b;

	vec3 halfway_dir = normalize(light_dir + view_dir);
	float n_dot_v = max(0.0, dot(view_dir, normal_dir)), n_dot_l = max(0.0, dot(normal_dir, light_dir)), n_dot_h = max(0.0, dot(normal_dir, halfway_dir)), v_dot_h = max(0.0, dot(view_dir, halfway_dir));

	vec3 c_diff = mix(albedo, vec3(0.0), metalness);
	vec3 f0 = mix(vec3(0.04), albedo, metalness);
	float alpha = roughness * roughness;

	vec3 F = f0 + (1.0 - f0) * pow(1.0 - v_dot_h, 5.0);

	vec3 f_diffuse = (1.0 - F) * (1.0 / PI) * c_diff;
	vec3 f_specular = F * gltf_microfacet_distribution(n_dot_h, alpha) * gltf_geometry_occlusion(n_dot_l, n_dot_v, alpha) / (4.0 * abs(n_dot_v) * abs(n_dot_l) + 0.001);

	return (f_diffuse + f_specular) * n_dot_l * light_intensity;
}