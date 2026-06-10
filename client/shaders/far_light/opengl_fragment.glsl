uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;

VARYING_ mediump vec4 varColor;
VARYING_ highp vec3 eyeVec;

void main(void)
{
	vec4 color = varColor;

	if (fogDistance > 0.0) {
		float clarity = clamp(fogShadingParameter -
				fogShadingParameter * length(eyeVec) / fogDistance, 0.0, 1.0);
		color.rgb = mix(fogColor.rgb, color.rgb, clarity);
	}

	gl_FragColor = color;
}
