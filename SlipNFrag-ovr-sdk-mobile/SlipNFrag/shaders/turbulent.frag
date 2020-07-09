#version 440 core

#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable

layout(binding = 1) uniform sampler2D fragmentTexture;
layout(binding = 2) uniform Time
{
	layout(offset = 0) mediump float time;
};

layout(location = 0) in mediump vec2 fragmentTexCoords;
layout(location = 0) out lowp vec4 outColor;

void main()
{
	mediump float t = mod(time, 3.14159 * 2);
	mediump float x = fragmentTexCoords.x + sin(t + fragmentTexCoords.y * 5) / 10;
	mediump float y = fragmentTexCoords.y + sin(t + fragmentTexCoords.x * 5) / 10;
	outColor = texture(fragmentTexture, vec2(x, y));
}
