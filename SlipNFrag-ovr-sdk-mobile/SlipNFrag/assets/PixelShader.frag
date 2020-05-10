#version 300 es

in vec2 texCoords;

out vec4 fragmentColor;

uniform sampler2D console;
uniform sampler2D screen;
uniform sampler2D palette;

void main()
{
    float consoleEntry = texture(console, texCoords).x;
    if (consoleEntry < 1.0f)
    {
        fragmentColor = texture(palette, vec2(consoleEntry, 0));
    }
    else
    {
        float screenEntry = texture(screen, texCoords).x;
        fragmentColor = texture(palette, vec2(screenEntry, 0));
    }
}