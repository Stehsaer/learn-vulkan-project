#include "binary-resource.hpp"

#define DEFINE_RESOURCE_HEAD(name) const uint8_t name##_data[] = {

#define DEFINE_RESOURCE_TAIL(name)                                                                                                    \
	}                                                                                                                                 \
	;                                                                                                                                 \
	const size_t name##_size = sizeof(name##_data);

// > Shader Replacement Strings:
// - Find:
// [\s]*// SOURCE: shaders/([A-Za-z0-9\-\.]+)[\r\n]*[\s]*DEFINE_RESOURCE\(([A-Za-z0-9_]+)\)
// - Replace:
// SOURCE: shaders/$1
// DEFINE_RESOURCE_HEAD($2)
// #include "$1.spv.h"
// DEFINE_RESOURCE_TAIL($2)

// > Assets Replacement Strings:
// - Find:
// [\s]*// SOURCE: assets/([A-Za-z0-9\-\.]+)[\r\n]*[\s]*DEFINE_RESOURCE\(([A-Za-z0-9_]+)\)
// - Replace:
// SOURCE: shaders/$1
// DEFINE_RESOURCE_HEAD($2)
// #include "$1.h"
// DEFINE_RESOURCE_TAIL($2)

namespace binary_resource
{
	// SOURCE: shaders/bloom-acc.comp
	DEFINE_RESOURCE_HEAD(bloom_acc_comp)
#include "bloom-acc.comp.spv.h"
	DEFINE_RESOURCE_TAIL(bloom_acc_comp)

	// SOURCE: shaders/bloom-blur.comp
	DEFINE_RESOURCE_HEAD(bloom_blur_comp)
#include "bloom-blur.comp.spv.h"
	DEFINE_RESOURCE_TAIL(bloom_blur_comp)

	// SOURCE: shaders/bloom-filter.comp
	DEFINE_RESOURCE_HEAD(bloom_filter_comp)
#include "bloom-filter.comp.spv.h"
	DEFINE_RESOURCE_TAIL(bloom_filter_comp)

	// SOURCE: shaders/composite.frag
	DEFINE_RESOURCE_HEAD(composite_frag)
#include "composite.frag.spv.h"
	DEFINE_RESOURCE_TAIL(composite_frag)

	// SOURCE: shaders/cube.vert
	DEFINE_RESOURCE_HEAD(cube_vert)
#include "cube.vert.spv.h"
	DEFINE_RESOURCE_TAIL(cube_vert)

	// SOURCE: shaders/exposure-lerp.comp
	DEFINE_RESOURCE_HEAD(exposure_lerp_comp)
#include "exposure-lerp.comp.spv.h"
	DEFINE_RESOURCE_TAIL(exposure_lerp_comp)

	// SOURCE: shaders/fxaa.frag
	DEFINE_RESOURCE_HEAD(fxaa_frag)
#include "fxaa.frag.spv.h"
	DEFINE_RESOURCE_TAIL(fxaa_frag)

	// SOURCE: shaders/gen-brdf.comp
	DEFINE_RESOURCE_HEAD(gen_brdf_comp)
#include "gen-brdf.comp.spv.h"
	DEFINE_RESOURCE_TAIL(gen_brdf_comp)

	// SOURCE: shaders/gen-cubemap.frag
	DEFINE_RESOURCE_HEAD(gen_cubemap_frag)
#include "gen-cubemap.frag.spv.h"
	DEFINE_RESOURCE_TAIL(gen_cubemap_frag)

	// SOURCE: shaders/gen-diffuse.frag
	DEFINE_RESOURCE_HEAD(gen_diffuse_frag)
#include "gen-diffuse.frag.spv.h"
	DEFINE_RESOURCE_TAIL(gen_diffuse_frag)

	// SOURCE: shaders/gen-specular.frag
	DEFINE_RESOURCE_HEAD(gen_specular_frag)
#include "gen-specular.frag.spv.h"
	DEFINE_RESOURCE_TAIL(gen_specular_frag)

	// SOURCE: shaders/gbuffer.frag
	DEFINE_RESOURCE_HEAD(gbuffer_frag)
#include "gbuffer.frag.spv.h"
	DEFINE_RESOURCE_TAIL(gbuffer_frag)

	// SOURCE: shaders/gbuffer.vert
	DEFINE_RESOURCE_HEAD(gbuffer_vert)
#include "gbuffer.vert.spv.h"
	DEFINE_RESOURCE_TAIL(gbuffer_vert)

	// SOURCE: shaders/gbuffer-skin.vert
	DEFINE_RESOURCE_HEAD(gbuffer_skin_vert)
#include "gbuffer-skin.vert.spv.h"
	DEFINE_RESOURCE_TAIL(gbuffer_skin_vert)

	// SOURCE: shaders/lighting.frag
	DEFINE_RESOURCE_HEAD(lighting_frag)
#include "lighting.frag.spv.h"
	DEFINE_RESOURCE_TAIL(lighting_frag)

	// SOURCE: shaders/luminance.comp
	DEFINE_RESOURCE_HEAD(luminance_comp)
#include "luminance.comp.spv.h"
	DEFINE_RESOURCE_TAIL(luminance_comp)

	// SOURCE: shaders/quad.vert
	DEFINE_RESOURCE_HEAD(quad_vert)
#include "quad.vert.spv.h"
	DEFINE_RESOURCE_TAIL(quad_vert)

	// SOURCE: shaders/shadow.frag
	DEFINE_RESOURCE_HEAD(shadow_frag)
#include "shadow.frag.spv.h"
	DEFINE_RESOURCE_TAIL(shadow_frag)

	// SOURCE: shaders/shadow.vert
	DEFINE_RESOURCE_HEAD(shadow_vert)
#include "shadow.vert.spv.h"
	DEFINE_RESOURCE_TAIL(shadow_vert)

	// SOURCE: shaders/shadow-opaque.vert
	DEFINE_RESOURCE_HEAD(shadow_opaque_vert)
#include "shadow-opaque.vert.spv.h"
	DEFINE_RESOURCE_TAIL(shadow_opaque_vert)

	// SOURCE: shaders/shadow-skin.vert
	DEFINE_RESOURCE_HEAD(shadow_skin_vert)
#include "shadow-skin.vert.spv.h"
	DEFINE_RESOURCE_TAIL(shadow_skin_vert)

	// SOURCE: shaders/shadow-skin-opaque.vert
	DEFINE_RESOURCE_HEAD(shadow_skin_opaque_vert)
#include "shadow-skin-opaque.vert.spv.h"
	DEFINE_RESOURCE_TAIL(shadow_skin_opaque_vert)

	/* Assets */

	// SOURCE: shaders/roboto.ttf
	DEFINE_RESOURCE_HEAD(roboto_font)
#include "roboto.ttf.h"
	DEFINE_RESOURCE_TAIL(roboto_font)

	// SOURCE: shaders/damaged-helmet.glb
	DEFINE_RESOURCE_HEAD(damaged_helmet)
#include "damaged-helmet.glb.h"
	DEFINE_RESOURCE_TAIL(damaged_helmet)

	// SOURCE: shaders/builtin-hdr.hdr
	DEFINE_RESOURCE_HEAD(builtin_hdr)
#include "builtin-hdr.hdr.h"
	DEFINE_RESOURCE_TAIL(builtin_hdr)

}
