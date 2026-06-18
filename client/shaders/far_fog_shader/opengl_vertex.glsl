uniform lowp vec4 materialColor;
uniform highp mat4 mWorld;
uniform highp vec3 cameraOffset;

VARYING_ lowp vec4 varColor;
VARYING_ mediump vec2 varTexCoord;
VARYING_ highp vec3 eyeVec;
VARYING_ highp vec3 fogWorldPos;

void main(void)
{
	gl_Position = mWorldViewProj * inVertexPosition;

	varColor = inVertexColor * materialColor;
	varTexCoord = inTexCoord0;
	eyeVec = -(mWorldView * inVertexPosition).xyz;
	fogWorldPos = (mWorld * inVertexPosition).xyz + cameraOffset;
}
