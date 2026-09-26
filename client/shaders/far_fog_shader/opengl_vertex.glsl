// fm: evaluate per-puff drift at vertices
uniform lowp vec4 materialColor;
uniform highp mat4 mWorld;
uniform highp vec3 cameraOffset;
uniform vec3 windDirection;
uniform float animationTimer;

VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;
VARYING_ highp vec3 eyeVec;
VARYING_ highp vec3 cloud_pos;
#if FM_FOG_DEPTH_TEST
VARYING_ highp vec4 fogClipPos;
#endif
VARYING_ highp float fogPhase;

void main(void)
{
	vec3 wind = windDirection;
	float phase = inVertexNormal.y;
	float speed = min(length(wind.xz), 80.0);
	vec3 wind_dir = speed > 0.001 ? normalize(wind) : vec3(1.0, 0.0, 0.0);
	vec3 side_dir = vec3(-wind_dir.z, 0.0, wind_dir.x);
	float fog_time = animationTimer * 100.0;
	float along =
		sin(fog_time * 0.19 + phase) * 0.65 +
		sin(fog_time * 0.37 + phase * 1.618) * 0.35;
	float sideways = sin(fog_time * 0.23 + phase * 2.31);
	vec3 offset = wind_dir * along * speed * 1.80 +
		side_dir * sideways * speed * 0.45;
	vec4 fogVertex = vec4(inVertexPosition.xyz + offset, 1.0);

	gl_Position = mWorldViewProj * fogVertex;

	varColor = inVertexColor * materialColor;
	varTexCoord = inTexCoord0;
	eyeVec = -(mWorldView * fogVertex).xyz;
	vec3 fogWorldPos = (mWorld * fogVertex).xyz + cameraOffset;
#if FM_FOG_DEPTH_TEST
	fogClipPos = gl_Position;
#endif
	fogPhase = phase;
	float wind_speed = min(length(windDirection.xz), 80.0);
	vec2 fog_wind_dir = wind_speed > 0.001 ? normalize(windDirection.xz) : vec2(1.0, 0.0);
	vec2 fog_side_dir = vec2(-fog_wind_dir.y, fog_wind_dir.x);
	vec3 fog_wind_vec = vec3(fog_wind_dir.x, 0.0, fog_wind_dir.y);
	vec3 fog_side_vec = vec3(fog_side_dir.x, 0.0, fog_side_dir.y);
	float wind_drift =
		(fog_time * 0.026 + sin(fog_time * 0.17 + phase) * 0.20) *
		(0.35 + wind_speed * 0.025);
	float curl_drift =
		sin(fog_time * 0.11 + phase * 1.73) *
		(0.16 + wind_speed * 0.006);
	vec3 drift = fog_wind_vec * wind_drift + fog_side_vec * curl_drift +
		vec3(fog_time * 0.010, fog_time * 0.003, fog_time * -0.007);
	cloud_pos = fogWorldPos * 0.00082 - drift + vec3(phase * 0.37);

}
// ===
