#version 300 es

in vec2 texCoords;

out vec4 fragmentColor;

uniform sampler2D console;
uniform sampler2D screen;
uniform sampler2D palette;

void main()
{
    vec4 consoleEntry = texture(console, texCoords);
    vec4 screenEntry = texture(screen, texCoords);
    fragmentColor = texture(palette, vec2((consoleEntry.x < 1.0 ? consoleEntry.x : screenEntry.x), 0));
}