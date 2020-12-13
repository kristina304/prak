#version 330 core
layout (location = 0) in vec3 masPos;
layout (location = 1) in vec3 masNormal;
layout (location = 2) in vec2 masTexCoords;
layout (location = 3) in vec3 masTangent;
layout (location = 4) in vec3 masBitangent;

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;

uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    vs_out.FragPos = vec3(model * vec4(masPos, 1.0));   
    vs_out.TexCoords = masTexCoords;
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 T = normalize(normalMatrix * masTangent);
    vec3 N = normalize(normalMatrix * masNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    
    mat3 TBN = transpose(mat3(T, B, N));    
    vs_out.TangentLightPos = TBN * lightPos;
    vs_out.TangentViewPos  = TBN * viewPos;
    vs_out.TangentFragPos  = TBN * vs_out.FragPos;
    gl_Position = projection * view * model * vec4(masPos, 1.0);
}