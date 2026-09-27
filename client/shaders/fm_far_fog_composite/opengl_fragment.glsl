// Fog RGB is premultiplied; alpha contains accumulated coverage.
uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform vec2 texelSize0;
VARYING_ highp vec2 varTexCoord;

void main(void)
{
	float depth = texture2D(texture2, varTexCoord).r;
	vec2 pixel = varTexCoord / texelSize0 - 0.5;
	vec2 base = (floor(pixel) + 0.5) * texelSize0;
	vec2 fraction = fract(pixel);
	vec4 fog = vec4(0.0);
	float total = 0.0;
	vec4 closest = vec4(0.0);
	float closest_error = 1e20;
	for (int y = 0; y < 2; ++y) {
		for (int x = 0; x < 2; ++x) {
			vec2 offset = vec2(float(x), float(y));
			vec2 uv = clamp(base + offset * texelSize0,
				texelSize0 * 0.5, vec2(1.0) - texelSize0 * 0.5);
			float sample_depth = texture2D(texture2, uv).r;
			// Relative perspective-depth difference; reject across silhouettes.
			float error = abs(sample_depth - depth) / max(1.0 - depth, 1e-7);
			vec4 value = texture2D(texture0, uv);
			if (error < closest_error) {
				closest_error = error;
				closest = value;
			}
			vec2 weights = mix(vec2(1.0) - fraction, fraction, offset);
			float weight = weights.x * weights.y * (1.0 - smoothstep(0.01, 0.05, error));
			fog += value * weight;
			total += weight;
		}
	}
	// If no low-res sample represents this surface, suppress fog rather than
	// bleeding a background cloud over a thin foreground object.
	fog = total > 1e-5 ? fog / total : (closest_error < 0.05 ? closest : vec4(0.0));
	vec4 scene = texture2D(texture1, varTexCoord);
	gl_FragColor = vec4(fog.rgb + scene.rgb * (1.0 - fog.a), scene.a);
	gl_FragDepth = depth;
}
