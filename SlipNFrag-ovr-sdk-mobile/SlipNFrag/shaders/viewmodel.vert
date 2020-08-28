#version 440 core

#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable

layout(binding = 0) uniform SceneMatrices
{
	layout(offset = 0) mat4 ViewMatrix;
	layout(offset = 64) mat4 ProjectionMatrix;
};

layout(push_constant) uniform Forward
{
	layout(offset = 0) float forwardX;
	layout(offset = 4) float forwardY;
	layout(offset = 8) float forwardZ;
	layout(offset = 12) float offset;
};

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoords;
layout(location = 2) in float vertexLight;
layout(location = 3) in mat4 vertexTransform;
layout(location = 0) out mediump vec2 fragmentTexCoords;
layout(location = 1) out mediump float fragmentLight;
layout(location = 2) out mediump float fragmentAlpha;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main(void)
{
	gl_Position = ProjectionMatrix * (ViewMatrix * (vertexTransform * vec4(vertexPosition, 1)));
	fragmentTexCoords = vertexTexCoords;
	fragmentLight = vertexLight;
	float projection = offset + vertexPosition.x * forwardX + vertexPosition.y * forwardY + vertexPosition.z * forwardZ;
	fragmentAlpha = min(max(projection / 8, 0), 1);
}
