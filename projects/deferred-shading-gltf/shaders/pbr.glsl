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

// Normal Distribution Function
float NDF_GGXTR(float n_dot_h, float alpha)
{
	float nominator = alpha * alpha;
	float denominator = n_dot_h * n_dot_h * (alpha * alpha - 1.0) + 1.0;
	denominator = PI * denominator * denominator;

	return nominator / denominator;
}

float schlick_ggx(float v_dot, float k)
{
	return v_dot / (v_dot * (1.0 - k) + k);
}

// Geometry Occlusion
float geometry_occlusion(float n_dot_v, float n_dot_l, float k)
{
	return schlick_ggx(n_dot_v, k) * schlick_ggx(n_dot_l, k);
}

float k_mapping_direct(float alpha)
{
	return (alpha + 1.0) * (alpha + 1.0) / 8.0;
}

float k_mapping_ibl(float alpha)
{
	return alpha * alpha / 2.0;
}

// Fresnel without roughness
vec3 fresnel(float v_dot, vec3 F0)
{
	return F0 + (vec3(1.0) - F0) * pow(clamp(1.0 - v_dot, 0.0, 1.0), 5.0);
}

// Fresnel with roughness
vec3 fresnel_roughness(float v_dot, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - v_dot, 0.0, 1.0), 5.0);
} 

vec3 cook_torrance(float n_dot_v, float n_dot_l, float n_dot_h, float v_dot_h, float alpha, vec3 F0)
{
	float NDF = NDF_GGXTR(n_dot_h, alpha);
	float GEO = geometry_occlusion(n_dot_v, n_dot_l, k_mapping_direct(alpha));
	vec3 F = fresnel(v_dot_h, F0);

	vec3 DFG = NDF * GEO * F;

	return DFG / (4.0 * n_dot_v * n_dot_l + 0.001);
}

vec3 calculate_pbr(vec3 light_dir, vec3 light_intensity, vec3 view_dir, vec3 normal_dir, vec3 albedo, vec3 pbr)
{
	float roughness = pbr.g;
	float metalness = pbr.b;

	// Calculate base F0
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metalness);

	float alpha = roughness;

	vec3 halfway_dir = halfway_vector(light_dir, view_dir);
	float n_dot_v = max(0.0, dot(view_dir, normal_dir)), n_dot_l = max(0.0, dot(normal_dir, light_dir)), n_dot_h = max(0.0, dot(normal_dir, halfway_dir)), v_dot_h = max(0.0, dot(view_dir, halfway_dir));

	vec3 F = fresnel(v_dot_h, F0);

	// Calculate Specular/Diffuse coefficients
	vec3 k_diffuse = 1.0 - F;
	k_diffuse *= (1.0 - metalness);

	vec3 diffuse = lambert_component(albedo, k_diffuse);
	vec3 specular = cook_torrance(n_dot_v, n_dot_l, n_dot_h, v_dot_h, alpha, F0);

	vec3 color = specular + diffuse;

	color *= light_intensity * n_dot_l ;

	return color;
}