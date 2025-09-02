#version 450 core

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main()
{
    vec4 t = texture(texSampler, fragUV);
    outColor = t * vec4(fragColor, 1.0);
    //outColor = t;
    //outColor = vec4(t.r, t.g, t.b, 1.0);
    //outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
