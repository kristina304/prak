#version 330 core
layout (location = 0) in vec3 masPos;
layout (location = 1) in vec3 masNormal;

out vec3 Normal;
out vec3 Position;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    Normal = mat3(transpose(inverse(model)))*masNormal;
    Position = vec3(model*vec4(masPos, 1.0));
    gl_Position = projection*view*model*vec4(masPos, 1.0);
}