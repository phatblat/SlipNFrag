#version 440 core

#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable

layout(binding = 1) uniform sampler2D fragmentTexture;
layout(binding = 2) uniform sampler2D fragmentPalette;
layout(location = 0) in mediump vec2 fragmentTexCoords;
layout(location = 0) out lowp vec4 outColor;

void main()
{
	mediump vec2 level = textureQueryLod(fragmentTexture, fragmentTexCoords);
	mediump float lowMip = floor(level.y);
	mediump float highMip = ceil(level.y);
	mediump vec4 lowEntry = textureLod(fragmentTexture, fragmentTexCoords, lowMip);
	mediump vec4 highEntry = textureLod(fragmentTexture, fragmentTexCoords, highMip);
	mediump vec4 lowColor = texelFetch(fragmentPalette, ivec2(lowEntry.x * 255.0, 0), 0);
	mediump vec4 highColor = texelFetch(fragmentPalette, ivec2(highEntry.x * 255.0, 0), 0);
	mediump float delta = level.y - lowMip;
	mediump float x = lowColor.x + delta * (highColor.x - lowColor.x);
	mediump float y = lowColor.y + delta * (highColor.y - lowColor.y);
	mediump float z = lowColor.z + delta * (highColor.z - lowColor.z);
	mediump float w = lowColor.w + delta * (highColor.w - lowColor.w);
	outColor = vec4(x, y, z, w);
}
