#version 440 core

#extension GL_EXT_shader_io_blocks : enable
#extension GL_ARB_enhanced_layouts : enable

layout( location = 0 ) in lowp vec4 fragmentColor;
layout( location = 0 ) out lowp vec4 outColor;

void main()
{
	outColor = fragmentColor;
}
