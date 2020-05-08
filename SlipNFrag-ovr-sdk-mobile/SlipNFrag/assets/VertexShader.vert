#version 300 es

in vec4 inputPosition;
in vec2 inputTexCoords;

out vec2 texCoords;

void main()
{
    gl_Position = inputPosition;
    texCoords = inputTexCoords;
}