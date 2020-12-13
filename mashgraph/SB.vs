#version 330 core
layout (location = 0) in vec3 masPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = masPos;
    vec4 pos = projection*view*vec4(masPos, 1.0);
    gl_Position = pos.xyww;
}  