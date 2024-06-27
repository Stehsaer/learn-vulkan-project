#include "controller.hpp"

template <typename T>
static T lerp(const T& target, const T& current, float speed, float dt)
{
	float mix_factor = 1 - exp(-speed * dt);
	return current * (1 - mix_factor) + target * mix_factor;
}

void Camera_controller::update(ImGuiIO& io)
{
	// Lerp
	float dt             = io.DeltaTime;
	current_pitch        = lerp(target_pitch, current_pitch, lerp_speed, dt);
	current_eye_center   = lerp(target_eye_center, current_eye_center, lerp_speed, dt);
	current_log_distance = lerp(target_log_distance, current_log_distance, lerp_speed, dt);

	float adjusted_target_yaw = target_yaw;
	if (abs(target_yaw - current_yaw) > 180) adjusted_target_yaw += round((current_yaw - target_yaw) / 360) * 360;

	current_yaw = lerp(adjusted_target_yaw, current_yaw, lerp_speed, dt);
	current_yaw = std::fmod(current_yaw, 360.0);

	// Sensitivity modifier
	float local_zoom_sensitivity = 1.0;
	if (io.KeyShift)
		local_zoom_sensitivity = 0.1;
	else if (io.KeyCtrl)
		local_zoom_sensitivity = 3;

	if (io.WantCaptureMouse) return;

	// Right button: Pan view
	if (io.MouseDown[ImGuiMouseButton_Right])
	{
		target_pitch += io.MouseDelta.y * pitch_sensitivity * local_zoom_sensitivity;
		target_yaw -= io.MouseDelta.x * yaw_sensitivity * local_zoom_sensitivity;

		target_pitch = std::clamp<float>(target_pitch, -89.5, 89.5);
		target_yaw   = std::fmod(target_yaw, 360.0);

		return;
	}

	// Left button: Rotate view
	if (io.MouseDown[ImGuiMouseButton_Left])
	{
		auto rot_mat = rotation_matrix();

		const glm::vec3 up    = rot_mat * glm::vec4(0.0, 1.0, 0.0, 0.0);
		const glm::vec3 right = rot_mat * glm::vec4(0.0, 0.0, 1.0, 0.0);

		const float     distance = exp2(target_log_distance);
		const glm::vec3 dpos     = right * distance * io.MouseDelta.x * shift_x_sensitivity * local_zoom_sensitivity
							 + up * distance * io.MouseDelta.y * shift_y_sensitivity * local_zoom_sensitivity;

		target_eye_center += dpos;
		return;
	}

	// Mouse wheel: Zoom
	if (io.MouseWheel != 0)
	{

		target_log_distance -= io.MouseWheel * zoom_sensitivity * local_zoom_sensitivity;
	}
}

glm::mat4 Camera_controller::rotation_matrix() const
{
	return glm::rotate(
		glm::rotate(glm::mat4(1.0), glm::radians(current_yaw), glm::vec3(0.0, 1.0, 0.0)),
		glm::radians(current_pitch),
		glm::vec3(0.0, 0.0, 1.0)
	);
}

glm::mat4 Camera_controller::view_matrix() const
{
	return glm::lookAt(eye_position(), current_eye_center, glm::vec3(0.0, 1.0, 0.0));
}

glm::vec3 Camera_controller::eye_position() const
{
	const float     distance     = exp2(current_log_distance);
	const glm::vec3 eye_vector   = rotation_matrix() * glm::vec4(1.0, 0.0, 0.0, 0.0) * distance;
	const glm::vec3 eye_position = current_eye_center + eye_vector;

	return eye_position;
}

glm::vec3 Camera_controller::eye_path() const
{
	return -glm::normalize(rotation_matrix() * glm::vec4(1.0, 0.0, 0.0, 0.0));
}