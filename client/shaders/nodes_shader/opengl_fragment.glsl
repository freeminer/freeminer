uniform sampler2D baseTexture;
uniform sampler2D normalTexture;
uniform sampler2D textureFlags;

uniform vec4 skyBgColor;
uniform float fogDistance;
uniform vec3 eyePosition;
uniform vec3 sunPosition;

uniform float wieldLight;

varying vec3 vPosition;
varying vec3 worldPosition;
varying float area_enable_parallax;

varying vec3 eyeVec;
varying vec3 tsEyeVec;
varying vec3 lightVec;
varying vec3 tsLightVec;

bool normalTexturePresent = false;

const float e = 2.718281828459;
const float BS = 1.0;

#ifdef ENABLE_TONE_MAPPING

/* Hable's UC2 Tone mapping parameters
	A = 0.22;
	B = 0.30;
	C = 0.10;
	D = 0.20;
	E = 0.01;
	F = 0.30;
	W = 11.2;
	equation used:  ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F
*/

vec3 uncharted2Tonemap(vec3 x)
{
	return ((x * (0.22 * x + 0.03) + 0.002) / (x * (0.22 * x + 0.3) + 0.06)) - 0.03334;
}

vec4 applyToneMapping(vec4 color)
{
	color = vec4(pow(color.rgb, vec3(2.2)), color.a);
	const float gamma = 1.6;
	const float exposureBias = 5.5;
	color.rgb = uncharted2Tonemap(exposureBias * color.rgb);
	// Precalculated white_scale from 
	//vec3 whiteScale = 1.0 / uncharted2Tonemap(vec3(W));
	vec3 whiteScale = vec3(1.036015346);
	color.rgb *= whiteScale;
	return vec4(pow(color.rgb, vec3(1.0 / gamma)), color.a);
}
#endif

void get_texture_flags()
{
	vec4 flags = texture2D(textureFlags, vec2(0.0, 0.0));
	if (flags.r > 0.5) {
		normalTexturePresent = true;
	}
}

float intensity(vec3 color)
{
	return (color.r + color.g + color.b) / 3.0;
}

float get_rgb_height(vec2 uv)
{
	return intensity(texture2D(baseTexture, uv).rgb);
}

vec4 get_normal_map(vec2 uv)
{
	vec4 bump = texture2D(normalTexture, uv).rgba;
	bump.xyz = normalize(bump.xyz * 2.0 - 1.0);
	return bump;
}

float find_intersection(vec2 dp, vec2 ds)
{
	float depth = 1.0;
	float best_depth = 0.0;
	float size = 0.0625;
	for (int i = 0; i < 15; i++) {
		depth -= size;
		float h = texture2D(normalTexture, dp + ds * depth).a;
		if (depth <= h) {
			best_depth = depth;
			break;
		}
	}
	depth = best_depth;
	for (int i = 0; i < 4; i++) {
		size *= 0.5;
		float h = texture2D(normalTexture,dp + ds * depth).a;
		if (depth <= h) {
			best_depth = depth;
			depth += size;
		} else {
			depth -= size;
		}
	}
	return best_depth;
}

float find_intersectionRGB(vec2 dp, vec2 ds)
{
	const float depth_step = 1.0 / 24.0;
	float depth = 1.0;
	for (int i = 0 ; i < 24 ; i++) {
		float h = get_rgb_height(dp + ds * depth);
		if (h >= depth)
			break;
		depth -= depth_step;
	}
	return depth;
}

void main(void)
{
	vec3 color;
	vec4 bump;
	vec2 uv = gl_TexCoord[0].st;
	bool use_normalmap = false;
	get_texture_flags();

#ifdef ENABLE_PARALLAX_OCCLUSION
	vec2 eyeRay = vec2 (tsEyeVec.x, -tsEyeVec.y);
	const float scale = PARALLAX_OCCLUSION_SCALE / PARALLAX_OCCLUSION_ITERATIONS;
	const float bias = PARALLAX_OCCLUSION_BIAS / PARALLAX_OCCLUSION_ITERATIONS;

#if PARALLAX_OCCLUSION_MODE == 0
	// Parallax occlusion with slope information
	if (normalTexturePresent && area_enable_parallax > 0.0) {
		for (int i = 0; i < PARALLAX_OCCLUSION_ITERATIONS; i++) {
			vec4 normal = texture2D(normalTexture, uv.xy);
			float h = normal.a * scale - bias;
			uv += h * normal.z * eyeRay;
		}
#endif

#if PARALLAX_OCCLUSION_MODE == 1
	// Relief mapping
	if (normalTexturePresent && area_enable_parallax > 0.0) {
		vec2 ds = eyeRay * PARALLAX_OCCLUSION_SCALE;
		float dist = find_intersection(uv, ds);
		uv += dist * ds;
#endif
	} else if (GENERATE_NORMALMAPS == 1 && area_enable_parallax > 0.0) {
		vec2 ds = eyeRay * PARALLAX_OCCLUSION_SCALE;
		float dist = find_intersectionRGB(uv, ds);
		uv += dist * ds;
	}
#endif

#if USE_NORMALMAPS == 1
	if (normalTexturePresent) {
		bump = get_normal_map(uv);
		use_normalmap = true;
	}
#endif

	if (GENERATE_NORMALMAPS == 1 && normalTexturePresent == false) {
		float tl = get_rgb_height(vec2(uv.x - SAMPLE_STEP, uv.y + SAMPLE_STEP));
		float t  = get_rgb_height(vec2(uv.x - SAMPLE_STEP, uv.y - SAMPLE_STEP));
		float tr = get_rgb_height(vec2(uv.x + SAMPLE_STEP, uv.y + SAMPLE_STEP));
		float r  = get_rgb_height(vec2(uv.x + SAMPLE_STEP, uv.y));
		float br = get_rgb_height(vec2(uv.x + SAMPLE_STEP, uv.y - SAMPLE_STEP));
		float b  = get_rgb_height(vec2(uv.x, uv.y - SAMPLE_STEP));
		float bl = get_rgb_height(vec2(uv.x -SAMPLE_STEP, uv.y - SAMPLE_STEP));
		float l  = get_rgb_height(vec2(uv.x - SAMPLE_STEP, uv.y));
		float dX = (tr + 2.0 * r + br) - (tl + 2.0 * l + bl);
		float dY = (bl + 2.0 * b + br) - (tl + 2.0 * t + tr);
		bump = vec4(normalize(vec3 (dX, dY, NORMALMAPS_STRENGTH)), 1.0);
		use_normalmap = true;
	}

	vec4 base = texture2D(baseTexture, uv).rgba;

#ifdef ENABLE_BUMPMAPPING
	if (use_normalmap) {
		vec3 L = normalize(lightVec);
		vec3 E = normalize(eyeVec);
		float specular = pow(clamp(dot(reflect(L, bump.xyz), E), 0.0, 1.0), 1.0);
		float diffuse = dot(-E,bump.xyz);
		color = (diffuse + 0.1 * specular) * base.rgb;
	} else {
		color = base.rgb;
	}
#else
	color = base.rgb;
#endif

	float light = max((wieldLight/2.0)/vPosition.z, 0.0);
	vec4 col = vec4(color.rgb * min(gl_Color.rgb + light, 1.0), 1.0);
	
#if MATERIAL_TYPE == TILE_MATERIAL_LIQUID_TRANSPARENT
	float alpha = gl_Color.a;
	if (fogDistance != 0.0) {
		float d = max(0.0, min(vPosition.z / fogDistance * 1.5 - 0.6, 1.0));
		alpha = mix(alpha, 0.0, d);
	}
	col = vec4(col.rgb, alpha);
#else
	if (fogDistance != 0.0) {
		float d = max(0.0, min(vPosition.z / fogDistance * 1.5 - 0.6, 1.0));
		col = mix(col, skyBgColor, d);
	}
	col = vec4(col.rgb, base.a);
#endif

#ifdef ENABLE_TONE_MAPPING
	gl_FragColor = applyToneMapping(col);
#else
	gl_FragColor = col;
#endif
}
