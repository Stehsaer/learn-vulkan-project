#version 450

layout(set = 0, binding = 0) uniform sampler2D input_tex;
layout(set = 0, binding = 1) uniform Params
{
	vec4 resolution; // XY stores 1/resolution, ZW stores resolution 
} params;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const uint mode = 1;

// Implementation based on the article from https://zhuanlan.zhihu.com/p/373379681
// Some code are directly copied from the article
// Many thanks to the author of the article and the author of FXAA -- Timothy Lottes

#define FXAA_MAX_SEARCH_STEP 5
const float[] search_step = float[](1,1.5,2,4,12);

#define FXAA_ABSOLUTE_LUMA_THRESHOLD 0.05
#define FXAA_SEARCH_END_THRESHOLD 0.07

#define MODE_NO_FXAA 0
#define MODE_FXAA_1_ORIGINAL 1
#define MODE_FXAA_1_IMPROVED 2
#define MODE_FXAA_3 3

ivec2 fragcoord = ivec2(floor(gl_FragCoord.xy));
vec2 fragcoord_f = gl_FragCoord.xy;

float luma(in vec3 color)
{
	return dot(color, vec3(0.21, 0.72, 0.07));
}

vec3 get_color(vec2 offset)
{
	return textureLod(input_tex, fragcoord_f + offset, 0.0).rgb;
}

vec3 fetch_color(ivec2 offset)
{
	return texelFetch(input_tex, fragcoord + offset, 0).rgb;
}

vec3 fxaa_1_original()
{
	vec3 colM = get_color(vec2(0, 0)), 
		colN = get_color(vec2(0, 1)),
		colE = get_color(vec2(1, 0)),
		colS = get_color(vec2(0, -1)),
		colW = get_color(vec2(-1, 0));

	float lumaM = luma(colM),
		lumaN = luma(colN),
		lumaE = luma(colE),
		lumaS = luma(colS),
		lumaW = luma(colW);

	float minNS = min(lumaN, lumaS), minWE = min(lumaW, lumaE), minLuma = min(lumaM, min(minNS, minWE));
	float maxNS = max(lumaN, lumaS), maxWE = max(lumaW, lumaE), maxLuma = max(lumaM, max(maxNS, maxWE));
	float lumaContrast = maxLuma - minLuma;

	if(lumaContrast < FXAA_ABSOLUTE_LUMA_THRESHOLD) return colM;

	float lumaGradS = lumaS - lumaM,
		lumaGradN = lumaN - lumaM,
		lumaGradW = lumaW - lumaM,
		lumaGradE = lumaE - lumaM;

	float lumaGradV = abs(lumaGradS + lumaGradN);
	float lumaGradH = abs(lumaGradW + lumaGradE);
	bool isHorz = lumaGradV > lumaGradH;

	vec2 normal = vec2(0, 0);
	if(isHorz)
	{
		normal.y = sign(abs(lumaGradN) - abs(lumaGradS));
	}
	else
	{
		normal.x = sign(abs(lumaGradE) - abs(lumaGradW));
	}

	float lumaL = (lumaN + lumaS + lumaE + lumaW) * 0.25;
	float deltaLuma = abs(lumaL - lumaM);
	float blend = deltaLuma / lumaContrast;

	vec3 target_color = get_color(normal * blend);
	return target_color;
}

vec3 fxaa_1_improved()
{
	vec3 colM = get_color(vec2(0, 0)), 
		colN = get_color(vec2(0, 1)),
		colE = get_color(vec2(1, 0)),
		colS = get_color(vec2(0, -1)),
		colW = get_color(vec2(-1, 0));

	float lumaM = luma(colM),
		lumaN = luma(colN),
		lumaE = luma(colE),
		lumaS = luma(colS),
		lumaW = luma(colW);

	float minNS = min(lumaN, lumaS), minWE = min(lumaW, lumaE), minLuma = min(lumaM, min(minNS, minWE));
	float maxNS = max(lumaN, lumaS), maxWE = max(lumaW, lumaE), maxLuma = max(lumaM, max(maxNS, maxWE));
	float lumaContrast = maxLuma - minLuma;

	if(lumaContrast < max(FXAA_ABSOLUTE_LUMA_THRESHOLD, lumaM * 0.1)) return colM;

	float lumaGradS = lumaS - lumaM,
		lumaGradN = lumaN - lumaM,
		lumaGradW = lumaW - lumaM,
		lumaGradE = lumaE - lumaM;

	float lumaGradV = abs(lumaGradS + lumaGradN);
	float lumaGradH = abs(lumaGradW + lumaGradE);
	bool isHorz = lumaGradV > lumaGradH;

	vec2 normal = vec2(0, 0);
	if(isHorz)
	{
		normal.y = sign(abs(lumaGradN) - abs(lumaGradS));
	}
	else
	{
		normal.x = sign(abs(lumaGradE) - abs(lumaGradW));
	}

	float lumaL = (lumaN + lumaS + lumaE + lumaW) * 0.25;
	float deltaLuma = abs(lumaL - lumaM);
	float blend = deltaLuma / lumaContrast;

	vec3 target_color = get_color(normal * blend);
	if(luma(target_color) > lumaM) return colM;
	return target_color;
}

vec3 fxaa_3_quality()
{
	const vec3 colM = fetch_color(ivec2(0, 0)), 
		colN = fetch_color(ivec2(0, 1)),
		colE = fetch_color(ivec2(1, 0)),
		colS = fetch_color(ivec2(0, -1)),
		colW = fetch_color(ivec2(-1, 0));

	const float lumaM = luma(colM),
		lumaN = luma(colN),
		lumaE = luma(colE),
		lumaS = luma(colS),
		lumaW = luma(colW);

	const float minNS = min(lumaN, lumaS), minWE = min(lumaW, lumaE), minLuma = min(lumaM, min(minNS, minWE));
	const float maxNS = max(lumaN, lumaS), maxWE = max(lumaW, lumaE), maxLuma = max(lumaM, max(maxNS, maxWE));
	const float lumaContrast = maxLuma - minLuma;
	const float avg = (maxLuma + minLuma) / 2;

	if(lumaContrast < FXAA_ABSOLUTE_LUMA_THRESHOLD) return colM;
	
	if(abs(lumaN - lumaS) < 0.05 * avg && abs(lumaW - lumaS) < 0.05 * avg)
		return (colN + colE + colS + colW + colM) * 0.2;

	const float lumaNW = luma(fetch_color(ivec2(-1, 1))),
		lumaNE = luma(fetch_color(ivec2(1, 1))),
		lumaSW = luma(fetch_color(ivec2(-1, -1))),
		lumaSE = luma(fetch_color(ivec2(1, -1)));

	const float lumaGradS = lumaS - lumaM,
		lumaGradN = lumaN - lumaM,
		lumaGradW = lumaW - lumaM,
		lumaGradE = lumaE - lumaM;

	const float lumaGradH = abs(lumaNW + lumaNE - 2 * lumaN) 
		+ 2 * abs(lumaW + lumaE - 2 * lumaM) 
		+ abs(lumaSW + lumaSE - 2 * lumaS); 

	const float lumaGradV = abs(lumaNW + lumaSW - 2 * lumaW)
		+ 2 * abs(lumaN + lumaS - 2 * lumaM) 
		+ abs(lumaNE + lumaSE - 2 * lumaE); 

	bool isHorz = lumaGradV > lumaGradH;

	vec2 normal = vec2(0, 0);
	vec3 blend_src;

	const float delta_luma_NS = abs(lumaGradN) - abs(lumaGradS),
		delta_luma_WE = abs(lumaGradE) - abs(lumaGradW);

	if(isHorz)
	{
		normal.y = sign(delta_luma_NS);
		blend_src = mix(colS, colN, step(0, delta_luma_NS));
	}
	else
	{
		normal.x = sign(delta_luma_WE);
		blend_src = mix(colW, colE, step(0, delta_luma_WE));
	}

	const vec2 offset = normal / 2;
	const vec2 search_dir = normal.yx;

	float luma_pos, luma_neg;
	float pos, neg;
	const float luma_start = (lumaM + luma(blend_src)) / 2;
	const float end_thres = FXAA_SEARCH_END_THRESHOLD;

	// positive direction search
	for(uint pos_search = 0; pos_search < FXAA_MAX_SEARCH_STEP; pos_search ++)
	{
		pos = search_step[pos_search];
		luma_pos = luma(get_color(offset + search_dir * pos));
		if(abs(luma_pos - luma_start) > end_thres) break;
	}

	for(uint neg_search = 0; neg_search < FXAA_MAX_SEARCH_STEP; neg_search ++)
	{
		neg = search_step[neg_search];
		luma_neg = luma(get_color(offset - search_dir * neg));
		if(abs(luma_neg - luma_start) > end_thres) break;
	}

	const float edge_length = pos + neg;
	const bool pos_closer = pos < neg;

	const float dst = pos_closer ? pos : neg;
	const float dst_luma = pos_closer ? luma_pos : luma_neg;

	float blend = 0;
	
	if((dst_luma - luma_start) * (lumaM - luma_start) <= 0)
		blend = abs(0.5 - dst / edge_length);

	return mix(colM, blend_src, blend);
}

void main()
{
	vec3 center_color;

	switch(mode)
	{
		case MODE_NO_FXAA:
			center_color = fetch_color(ivec2(0));
			break;
		case MODE_FXAA_1_ORIGINAL:
			center_color = fxaa_1_original();
			break;
		case MODE_FXAA_1_IMPROVED:
			center_color = fxaa_1_improved();
			break;
		case MODE_FXAA_3:
			center_color = fxaa_3_quality();
			break;
		default:
			center_color = fetch_color(ivec2(0));
	}

	out_color = vec4(center_color, 0.0);
}