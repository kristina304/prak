#version 330 core
layout (location = 0) in vec3 masPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix*model*vec4(masPos, 1.0);
}