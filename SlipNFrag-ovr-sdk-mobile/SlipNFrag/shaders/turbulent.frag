#version 440 core

#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable

layout(binding = 1) uniform sampler2D fragmentTexture;
layout(binding = 2) uniform sampler2D fragmentPalette;
layout(binding = 3) uniform Time
{
	layout(offset = 0) mediump float time;
};

layout(location = 0) in mediump vec2 fragmentTexCoords;
layout(location = 0) out lowp vec4 outColor;

void main()
{
	mediump float t = mod(time, 3.14159 * 2);
	mediump float tx = fragmentTexCoords.x + sin(t + fragmentTexCoords.y * 5) / 10;
	mediump float ty = fragmentTexCoords.y + sin(t + fragmentTexCoords.x * 5) / 10;
	mediump vec2 texCoords = vec2(tx, ty);
	mediump vec2 level = textureQueryLod(fragmentTexture, texCoords);
	mediump float lowMip = floor(level.y);
	mediump float highMip = ceil(level.y);
	mediump vec4 lowEntry = textureLod(fragmentTexture, texCoords, lowMip);
	mediump vec4 highEntry = textureLod(fragmentTexture, texCoords, highMip);
	mediump vec4 lowColor = texelFetch(fragmentPalette, ivec2(lowEntry.x * 255.0, 0), 0);
	mediump vec4 highColor = texelFetch(fragmentPalette, ivec2(highEntry.x * 255.0, 0), 0);
	mediump float delta = level.y - lowMip;
	mediump float x = lowColor.x + delta * (highColor.x - lowColor.x);
	mediump float y = lowColor.y + delta * (highColor.y - lowColor.y);
	mediump float z = lowColor.z + delta * (highColor.z - lowColor.z);
	mediump float w = lowColor.w + delta * (highColor.w - lowColor.w);
	outColor = vec4(x, y, z, w);
}
