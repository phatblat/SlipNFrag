#version 440 core

#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable

layout(set = 1, binding = 0) uniform SceneMatrices
{
	layout(offset = 0) mat4 ViewMatrix;
	layout(offset = 64) mat4 ProjectionMatrix;
};

layout(push_constant) uniform Transforms
{
	layout(offset = 0) mat4 aliasTransform;
};

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexTexCoords;
layout(location = 2) in float vertexLight;
layout(location = 3) in mat4 vertexTransform;
layout(location = 0) out mediump vec2 fragmentTexCoords;
layout(location = 1) out mediump float fragmentLight;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main(void)
{
	gl_Position = ProjectionMatrix * (ViewMatrix * (vertexTransform * (aliasTransform * vec4(vertexPosition, 1))));
	fragmentTexCoords = vertexTexCoords;
	fragmentLight = vertexLight;
}
