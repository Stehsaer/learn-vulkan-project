#include "data-accessor.hpp"

namespace VKLIB_HPP_NAMESPACE::io::gltf
{

	template <Vec3_or_quat T>
	T Animation_sampler<T>::operator[](float time) const
	{
		const auto upper = std::upper_bound(
			keyframes.begin(),
			keyframes.end(),
			time,
			[](float time, auto val)
			{
				return time < val.time;
			}
		);

		// time larger than upper bound
		if (upper == keyframes.end()) return keyframes.rbegin()->linear;

		// time smaller than lower bound
		if (upper == keyframes.begin()) return keyframes.begin()->linear;

		auto lower = upper;
		lower--;

		switch (mode)
		{
		case Interpolation_mode::Step:
			return lower->linear;

		case Interpolation_mode::Linear:
			return linear_interpolate(lower, upper, time);

		case Interpolation_mode::Cubic_spline:
			return cubic_interpolate(lower, upper, time);

		default:
			throw Animation_runtime_error(
				"Unknown interpolation mode",
				"This is very likely a case of data corruption or APPLICATION bug"
			);
		}
	}

	template <>
	glm::vec3 Animation_sampler<glm::vec3>::linear_interpolate(
		const std::set<Animation_sampler<glm::vec3>::Keyframe>::const_iterator& first,
		const std::set<Animation_sampler<glm::vec3>::Keyframe>::const_iterator& second,
		float                                                                   time
	) const
	{
		const auto& frame1 = *first;
		const auto& frame2 = *second;

		return glm::mix(frame1.linear, frame2.linear, (time - frame1.time) / (frame2.time - frame1.time));
	}

	template <>
	glm::quat Animation_sampler<glm::quat>::linear_interpolate(
		const std::set<Animation_sampler<glm::quat>::Keyframe>::const_iterator& first,
		const std::set<Animation_sampler<glm::quat>::Keyframe>::const_iterator& second,
		float                                                                   time
	) const
	{
		const auto& frame1 = *first;
		const auto& frame2 = *second;

		return glm::slerp(frame1.linear, frame2.linear, (time - frame1.time) / (frame2.time - frame1.time));
	}

	template <>
	glm::vec3 Animation_sampler<glm::vec3>::cubic_interpolate(
		const std::set<Animation_sampler<glm::vec3>::Keyframe>::const_iterator& first,
		const std::set<Animation_sampler<glm::vec3>::Keyframe>::const_iterator& second,
		float                                                                   time
	) const
	{
		const auto& frame1 = *first;
		const auto& frame2 = *second;

		const float tc = time, td = frame2.time - frame1.time, t = (tc - frame1.time) / td;
		const float t_2 = t * t, t_3 = t_2 * t;

		const glm::vec3 item1 = (2 * t_3 - 3 * t_2 + 1) * frame1.cubic.v, item2 = td * (t_3 - 2 * t_2 + t) * frame1.cubic.b,
						item3 = (-2 * t_3 + 3 * t_2) * frame2.cubic.v, item4 = td * (t_3 - t_2) * frame2.cubic.a;

		return item1 + item2 + item3 + item4;
	}

	template <>
	glm::quat Animation_sampler<glm::quat>::cubic_interpolate(
		const std::set<Animation_sampler<glm::quat>::Keyframe>::const_iterator& first,
		const std::set<Animation_sampler<glm::quat>::Keyframe>::const_iterator& second,
		float                                                                   time
	) const
	{
		const auto& frame1 = *first;
		const auto& frame2 = *second;

		const float tc = time, td = frame2.time - frame1.time, t = (tc - frame1.time) / td;
		const float t_2 = t * t, t_3 = t_2 * t;

		const glm::quat item1 = (2 * t_3 - 3 * t_2 + 1) * frame1.cubic.v, item2 = td * (t_3 - 2 * t_2 + t) * frame1.cubic.b,
						item3 = (-2 * t_3 + 3 * t_2) * frame2.cubic.v, item4 = td * (t_3 - t_2) * frame2.cubic.a;

		return glm::normalize(item1 + item2 + item3 + item4);
	}

	template <>
	void Animation_sampler<glm::vec3>::load(const tinygltf::Model& model, const tinygltf::AnimationSampler& sampler)
	{
		const static std::unordered_map<std::string, Interpolation_mode> mode_map = {
			{"LINEAR",      Interpolation_mode::Linear      },
			{"STEP",        Interpolation_mode::Step        },
			{"CUBICSPLINE", Interpolation_mode::Cubic_spline}
		};

		if (auto find = mode_map.find(sampler.interpolation); find != mode_map.end())
			mode = find->second;
		else
			throw Animation_parse_error(std::format("Invalid animation sampler interpolation mode: {}", sampler.interpolation));

		// check accessor
		if (sampler.input < 0 || sampler.output < 0) throw Animation_parse_error("Invalid animation sampler accessor index");

		// check input accessor type
		if (const auto& accessor = model.accessors[sampler.input];
			accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_SCALAR)
			throw Animation_parse_error("Invalid animation sampler input type");

		// check output accessor type
		if (const auto& accessor = model.accessors[sampler.output];
			accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC3)
			throw Animation_parse_error("Invalid animation sampler output type");

		const auto timestamp_list = data_parser::acquire_accessor<float>(model, sampler.input);
		const auto value_list     = data_parser::acquire_accessor<glm::vec3>(model, sampler.output);

		// check value size
		if (auto target = (mode == Interpolation_mode::Cubic_spline) ? timestamp_list.size() * 3 : timestamp_list.size();
			value_list.size() != target)
			throw Animation_parse_error(
				std::format("Invalid animation sampler data size: {} VEC3, target is {}", value_list.size(), target)
			);

		// store values
		if (mode == Interpolation_mode::Cubic_spline)
			// Cubic spline, 3 values per timestamp
			for (auto i : Iota(timestamp_list.size()))
			{
				keyframes
					.emplace_hint(keyframes.end(), timestamp_list[i], value_list[i * 3 + 1], value_list[i * 3], value_list[i * 3 + 2]);
			}
		else
			// Not cubic spline, 1 value per timestamp
			for (auto i : Iota(timestamp_list.size()))
			{
				keyframes.emplace_hint(keyframes.end(), timestamp_list[i], value_list[i]);
			}
	}

	template <>
	void Animation_sampler<glm::quat>::load(const tinygltf::Model& model, const tinygltf::AnimationSampler& sampler)
	{
		const static std::unordered_map<std::string, Interpolation_mode> mode_map = {
			{"LINEAR",      Interpolation_mode::Linear      },
			{"STEP",        Interpolation_mode::Step        },
			{"CUBICSPLINE", Interpolation_mode::Cubic_spline}
		};

		if (auto find = mode_map.find(sampler.interpolation); find != mode_map.end())
			mode = find->second;
		else
			throw Animation_parse_error(std::format("Invalid animation sampler interpolation mode: {}", sampler.interpolation));

		// check accessor
		if (sampler.input < 0 || sampler.output < 0) throw Animation_parse_error("Invalid animation sampler accessor index");

		// check input accessor type
		if (const auto& accessor = model.accessors[sampler.input];
			accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_SCALAR)
			throw Animation_parse_error("Invalid animation sampler input type");

		// check output accessor type
		if (const auto& accessor = model.accessors[sampler.output];
			accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != TINYGLTF_TYPE_VEC4)
			throw Animation_parse_error("Invalid animation sampler output type");

		const auto timestamp_list = data_parser::acquire_accessor<float>(model, sampler.input);
		const auto value_list     = data_parser::acquire_normalized_accessor<glm::quat>(model, sampler.output);

		// check value size
		if (auto target = (mode == Interpolation_mode::Cubic_spline) ? timestamp_list.size() * 3 : timestamp_list.size();
			value_list.size() != target)
			throw Animation_parse_error(
				std::format("Invalid animation sampler data size: {} VEC3, target is {}", value_list.size(), target)
			);

		// store values
		if (mode == Interpolation_mode::Cubic_spline)
			// Cubic spline, 3 values per timestamp
			for (auto i : Iota(timestamp_list.size()))
			{
				keyframes
					.emplace_hint(keyframes.end(), timestamp_list[i], value_list[i * 3 + 1], value_list[i * 3], value_list[i * 3 + 2]);
			}
		else
			// Not cubic spline, 1 value per timestamp
			for (auto i : Iota(timestamp_list.size()))
			{
				keyframes.emplace_hint(keyframes.end(), timestamp_list[i], value_list[i]);
			}
	}

	std::variant<Animation_sampler<glm::vec3>, Animation_sampler<glm::quat>> load_sampler(
		const tinygltf::Model&            model,
		const tinygltf::AnimationSampler& sampler
	)
	{
		const auto& output_accessor = model.accessors[sampler.output];

		switch (output_accessor.type)
		{
		case TINYGLTF_TYPE_VEC3:
		{
			Animation_sampler<glm::vec3> dst;
			dst.load(model, sampler);
			return dst;
		}

		case TINYGLTF_TYPE_VEC4:
		{
			Animation_sampler<glm::quat> dst;
			dst.load(model, sampler);
			return dst;
		}

		default:
			throw Animation_parse_error("Invalid animation sampler output accessor type");
		}
	}

	bool Animation_channel::load(const tinygltf::AnimationChannel& animation)
	{
		if (animation.target_node < 0 || animation.sampler < 0) return false;

		node    = to_optional(animation.target_node);
		sampler = to_optional(animation.sampler);

		const static std::map<std::string, Animation_target> animation_target_lut = {
			{"translation", Animation_target::Translation},
			{"rotation",    Animation_target::Rotation   },
			{"scale",       Animation_target::Scale      },
			{"weights",     Animation_target::Weights    }
		};

		if (auto find = animation_target_lut.find(animation.target_path); find != animation_target_lut.end())
			target = find->second;
		else
			throw Animation_parse_error(std::format("Invalid animation path: {}", animation.target_path));

		return true;
	}

	void Animation::load(const tinygltf::Model& model, const tinygltf::Animation& animation)
	{
		name       = animation.name;
		start_time = std::numeric_limits<float>::max();
		end_time   = -std::numeric_limits<float>::max();

		// parse samplers
		for (const auto& sampler : animation.samplers) samplers.push_back(load_sampler(model, sampler));

		auto get_time = [this](uint32_t idx) -> std::tuple<float, float>
		{
			const auto& mut = samplers[idx];
			if (mut.index() == 0)
			{
				const auto& item = std::get<0>(mut);
				return {item.start_time(), item.end_time()};
			}
			else
			{
				const auto& item = std::get<1>(mut);
				return {item.start_time(), item.end_time()};
			}
		};

		// parse channels
		for (const auto& channel : animation.channels)
		{
			Animation_channel output;
			if (!output.load(channel)) continue;

			auto [start, end] = get_time(output.sampler.value());
			start_time        = std::min(start_time, start);
			end_time          = std::max(end_time, end);

			channels.push_back(output);
		}
	}

	void Animation::set_transformation(float time, const std::function<Node_transformation&(uint32_t)>& func) const
	{
		for (const auto& channel : channels)
		{
			const auto& sampler_variant = samplers[channel.sampler.value()];

			// check type
			if ((sampler_variant.index() != 1 && channel.target == Animation_target::Rotation)
				|| (sampler_variant.index() != 0 && channel.target != Animation_target::Rotation))
			{
				throw Animation_runtime_error("Sampler Type Mismatch", "Mismatch sampler type with channel target");
			}

			auto& find = func(channel.node.value());

			switch (channel.target)
			{
			case Animation_target::Translation:
			{
				const auto& sampler = std::get<0>(sampler_variant);
				find.translation    = sampler[time];
				break;
			}
			case Animation_target::Rotation:
			{
				const auto& sampler = std::get<1>(sampler_variant);
				find.rotation       = sampler[time];
				break;
			}
			case Animation_target::Scale:
			{
				const auto& sampler = std::get<0>(sampler_variant);
				find.scale          = sampler[time];
				break;
			}
			default:
				continue;
			}
		}
	}
}