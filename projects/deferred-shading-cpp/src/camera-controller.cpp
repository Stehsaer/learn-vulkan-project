#include "application.hpp"

void Camera_controller::update(ImGuiIO& io)
{
	if (io.WantCaptureMouse) return;

	if (io.MouseDown[ImGuiMouseButton_Right])
	{
		pitch += io.MouseDelta.y * pitch_sensitivity;
		yaw -= io.MouseDelta.x * yaw_sensitivity;

		pitch = std::clamp<float>(pitch, -89.5, 89.5);
		yaw   = std::fmod(yaw, 360.0);

		return;
	}

	if (io.MouseDown[ImGuiMouseButton_Left])
	{
		auto rot_mat = rotation_matrix();

		const glm::vec3 up    = rot_mat * glm::vec4(0.0, 1.0, 0.0, 0.0);
		const glm::vec3 right = rot_mat * glm::vec4(0.0, 0.0, 1.0, 0.0);

		const float     distance = exp2(log_distance);
		const glm::vec3 dpos     = right * distance * io.MouseDelta.x * shift_x_sensitivity
			+ up * distance * io.MouseDelta.y * shift_y_sensitivity;

		eye_center += dpos;

		return;
	}

	if (io.MouseWheel != 0)
	{
		log_distance -= io.MouseWheel * zoom_sensitivity;
	}
}

glm::mat4 Camera_controller::rotation_matrix() const
{
	return glm::rotate(
		glm::rotate(glm::mat4(1.0), glm::radians(yaw), glm::vec3(0.0, 1.0, 0.0)),
		glm::radians(pitch),
		glm::vec3(0.0, 0.0, 1.0)
	);
}

glm::mat4 Camera_controller::view_matrix() const
{
	return glm::lookAt(eye_position(), eye_center, glm::vec3(0.0, 1.0, 0.0));
}

glm::vec3 Camera_controller::eye_position() const
{
	const float     distance     = exp2(log_distance);
	const glm::vec3 eye_vector   = rotation_matrix() * glm::vec4(1.0, 0.0, 0.0, 0.0) * distance;
	const glm::vec3 eye_position = eye_center + eye_vector;

	return eye_position;
}

glm::vec3 Camera_controller::eye_path() const
{
	return -glm::normalize(rotation_matrix() * glm::vec4(1.0, 0.0, 0.0, 0.0));
}