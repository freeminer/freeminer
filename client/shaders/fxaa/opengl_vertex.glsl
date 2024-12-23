uniform vec2 texelSize0;

#ifdef GL_ES
varying mediump vec2 varTexCoord;
#else
centroid varying vec2 varTexCoord;
#endif

varying vec2 sampleNW;
varying vec2 sampleNE;
varying vec2 sampleSW;
varying vec2 sampleSE;

/*
Based on
https://github.com/mattdesl/glsl-fxaa/
Portions Copyright (c) 2011 by Armin Ronacher.
*/
void main(void)
{
	varTexCoord.st = inTexCoord0.st;
	sampleNW = varTexCoord.st + vec2(-1.0, -1.0) * texelSize0;
	sampleNE = varTexCoord.st + vec2(1.0, -1.0) * texelSize0;
	sampleSW = varTexCoord.st + vec2(-1.0, 1.0) * texelSize0;
	sampleSE = varTexCoord.st + vec2(1.0, 1.0) * texelSize0;
	gl_Position = inVertexPosition;
}
