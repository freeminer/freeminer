uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;
uniform float animationTimer;

VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;
VARYING_ highp vec3 eyeVec;
VARYING_ highp vec3 fogWorldPos;
VARYING_ highp vec3 fogWind;
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
	float radial_distance = length(centered_uv);

	float fog_time = animationTimer * 100.0;
	float wind_speed = min(length(fogWind.xz), 80.0);
	vec2 wind_dir = wind_speed > 0.001 ? normalize(fogWind.xz) : vec2(1.0, 0.0);
	vec2 side_dir = vec2(-wind_dir.y, wind_dir.x);
	vec3 wind_vec = vec3(wind_dir.x, 0.0, wind_dir.y);
	vec3 side_vec = vec3(side_dir.x, 0.0, side_dir.y);
	float wind_drift =
		(fog_time * 0.026 + sin(fog_time * 0.17 + fogPhase) * 0.20) *
		(0.35 + wind_speed * 0.025);
	float curl_drift =
		sin(fog_time * 0.11 + fogPhase * 1.73) *
		(0.16 + wind_speed * 0.006);
	vec3 drift = wind_vec * wind_drift + side_vec * curl_drift +
		vec3(fog_time * 0.010, fog_time * 0.003, fog_time * -0.007);
	vec3 cloud_pos = fogWorldPos * 0.00082 - drift + vec3(fogPhase * 0.37);
	float local_falloff = radial_distance * 0.34 + abs(centered_uv.y) * 0.12;
	float cloud = fogMap5(cloud_pos, local_falloff);
	float detail = fogMap3(cloud_pos * 3.10 + vec3(7.1, fogPhase, 13.7),
		local_falloff * 0.45);
	float edge_breakup = fogMap3(vec3(centered_uv * 1.45, fogPhase), 0.08);
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
	vec3 sun_dir = normalize(vec3(-0.7071, 0.12, -0.7071));
	float cloud_light_sample = fogMap3(cloud_pos + sun_dir * 0.42, local_falloff);
	float direct_light = clamp((cloud - cloud_light_sample) / 0.55, 0.0, 1.0);
	vec3 cloud_light = vec3(0.91, 0.98, 1.05) + vec3(1.0, 0.60, 0.30) * direct_light * 0.28;
	float density_shadow = smoothstep(0.16, 0.72, density);
	float self_shadow = mix(1.00, 0.66,
		max(cloud * clamp(varColor.a * 1.70, 0.0, 1.0), density_shadow * 0.72));
	vec3 color = mix(varColor.rgb * cloud_light * self_shadow, fogColor.rgb, fog_mix);

	gl_FragColor = vec4(color, alpha);
}
