#version 440 core

#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable
#extension GL_OVR_multiview2 : enable

layout(set = 1, binding = 0) uniform SceneMatrices
{
	layout(offset = 0) mat4 ViewMatrix[2];
	layout(offset = 128) mat4 ProjectionMatrix[2];
};

layout(push_constant) uniform Transforms
{
	layout(offset = 0) mat4 aliasTransform;
	layout(offset = 64) float forwardX;
	layout(offset = 68) float forwardY;
	layout(offset = 72) float forwardZ;
	layout(offset = 76) float offset;
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
	vec4 position = vertexTransform * (aliasTransform * vec4(vertexPosition, 1));
	gl_Position = ProjectionMatrix[gl_ViewID_OVR] * (ViewMatrix[gl_ViewID_OVR] * position);
	fragmentTexCoords = vertexTexCoords;
	fragmentLight = vertexLight;
	float projection = offset + position.x * forwardX + position.y * forwardY + position.z * forwardZ;
	fragmentAlpha = min(max(projection / 8, 0), 1);
}
