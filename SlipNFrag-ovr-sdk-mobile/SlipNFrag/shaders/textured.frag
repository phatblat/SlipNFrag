#version 440 core

#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable

layout(binding = 1) uniform sampler2D fragmentTexture;
layout(binding = 2) uniform sampler2D palette;
layout(location = 0) in mediump vec2 fragmentTexCoords;
layout(location = 0) out lowp vec4 outColor;

void main()
{
	mediump vec4 entry = texture(fragmentTexture, fragmentTexCoords);
	outColor = texelFetch(palette, ivec2(entry.x * 255, 0), 0);
}
