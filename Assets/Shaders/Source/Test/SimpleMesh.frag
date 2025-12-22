#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragViewPos;
layout(location = 3) in vec3 fragFragPos;

struct Light {
    vec4 position; // w = type (0=Directional, 1=Point, 2=Spot)
    vec4 direction; // w = range
    vec4 color;     // w = intensity
    vec4 params;    // x = innerCone, y = outerCone
};

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 viewPos;
    int lightCount;
    Light lights[4];
} global;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 baseColor;
    float metallic;
    float roughness;
    float ao;
    float padding;
} material;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

vec3 CalcDirLight(Light light, vec3 normal, vec3 viewDir, vec3 materialColor) {
    vec3 lightDir = normalize(-light.direction.xyz);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float shininess = (1.0 - material.roughness) * 128.0;
    float spec = pow(max(dot(normal, halfwayDir), 0.0), max(shininess, 1.0));
    
    vec3 ambient = 0.1 * light.color.rgb * light.color.a * material.ao; 
    vec3 diffuse = diff * light.color.rgb * light.color.a;
    
    // Simple metallic workflow approximation for specular color
    vec3 specularColor = mix(vec3(1.0), materialColor, material.metallic);
    vec3 specular = 0.5 * spec * light.color.rgb * light.color.a * specularColor;
    
    return (ambient + diffuse + specular) * materialColor;
}

vec3 CalcPointLight(Light light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 materialColor) {
    vec3 lightDir = normalize(light.position.xyz - fragPos);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float shininess = (1.0 - material.roughness) * 128.0;
    float spec = pow(max(dot(normal, halfwayDir), 0.0), max(shininess, 1.0));
    
    // Attenuation
    float distance = length(light.position.xyz - fragPos);
    float attenuation = 1.0;
    if (light.direction.w > 0.0) { // Range
         attenuation = max(0.0, 1.0 - (distance / light.direction.w)); 
         attenuation = 1.0 / (1.0 + 0.1 * distance + 0.01 * distance * distance);
    }
    
    vec3 ambient = 0.1 * light.color.rgb * light.color.a * material.ao;
    vec3 diffuse = diff * light.color.rgb * light.color.a;
    
    vec3 specularColor = mix(vec3(1.0), materialColor, material.metallic);
    vec3 specular = 0.5 * spec * light.color.rgb * light.color.a * specularColor;
    
    return (ambient + diffuse + specular) * materialColor * attenuation;
}

void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 objectColor = texColor.rgb * material.baseColor.rgb;
    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(fragViewPos - fragFragPos);
    
    vec3 result = vec3(0.0);
    
    for(int i = 0; i < global.lightCount; i++) {
        int type = int(global.lights[i].position.w);
        if (type == 0) {
            result += CalcDirLight(global.lights[i], norm, viewDir, objectColor);
        } else if (type == 1) {
            result += CalcPointLight(global.lights[i], norm, fragFragPos, viewDir, objectColor);
        }
    }
    
    outColor = vec4(result, texColor.a * material.baseColor.a);
}
