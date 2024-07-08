#include "logic.hpp"

void Scene_preset::collect(const Application_logic_base& src)
{
	const auto& core = *src.core;

	hdri_path  = core.source.hdri_path;
	model_path = core.source.model_path;

	collect(core.params);
}

void Scene_preset::collect(const Render_params& src)
{
	exposure_ev         = src.exposure_ev;
	emissive_brightness = src.emissive_brightness;
	skybox_brightness   = src.skybox_brightness;
	bloom_start         = src.bloom_start;
	bloom_end           = src.bloom_end;
	bloom_intensity     = src.bloom_intensity;
	adapt_speed         = src.adapt_speed;
	csm_blend_factor    = src.csm_blend_factor;
	fov                 = src.fov;

	const auto& camera = src.camera_controller;

	std::tie(camera_yaw, camera_pitch, camera_log_distance, camera_eye_center)
		= std::tie(camera.target_yaw, camera.target_pitch, camera.target_log_distance, camera.target_eye_center);

	light_color     = src.sun.color;
	light_intensity = src.sun.intensity;
	sunlight_yaw    = src.sun.yaw;
	sunlight_pitch  = src.sun.pitch;
}

nlohmann::json Scene_preset::serialize() const
{
	nlohmann::json j;

	j["hdri_path"]  = hdri_path;
	j["model_path"] = model_path;

	j["exposure_ev"]         = exposure_ev;
	j["emissive_brightness"] = emissive_brightness;
	j["skybox_brightness"]   = skybox_brightness;
	j["bloom_start"]         = bloom_start;
	j["bloom_end"]           = bloom_end;
	j["bloom_intensity"]     = bloom_intensity;
	j["adapt_speed"]         = adapt_speed;
	j["csm_blend_factor"]    = csm_blend_factor;
	j["fov"]                 = fov;

	auto& camera           = j["camera"];
	camera["yaw"]          = camera_yaw;
	camera["pitch"]        = camera_pitch;
	camera["log_distance"] = camera_log_distance;
	camera["eye_center"]   = {
        {"x", camera_eye_center.x},
        {"y", camera_eye_center.y},
        {"z", camera_eye_center.z}
    };

	auto& light    = j["light"];
	light["color"] = {
		{"x", light_color.x},
		{"y", light_color.y},
		{"z", light_color.z}
	};
	light["intensity"] = light_intensity;
	light["yaw"]       = sunlight_yaw;
	light["pitch"]     = sunlight_pitch;

	return j;
}

void Scene_preset::deserialize(const nlohmann::json& json)
{
	hdri_path  = json["hdri_path"];
	model_path = json["model_path"];

	exposure_ev         = json["exposure_ev"];
	emissive_brightness = json["emissive_brightness"];
	skybox_brightness   = json["skybox_brightness"];
	bloom_start         = json["bloom_start"];
	bloom_end           = json["bloom_end"];
	bloom_intensity     = json["bloom_intensity"];
	adapt_speed         = json["adapt_speed"];
	csm_blend_factor    = json["csm_blend_factor"];
	fov                 = json["fov"];

	const auto& camera  = json["camera"];
	camera_yaw          = camera["yaw"];
	camera_pitch        = camera["pitch"];
	camera_log_distance = camera["log_distance"];
	camera_eye_center   = {camera["eye_center"]["x"], camera["eye_center"]["y"], camera["eye_center"]["z"]};

	const auto& light = json["light"];
	light_color.x     = light["color"]["x"];
	light_color.y     = light["color"]["y"];
	light_color.z     = light["color"]["z"];
	light_intensity   = light["intensity"];
	sunlight_yaw      = light["yaw"];
	sunlight_pitch    = light["pitch"];
}

void Scene_preset::assign(Application_logic_base& dst) const
{
	dst.core->source.hdri_path  = hdri_path;
	dst.core->source.model_path = model_path;

	assign(dst.core->params);
}

void Scene_preset::assign(Render_params& dst) const
{
	dst.exposure_ev         = exposure_ev;
	dst.emissive_brightness = emissive_brightness;
	dst.skybox_brightness   = skybox_brightness;
	dst.bloom_start         = bloom_start;
	dst.bloom_end           = bloom_end;
	dst.bloom_intensity     = bloom_intensity;
	dst.adapt_speed         = adapt_speed;
	dst.csm_blend_factor    = csm_blend_factor;
	dst.fov                 = fov;

	auto& camera = dst.camera_controller;
	std::tie(camera.target_yaw, camera.target_pitch, camera.target_log_distance, camera.target_eye_center)
		= std::tie(camera_yaw, camera_pitch, camera_log_distance, camera_eye_center);

	dst.sun.color     = light_color;
	dst.sun.intensity = light_intensity;
	dst.sun.yaw       = sunlight_yaw;
	dst.sun.pitch     = sunlight_pitch;
}