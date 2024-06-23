#include <cstdint>
#include <span>

#define DEFINE_RESOURCE(name)                                                                                          \
	extern const uint8_t                  name##_data[];                                                               \
	extern const size_t                   name##_size;                                                                 \
	inline const std::span<const uint8_t> name##_span{(const uint8_t*)name##_data, name##_size};

namespace binary_resource
{
	// simple workaround
	using namespace std;

	/* Shaders */

	// SOURCE: shaders/bloom-acc.comp
	DEFINE_RESOURCE(bloom_acc_comp)

	// SOURCE: shaders/bloom-filter.comp
	DEFINE_RESOURCE(bloom_blur_comp)

	// SOURCE: shaders/bloom-filter.comp
	DEFINE_RESOURCE(bloom_filter_comp)

	// SOURCE: shaders/composite.frag
	DEFINE_RESOURCE(composite_frag)

	// SOURCE: shaders/cube.vert
	DEFINE_RESOURCE(cube_vert)

	// SOURCE: shaders/exposure-lerp.comp
	DEFINE_RESOURCE(exposure_lerp_comp)

	// SOURCE: shaders/gen-brdf.comp
	DEFINE_RESOURCE(gen_brdf_comp)

	// SOURCE: shaders/gen-cubemap.frag
	DEFINE_RESOURCE(gen_cubemap_frag)

	// SOURCE: shaders/gen-diffuse.frag
	DEFINE_RESOURCE(gen_diffuse_frag)

	// SOURCE: shaders/gen-specular.frag
	DEFINE_RESOURCE(gen_specular_frag)

	// SOURCE: shaders/gbuffer.frag
	DEFINE_RESOURCE(gbuffer_frag)

	// SOURCE: shaders/gbuffer.vert
	DEFINE_RESOURCE(gbuffer_vert)

	// SOURCE: shaders/lighting.frag
	DEFINE_RESOURCE(lighting_frag)

	// SOURCE: shaders/luminance.comp
	DEFINE_RESOURCE(luminance_comp)

	// SOURCE: shaders/quad.vert
	DEFINE_RESOURCE(quad_vert)

	// SOURCE: shaders/shadow.frag
	DEFINE_RESOURCE(shadow_frag)

	// SOURCE: shaders/shadow.vert
	DEFINE_RESOURCE(shadow_vert)

	// SOURCE: shaders/shadow-opaque.vert
	DEFINE_RESOURCE(shadow_opaque_vert)

	/* Assets */

	// SOURCE: assets/roboto.ttf
	DEFINE_RESOURCE(roboto_font)

	// SOURCE: assets/damaged-helmet.glb
	DEFINE_RESOURCE(damaged_helmet)

	// SOURCE: assets/builtin-hdr.hdr
	DEFINE_RESOURCE(builtin_hdr)
}