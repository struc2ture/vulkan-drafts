#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 proj_view;
} ubo;

layout(push_constant) uniform Push {
    mat4 model;
} push;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragPos;

void main()
{
    gl_Position = ubo.proj_view * push.model * vec4(inPos, 1.0);
    fragColor = inColor;
    fragUV = inUV;
    fragNormal = mat3(transpose(inverse(push.model))) * inNormal;
    fragPos = vec3(push.model * vec4(inPos, 1.0));
}
