precision highp float;

const float PI = 3.1415926;

vec3 halfway_vector(vec3 light_dir, vec3 view_dir)
{
	return normalize(light_dir + view_dir);
}

vec3 lambert_component(vec3 albedo, vec3 k_diffuse)
{
	return k_diffuse * albedo / PI;
}

float NDF_GGXTR(vec3 normal_dir, vec3 halfway_dir, float alpha)
{
	float n_dot_h = max(dot(normal_dir, halfway_dir), 0.0);

	float nominator = alpha * alpha;
	float denominator = n_dot_h * n_dot_h * (alpha * alpha - 1.0) + 1.0;
	denominator = PI * denominator * denominator;

	return nominator / denominator;
}

float schlick_ggx(vec3 normal_dir, vec3 view_dir, float k)
{
	float n_dot_v = max(0.0, dot(normal_dir, view_dir));
	return n_dot_v / (n_dot_v * (1.0 - k) + k);
}

float geometry_occlusion(vec3 normal_dir, vec3 view_dir, vec3 light_dir, float k)
{
	return schlick_ggx(normal_dir, view_dir, k) * schlick_ggx(normal_dir, light_dir, k);
}

float k_mapping_direct(float alpha)
{
	return (alpha + 1.0) * (alpha + 1.0) / 8.0;
}

float k_mapping_ibl(float alpha)
{
	return alpha * alpha / 2.0;
}

vec3 fresnel(vec3 dir1, vec3 dir2, vec3 F0)
{
	return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - dot(dir1, dir2), 0.0, 1.0), 5.0);
}

vec3 fresnel_roughness(vec3 dir1, vec3 dir2, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - dot(dir1, dir2), 0.0, 1.0), 5.0);
} 

vec3 cook_torrance(vec3 view_dir, vec3 normal_dir, vec3 light_dir, float alpha, vec3 F0)
{
	vec3 halfway_dir = halfway_vector(light_dir, view_dir);
	vec3 DFG = NDF_GGXTR(normal_dir, halfway_dir, alpha) * fresnel(halfway_dir, view_dir, F0) * geometry_occlusion(normal_dir, view_dir, light_dir, k_mapping_direct(alpha));
	return DFG / (4.0 * max(dot(view_dir, normal_dir), 0.0) * max(dot(light_dir, normal_dir), 0.0) + 0.001);
}

vec3 calculate_pbr(vec3 light_dir, vec3 light_intensity, vec3 view_dir, vec3 normal_dir, vec3 albedo, vec3 pbr)
{
	// Calculate base F0
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, pbr.b);
	float alpha = pbr.g;

	vec3 halfway_dir = halfway_vector(light_dir, view_dir);
	vec3 F = fresnel(halfway_dir, view_dir, F0);

	// Calculate Specular/Diffuse coefficients
	vec3 k_diffuse = 1.0 - F;
	k_diffuse *= (1.0 - pbr.b);

	float n_dot_l = max(0.0, dot(normal_dir, light_dir));
	vec3 diffuse = lambert_component(albedo, k_diffuse);
	vec3 specular = cook_torrance(view_dir, normal_dir, light_dir, alpha, F0);

	vec3 color = specular + diffuse;

	color *= light_intensity * n_dot_l ;

	return color;
}