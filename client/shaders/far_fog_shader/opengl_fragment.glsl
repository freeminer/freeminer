// fm: procedural fog quality and early rejection
uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;
uniform highp vec3 fmFogSunDirection;
uniform lowp vec4 fmFogLight;
uniform highp vec2 fmFogCloudLayer;
#ifndef FM_FOG_QUALITY
#define FM_FOG_QUALITY 2
#endif
#if FM_FOG_DEPTH_TEST
uniform sampler2D texture0;
VARYING_ highp vec4 fogClipPos;
#endif
#ifndef FM_FOG_DENSITY_TARGET
#define FM_FOG_DENSITY_TARGET 0
#endif

VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;
VARYING_ highp vec3 eyeVec;
VARYING_ highp vec3 eyeSunDirection;
VARYING_ highp vec3 cloud_pos;
VARYING_ highp float fogAltitude;
VARYING_ highp float fogPhase;

float fogHash(vec3 p)
{
	p = fract(p * 0.3183099 + vec3(0.11, 0.17, 0.23));
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float fogNoise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	vec3 u = f * f * (3.0 - 2.0 * f);

	float n000 = fogHash(i + vec3(0.0, 0.0, 0.0));
	float n100 = fogHash(i + vec3(1.0, 0.0, 0.0));
	float n010 = fogHash(i + vec3(0.0, 1.0, 0.0));
	float n110 = fogHash(i + vec3(1.0, 1.0, 0.0));
	float n001 = fogHash(i + vec3(0.0, 0.0, 1.0));
	float n101 = fogHash(i + vec3(1.0, 0.0, 1.0));
	float n011 = fogHash(i + vec3(0.0, 1.0, 1.0));
	float n111 = fogHash(i + vec3(1.0, 1.0, 1.0));

	float nx00 = mix(n000, n100, u.x);
	float nx10 = mix(n010, n110, u.x);
	float nx01 = mix(n001, n101, u.x);
	float nx11 = mix(n011, n111, u.x);
	float nxy0 = mix(nx00, nx10, u.y);
	float nxy1 = mix(nx01, nx11, u.y);
	return mix(nxy0, nxy1, u.z);
}

float fogSignedNoise(vec3 p)
{
	return fogNoise(p) * 2.0 - 1.0;
}

float fogMap5(vec3 p, float local_falloff)
{
	float f;
	float a = 0.5;
	f  = a * fogSignedNoise(p); p = p * 2.02 + vec3(17.13, 31.71, 11.47); a *= 0.5;
	f += a * fogSignedNoise(p); p = p * 2.03 + vec3(41.37, 19.19, 23.11); a *= 0.5;
	f += a * fogSignedNoise(p); p = p * 2.01 + vec3(13.89, 47.53, 37.17); a *= 0.5;
	f += a * fogSignedNoise(p); p = p * 2.02 + vec3(29.41, 11.83, 43.79); a *= 0.5;
	f += a * fogSignedNoise(p);

	return clamp(0.42 + 1.75 * f - local_falloff, 0.0, 1.0);
}

float fogMap3(vec3 p, float local_falloff)
{
	float f;
	float a = 0.5;
	f  = a * fogSignedNoise(p); p = p * 2.02 + vec3(17.13, 31.71, 11.47); a *= 0.5;
	f += a * fogSignedNoise(p); p = p * 2.03 + vec3(41.37, 19.19, 23.11); a *= 0.5;
	f += a * fogSignedNoise(p);

	return clamp(0.42 + 1.75 * f - local_falloff, 0.0, 1.0);
}

void main(void)
{
	vec2 centered_uv = varTexCoord * 2.0 - 1.0;
	// Reject guaranteed transparent corners before any procedural noise.
	float radius_squared = dot(centered_uv, centered_uv);
	if (radius_squared >= 1.18 * 1.18)
		discard;
#if FM_FOG_DEPTH_TEST
	vec2 screen_uv = fogClipPos.xy / fogClipPos.w * 0.5 + 0.5;
	if (gl_FragCoord.z > texture2D(texture0, screen_uv).r)
		discard;
#endif
	float radial_distance = sqrt(radius_squared);

	float local_falloff = radial_distance * 0.34 + abs(centered_uv.y) * 0.12;
#if FM_FOG_QUALITY >= 2
	float cloud = fogMap5(cloud_pos, local_falloff);
	float detail = fogMap3(cloud_pos * 3.10 + vec3(7.1, fogPhase, 13.7),
		local_falloff * 0.45);
	float edge_breakup = fogMap3(vec3(centered_uv * 1.45, fogPhase), 0.08);
#else
	float cloud = fogMap3(cloud_pos, local_falloff);
	float detail = 0.5;
	float edge_breakup = fogNoise(vec3(centered_uv * 1.45, fogPhase));
#endif
	float radial = 1.0 - smoothstep(0.62 + edge_breakup * 0.16, 1.18, radial_distance);
	radial *= 1.0 - smoothstep(1.00, 1.22, max(abs(centered_uv.x), abs(centered_uv.y)));
	radial = smoothstep(0.0, 1.0, radial);

	float density = varColor.a * radial * mix(0.30, 1.75, cloud) *
		mix(0.76, 1.20, detail);
	float alpha = 1.0 - exp(-density * 1.45);
	if (alpha < 0.003)
		discard;

	float distance_mix = clamp(length(eyeVec) / max(fogDistance, 1.0), 0.0, 1.0);
	float fog_mix = clamp(0.16 + distance_mix * 0.22
		+ (1.0 - fogShadingParameter) * 0.08, 0.12, 0.48);
	// Use height within the entire cloud layer, not each billboard's UV.
	float layer_height = (fogAltitude - fmFogCloudLayer.x) / max(fmFogCloudLayer.y, 1.0);
	float cloud_layer = smoothstep(-0.1, 0.0, layer_height);
	float puff_height = clamp(layer_height, 0.0, 1.0);
#if FM_FOG_DENSITY_TARGET
	float sky_light = mix(1.0, 1.12, cloud_layer);
#else
	float sky_light = mix(1.0, mix(0.72, 1.12, smoothstep(0.05, 0.95, puff_height)),
		cloud_layer);
#endif
#if FM_FOG_QUALITY >= 2
	// Sample the density field towards the actual sun, not a fixed direction.
	float cloud_light_sample = fogMap3(cloud_pos + fmFogSunDirection * 0.42,
		local_falloff);
	float direct_light = clamp((cloud - cloud_light_sample) / 0.55, 0.0, 1.0);
	// An overhead sun crosses more dense fog on its way to the underside.
	// Preserve sunset's horizontal illumination instead of whitening all tops.
	float optical_density = (cloud + cloud_light_sample) * varColor.a * 0.5;
	float optical_depth = optical_density * (1.0 - puff_height) * 1.35 *
		max(fmFogSunDirection.y, 0.0);
	direct_light *= mix(1.0, exp(-optical_depth * 1.6), cloud_layer);
#else
	float direct_light = 0.0;
#endif
	vec3 cloud_light = mix(vec3(0.91, 0.98, 1.05), fmFogLight.rgb * 1.12,
		fmFogLight.a) * sky_light;
	cloud_light += fmFogLight.rgb * direct_light * fmFogLight.a * 0.28;
	float density_shadow = smoothstep(0.16, 0.72, density);
#if FM_FOG_DENSITY_TARGET
	// Cloud-layer darkness is accumulated across all puffs, not per billboard.
	// Ground fog retains its ordinary density shadow.
	float shadow_strength = 1.0 - cloud_layer;
#else
	float shadow_strength = mix(1.0, 1.0 - puff_height, cloud_layer);
#endif
	float self_shadow = mix(1.00, 0.66,
		max(cloud * clamp(varColor.a * 1.70, 0.0, 1.0), density_shadow * 0.72) *
		shadow_strength);
	vec3 color = mix(varColor.rgb * cloud_light * self_shadow, fogColor.rgb, fog_mix);
	// Warm forward scattering ties the puffs to the low sun and distant haze.
	// The view matrix preserves the length of the sun vector; zero is safe when
	// the sun is hidden or below the horizon.
	float forward_light = pow(max(0.0, dot(normalize(-eyeVec), eyeSunDirection)), 6.0);
	color = mix(color, fogColor.rgb * fmFogLight.rgb * 1.18,
		forward_light * fmFogLight.a * 0.22 * distance_mix);

#if FM_FOG_DENSITY_TARGET
	gl_FragData[0] = vec4(color, alpha);
	// Independently of puff order, this contributes
	// -log(1 - alpha) * underside_weight after source-alpha blending.
	float underside_weight = cloud_layer * (1.0 - puff_height);
	float output_optical_depth = -log(max(1.0 - alpha, 0.0001));
	gl_FragData[1] = vec4(output_optical_depth * underside_weight / alpha, 0.0, 0.0, 0.0);
#else
	gl_FragColor = vec4(color, alpha);
#endif
}
// ===
