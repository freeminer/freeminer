uniform mat4 mWorld;
uniform highp vec3 cameraOffset;
uniform float animationTimer;

VARYING_ mediump vec4 varColor;
VARYING_ highp vec3 eyeVec;

float hash13(vec3 p)
{
	p = fract(p * 0.1031);
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

void main(void)
{
	vec4 viewPosition = mWorldView * inVertexPosition;
	// Hash node coordinates, wrapping the offset separately to retain precision
	// far from the origin. Camera rebasing must not change a light's phase.
	vec3 worldPosition = mod(floor((mWorld * inVertexPosition).xyz / 10.0 + 0.5) +
			mod(cameraOffset / 10.0, 4096.0), 4096.0);
	float distance = length(viewPosition.xyz);
	float seed = hash13(worldPosition);
	float distanceFactor = clamp((distance - 200.0) / 1800.0, 0.0, 1.0);
	float flickerAmount = distanceFactor * mix(1.0 / 3.0, 1.0, distanceFactor);
	float phase = seed * 6.2831853;
	float wave = 0.5 + 0.5 * (
			0.62 * sin(animationTimer * (124.0 + seed * 46.0) + phase) +
			0.38 * sin(animationTimer * (314.0 + seed * 82.0) + phase * 2.17));
	// Modulate opacity so bright channels cannot clip away the twinkle or
	// change the hue. Nearby lights stay steady; distant lights scintillate.
	float flicker = mix(1.0, 0.35 + 0.65 * wave, flickerAmount);

	gl_Position = mWorldViewProj * inVertexPosition;
	gl_PointSize = 1.0;

	eyeVec = -viewPosition.xyz;
	// Vertex RGB already contains the source intensity (including its alpha).
	varColor = vec4(inVertexColor.rgb, flicker);
}
