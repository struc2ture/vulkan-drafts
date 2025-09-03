#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inColor;

//layout(set = 0, binding = 0) uniform UBO {
//    mat4 mvp;
//} ubo;

layout(push_constant) uniform Push {
    mat4 mvp;
} push;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

void main()
{
    gl_Position = push.mvp * vec4(inPos, 1.0);
    fragColor = inColor;
    fragUV = inUV;
}
