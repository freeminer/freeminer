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

float fogHash(vec2 p)
{
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float fogNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);

	float a = fogHash(i);
	float b = fogHash(i + vec2(1.0, 0.0));
	float c = fogHash(i + vec2(0.0, 1.0));
	float d = fogHash(i + vec2(1.0, 1.0));

	return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

void main(void)
{
	vec2 centered_uv = varTexCoord * 2.0 - 1.0;
	float radial_distance = length(centered_uv);
	float radial = 1.0 - smoothstep(0.62, 1.20, radial_distance);
	radial *= 1.0 - smoothstep(0.94, 1.12, max(abs(centered_uv.x), abs(centered_uv.y)));

	float fog_time = animationTimer * 100.0;
	float wind_speed = min(length(fogWind.xz), 80.0);
	vec2 wind_dir = wind_speed > 0.001 ? normalize(fogWind.xz) : vec2(1.0, 0.0);
	vec2 side_dir = vec2(-wind_dir.y, wind_dir.x);
	vec2 wind_drift = wind_dir *
		((fog_time * 0.026 + sin(fog_time * 0.17 + fogPhase) * 0.20) *
			(0.35 + wind_speed * 0.025));
	vec2 curl_drift = side_dir *
		(sin(fog_time * 0.11 + fogPhase * 1.73) *
			(0.16 + wind_speed * 0.006));
	vec2 drift = vec2(fog_time * 0.013, fog_time * -0.008) + curl_drift;
	float n1 = fogNoise(fogWorldPos.xz * 0.00085 - wind_drift + drift);
	float n2 = fogNoise(fogWorldPos.xy * 0.00125 - vec2(wind_drift.x, curl_drift.y) - drift * 1.7);
	float n3 = fogNoise(fogWorldPos.zy * 0.00170 - vec2(wind_drift.y, curl_drift.x) + drift.yx * 1.3);
	float cloud = smoothstep(0.18, 0.92, n1 * 0.56 + n2 * 0.30 + n3 * 0.14);
	float soft_grain = fogNoise(fogWorldPos.xz * 0.0032 - wind_drift * 1.7 - drift * 2.2);

	float alpha = varColor.a * radial * mix(0.34, 1.12, cloud) *
		mix(0.82, 1.12, soft_grain);
	if (alpha < 0.003)
		discard;

	float distance_mix = clamp(length(eyeVec) / max(fogDistance, 1.0), 0.0, 1.0);
	float fog_mix = clamp(0.16 + distance_mix * 0.22
		+ (1.0 - fogShadingParameter) * 0.08, 0.12, 0.48);
	vec3 color = mix(varColor.rgb, fogColor.rgb, fog_mix);

	gl_FragColor = vec4(color, alpha);
}
