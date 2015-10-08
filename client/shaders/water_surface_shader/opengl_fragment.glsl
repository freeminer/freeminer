uniform sampler2D baseTexture;
uniform sampler2D normalTexture;
uniform sampler2D textureFlags;

uniform vec4 skyBgColor;
uniform float fogDistance;
uniform vec3 eyePosition;
uniform vec3 sunPosition;

varying vec3 vPosition;
varying vec3 worldPosition;

varying vec3 eyeVec;
varying vec3 tsEyeVec;
varying vec3 lightVec;
varying vec3 tsLightVec;

uniform float wieldLight;

bool normalTexturePresent = false;
bool texTileableHorizontal = false;
bool texTileableVertical = false;
bool texSeamless = false;

const float e = 2.718281828459;
const float BS = 1.0;

void get_texture_flags()
{
	vec4 flags = texture2D(textureFlags, vec2(0.0, 0.0));
	if (flags.r > 0.5) {
		normalTexturePresent = true;
	}
	if (flags.g > 0.5) {
		texTileableHorizontal = true;
	}
	if (flags.b > 0.5) {
		texTileableVertical = true;
	}
	if (texTileableHorizontal && texTileableVertical) {
		texSeamless = true;
	}
}

float intensity(vec3 color)
{
	return (color.r + color.g + color.b) / 3.0;
}

float get_rgb_height(vec2 uv)
{
	return intensity(texture2D(baseTexture,uv).rgb);
}

vec4 get_normal_map(vec2 uv)
{
	vec4 bump = texture2D(normalTexture, uv).rgba;
	bump.xyz = normalize(bump.xyz * 2.0 -1.0);
	bump.y = -bump.y;
	return bump;
}

void main(void)
{
	vec3 color;
	vec4 bump;
	vec2 uv = gl_TexCoord[0].st;
	bool use_normalmap = false;
	get_texture_flags();

#ifdef ENABLE_PARALLAX_OCCLUSION
	if (normalTexturePresent) {
		vec3 tsEye = normalize(tsEyeVec);
		float height = PARALLAX_OCCLUSION_SCALE * texture2D(normalTexture, uv).a - PARALLAX_OCCLUSION_BIAS;
		uv = uv + texture2D(normalTexture, uv).z * height * vec2(tsEye.x,-tsEye.y);
	}
#endif

#ifdef USE_NORMALMAPS
	if (normalTexturePresent) {
		bump = get_normal_map(uv);
		use_normalmap = true;
	} 
#endif

	if (GENERATE_NORMALMAPS == 1 && use_normalmap == false) {
		float tl = get_rgb_height (vec2(uv.x-SAMPLE_STEP,uv.y+SAMPLE_STEP));
		float t  = get_rgb_height (vec2(uv.x-SAMPLE_STEP,uv.y-SAMPLE_STEP));
		float tr = get_rgb_height (vec2(uv.x+SAMPLE_STEP,uv.y+SAMPLE_STEP));
		float r  = get_rgb_height (vec2(uv.x+SAMPLE_STEP,uv.y));
		float br = get_rgb_height (vec2(uv.x+SAMPLE_STEP,uv.y-SAMPLE_STEP));
		float b  = get_rgb_height (vec2(uv.x,uv.y-SAMPLE_STEP));
		float bl = get_rgb_height (vec2(uv.x-SAMPLE_STEP,uv.y-SAMPLE_STEP));
		float l  = get_rgb_height (vec2(uv.x-SAMPLE_STEP,uv.y));
		float dX = (tr + 2.0 * r + br) - (tl + 2.0 * l + bl);
		float dY = (bl + 2.0 * b + br) - (tl + 2.0 * t + tr);
		bump = vec4 (normalize(vec3 (dX, -dY, NORMALMAPS_STRENGTH)),1.0);
		use_normalmap = true;
	}

vec4 base = texture2D(baseTexture, uv).rgba;

#ifdef ENABLE_BUMPMAPPING
	if (use_normalmap) {
		vec3 L = normalize(lightVec);
		vec3 E = normalize(eyeVec);
		float specular = pow(clamp(dot(reflect(L, bump.xyz), E), 0.0, 1.0),0.5);
		float diffuse = dot(E,bump.xyz);
		/* Mathematic optimization
		* Original: color = 0.05*base.rgb + diffuse*base.rgb + 0.2*specular*base.rgb;
		* This optimization save 2 multiplications (orig: 4 multiplications + 3 additions
		* end: 2 multiplications + 3 additions)
		*/
		color = (0.05 + diffuse + 0.2 * specular) * base.rgb;
	} else {
		color = base.rgb;
	}
#else
	color = base.rgb;
#endif

	float light = max((wieldLight/2.0)/vPosition.z, 0.0);
#if MATERIAL_TYPE == TILE_MATERIAL_LIQUID_TRANSPARENT || MATERIAL_TYPE == TILE_MATERIAL_LIQUID_OPAQUE
	float alpha = gl_Color.a;
	vec4 col = vec4(color.rgb, alpha);
	col *= min(gl_Color+vec4(light), 1.0);
	if(fogDistance != 0.0){
		float d = max(0.0, min(vPosition.z / fogDistance * 1.5 - 0.6, 1.0));
		alpha = mix(alpha, 0.0, d);
	}
	gl_FragColor = vec4(col.rgb, alpha);
#else
	vec4 col = vec4(color.rgb, base.a);
	col *= min(gl_Color+vec4(light), 1.0);
	if(fogDistance != 0.0){
		float d = max(0.0, min(vPosition.z / fogDistance * 1.5 - 0.6, 1.0));
		col = mix(col, skyBgColor, d);
	}
	gl_FragColor = vec4(col.rgb, base.a);
#endif
}
