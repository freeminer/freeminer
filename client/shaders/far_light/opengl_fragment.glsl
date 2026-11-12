uniform float fogDistance;
uniform float fogShadingParameter;
uniform vec3 dayLight;

VARYING_ mediump vec4 varColor;
VARYING_ highp vec3 eyeVec;

void main(void)
{
	vec4 color = varColor;
	// get_sunlight_color maps the night/day ratios 0.175/1.0 to red 0.135/0.96.
	// Preserve the vertex twinkle while fading out in daylight.
	color.a *= 1.0 - smoothstep(0.135, 0.96, dayLight.r);

	if (fogDistance > 0.0) {
		float clarity = clamp(fogShadingParameter -
				fogShadingParameter * length(eyeVec) / fogDistance, 0.0, 1.0);
		// A light disappears into fog instead of leaving a fog-colored dot.
		color.a *= clarity;
	}
	if (color.a <= 0.0)
		discard;

	gl_FragColor = color;
}
