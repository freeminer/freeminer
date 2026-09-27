VARYING_ highp vec2 varTexCoord;
void main(void)
{
	varTexCoord = inTexCoord0.st;
	gl_Position = inVertexPosition;
}
