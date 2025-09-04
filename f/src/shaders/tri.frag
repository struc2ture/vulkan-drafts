#version 450 core

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUV;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragPos;

layout(location = 0) out vec4 outColor;

layout(std140, set = 0, binding = 0) uniform UBO {
    mat4 proj_view;
    vec3 view_pos;
} ubo;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

void main()
{
    vec3 lightColor = vec3(0.7, 0.6, 0.4);
    vec3 lightPos = vec3(0.0, 10.0, 0.0);

    // ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    // diffuse 
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(ubo.view_pos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec4 c = vec4(fragColor, 1.0);
    vec4 l = vec4(ambient + diffuse + specular, 1.0);
    vec4 t = texture(texSampler, fragUV);
    outColor = l * t * c;
}
