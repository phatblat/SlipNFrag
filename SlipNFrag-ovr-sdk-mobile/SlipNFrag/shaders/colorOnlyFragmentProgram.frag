#version 440 core

#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable

layout(set = 0, binding = 1) uniform sampler2D fragmentTexture;
layout(location = 0) in mediump vec2 fragmentTexCoords;
layout(location = 0) out lowp vec4 outColor;

void main()
{
	outColor = texture(fragmentTexture, fragmentTexCoords);
}
