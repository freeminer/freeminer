// Fog RGB is premultiplied; alpha contains accumulated coverage.
uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;
uniform vec2 texelSize0;
uniform lowp vec4 fogColor;
uniform float fogDistance;
uniform float fogShadingParameter;
uniform lowp vec4 fmFogSun;
uniform lowp vec4 fmFogLight;
uniform vec2 fmFogDepthRange;
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
	float optical_depth = 0.0;
	float closest_optical_depth = 0.0;
	vec4 distant_fog = vec4(0.0);
	float distant_optical_depth = 0.0;
	float distant_total = 0.0;
	float near_plane = max(fmFogDepthRange.x, 0.001);
	float far_plane = max(fmFogDepthRange.y, near_plane + 1.0);
	float view_distance = near_plane * far_plane /
		max(far_plane - depth * (far_plane - near_plane), 0.001);
	// Distant terrain and sky can alternate within a half-resolution pixel.
	// Reconstruct their fog together, retaining strict rejection near the camera.
	float distant_surface =
			smoothstep(near_plane * 5000.0, near_plane * 20000.0, view_distance);
	for (int y = 0; y < 2; ++y) {
		for (int x = 0; x < 2; ++x) {
			vec2 offset = vec2(float(x), float(y));
			vec2 uv = clamp(base + offset * texelSize0,
				texelSize0 * 0.5, vec2(1.0) - texelSize0 * 0.5);
			float sample_depth = texture2D(texture2, uv).r;
			// Relative perspective-depth difference; reject across silhouettes.
			float error = abs(sample_depth - depth) / max(1.0 - depth, 1e-7);
			vec4 value = texture2D(texture0, uv);
			float sample_optical_depth = texture2D(texture3, uv).r;
			if (error < closest_error) {
				closest_error = error;
				closest = value;
				closest_optical_depth = sample_optical_depth;
			}
			vec2 weights = mix(vec2(1.0) - fraction, fraction, offset);
			float depth_weight = 1.0 - smoothstep(0.01, 0.05, error);
			float weight = weights.x * weights.y * depth_weight;
			fog += value * weight;
			optical_depth += sample_optical_depth * weight;
			total += weight;
			// Accept distant samples on either side of the surface depth, so
			// closer terrain cannot leave holes on sky or farther terrain pixels.
			// Keep nearby occluders out of this reconstruction.
			float sample_distance =
					near_plane * far_plane /
					max(far_plane - sample_depth * (far_plane - near_plane), 0.001);
			float distant_sample = smoothstep(
					near_plane * 5000.0, near_plane * 20000.0, sample_distance);
			float distant_weight =
					weights.x * weights.y * mix(depth_weight, 1.0, distant_sample);
			distant_fog += value * distant_weight;
			distant_optical_depth += sample_optical_depth * distant_weight;
			distant_total += distant_weight;
		}
	}
	// If no low-res sample represents this surface, suppress fog rather than
	// bleeding a background cloud over a thin foreground object.
	fog = total > 1e-5 ? fog / total : (closest_error < 0.05 ? closest : vec4(0.0));
	optical_depth = total > 1e-5 ? optical_depth / total :
		(closest_error < 0.05 ? closest_optical_depth : 0.0);
	if (distant_total > 1e-5) {
		fog = mix(fog, distant_fog / distant_total, distant_surface);
		optical_depth = mix(optical_depth, distant_optical_depth / distant_total,
			distant_surface);
	}
	vec4 scene = texture2D(texture1, varTexCoord);
	// Apply one attenuation to the combined underside contributions instead
	// of darkening every thin puff independently.
	// Preserve ambient illumination in the densest clouds instead of allowing
	// accumulated extinction to make a nearly black, solid silhouette.
	float light_transmittance =
		min(1.0, 1.40 * mix(0.70, 1.0, exp(-min(optical_depth, 16.0) * 0.25)));
	vec3 color = fog.rgb * light_transmittance + scene.rgb * (1.0 - fog.a);

	// Aerial perspective complements the local puffs without hiding nearby
	// geometry. Leave the sky untouched because it already contains its tint.
	if (depth < 0.999999 && fogDistance > 0.0) {
		float fog_start = fogShadingParameter > 1.0 ?
			1.0 - 1.0 / fogShadingParameter : 0.0;
		float relative_distance = clamp(view_distance / fogDistance, 0.0, 1.0);
		float fog_range = clamp((relative_distance - fog_start) /
			max(1.0 - fog_start, 0.001), 0.0, 1.0);
		float aerial = (1.0 - exp(-1.8 * fog_range * fog_range)) * 0.55;

		float sun_glow = 0.0;
		if (fmFogSun.z > 0.0 && fmFogLight.a > 0.0) {
			vec2 sun_uv = fmFogSun.xy / fmFogSun.z * 0.5 + 0.5;
			float aspect = texelSize0.y / max(texelSize0.x, 1e-7);
			vec2 sun_delta = (varTexCoord - sun_uv) * vec2(aspect, 1.0);
			sun_glow = exp(-dot(sun_delta, sun_delta) * 3.0) * fmFogLight.a;
		}
		vec3 aerial_color = mix(fogColor.rgb,
			fogColor.rgb * fmFogLight.rgb * 1.20, sun_glow * 0.75);
		color = mix(color, aerial_color, aerial);
	}

	gl_FragColor = vec4(color, scene.a);
	gl_FragDepth = depth;
}
