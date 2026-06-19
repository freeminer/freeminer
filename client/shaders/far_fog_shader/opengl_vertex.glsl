uniform lowp vec4 materialColor;
uniform highp mat4 mWorld;
uniform highp vec3 cameraOffset;
uniform float animationTimer;

VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;
VARYING_ highp vec3 eyeVec;
VARYING_ highp vec3 fogWorldPos;
VARYING_ highp vec3 fogWind;
VARYING_ highp float fogPhase;

void main(void)
{
	vec3 wind = vec3(inVertexNormal.x, 0.0, inVertexNormal.z);
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
	fogWorldPos = (mWorld * fogVertex).xyz + cameraOffset;
	fogWind = wind;
	fogPhase = phase;
}
