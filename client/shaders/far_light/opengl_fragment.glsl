uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;
uniform vec3 dayLight;

VARYING_ mediump vec4 varColor;
VARYING_ highp vec3 eyeVec;

void main(void)
{
	vec4 color = varColor;
	// get_sunlight_color maps the night/day ratios 0.175/1.0 to red 0.135/0.96.
	// RGB already includes the light's brightness; alpha only controls the day fade.
	color.a = 1.0 - smoothstep(0.135, 0.96, dayLight.r);
	if (color.a <= 0.0)
		discard;

	if (fogDistance > 0.0) {
		float clarity = clamp(fogShadingParameter -
				fogShadingParameter * length(eyeVec) / fogDistance, 0.0, 1.0);
		color.rgb = mix(fogColor.rgb, color.rgb, clarity);
	}

	gl_FragColor = color;
}
