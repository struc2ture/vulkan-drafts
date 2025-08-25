#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 mvp;
} ubo;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

void main()
{
    vec2 uvs[6] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0)
    );

    gl_Position = ubo.mvp * vec4(inPos, 0.0, 1.0);
    fragColor = inColor;
    fragUV = uvs[gl_VertexIndex];
}
