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
    mat4 lightSpaceMatrix;
    vec4 viewPos;
    int lightCount;
    int hasShadows;
    Light lights[4];
} global;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 baseColor;
    float metallic;
    float roughness;
    float ao;
    float padding;
} material;

layout(set = 1, binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    
    if (projCoords.z > 1.0) return 0.0;
    
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += projCoords.z - bias > pcfDepth ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    return shadow;
}

vec3 CalcDirLight(Light light, vec3 normal, vec3 viewDir, vec3 materialColor, float shadow) {
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
    
    return (ambient + (1.0 - shadow) * (diffuse + specular)) * materialColor;
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
    
    float shadow = 0.0;
    if (global.hasShadows == 1) {
        vec4 fragPosLightSpace = global.lightSpaceMatrix * vec4(fragFragPos, 1.0);
        // We only support shadow for the first directional light for now
        vec3 lightDir = normalize(-global.lights[0].direction.xyz);
        shadow = ShadowCalculation(fragPosLightSpace, norm, lightDir);
    }

    vec3 result = vec3(0.0);
    
    for(int i = 0; i < global.lightCount; i++) {
        int type = int(global.lights[i].position.w);
        if (type == 0) {
            result += CalcDirLight(global.lights[i], norm, viewDir, objectColor, shadow);
        } else if (type == 1) {
            result += CalcPointLight(global.lights[i], norm, fragFragPos, viewDir, objectColor);
        }
    }
    
    outColor = vec4(result, texColor.a * material.baseColor.a);
}
