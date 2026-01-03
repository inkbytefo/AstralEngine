#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragViewPos;
layout(location = 3) in vec3 fragFragPos;
layout(location = 4) in mat3 TBN;

struct Light {
    vec4 position; // w = type (0=Directional, 1=Point, 2=Spot)
    vec4 direction; // w = range
    vec4 color;     // w = intensity
    vec4 params;    // x = innerCone, y = outerCone
};

layout(set = 0, binding = 0) uniform GlobalUBO {
    mat4 view;
    mat4 proj;
    mat4 lightSpaceMatrix;
    vec4 viewPos;
    int lightCount;
    int hasShadows;
    int hasIBL;
    Light lights[4];
} global;

layout(set = 0, binding = 1) uniform sampler2D shadowMap;
layout(set = 0, binding = 2) uniform samplerCube irradianceMap;
layout(set = 0, binding = 3) uniform samplerCube prefilterMap;
layout(set = 0, binding = 4) uniform sampler2D brdfLUT;

layout(set = 1, binding = 0) uniform MaterialUBO {
    vec4 baseColor;
    float metallic;
    float roughness;
    float ao;
    float emissiveIntensity;
    vec4 emissiveColor;
    int useNormalMap;
    int useMetallicMap;
    int useRoughnessMap;
    int useAOMap;
    int useEmissiveMap;
} material;

// Texture Bindings
layout(set = 1, binding = 1) uniform sampler2D albedoMap;
layout(set = 1, binding = 2) uniform sampler2D normalMap;
layout(set = 1, binding = 3) uniform sampler2D metallicMap;
layout(set = 1, binding = 4) uniform sampler2D roughnessMap;
layout(set = 1, binding = 5) uniform sampler2D aoMap;
layout(set = 1, binding = 6) uniform sampler2D emissiveMap;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// ... PBR Functions ...

vec3 getNormalFromMap() {
    if (material.useNormalMap == 0) return normalize(fragNormal);
    
    vec3 tangentNormal = texture(normalMap, fragTexCoord).xyz * 2.0 - 1.0;
    return normalize(TBN * tangentNormal);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ShadowCalculation(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    if (global.hasShadows == 0) return 0.0;
    
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    
    if (projCoords.z > 1.0) return 0.0;
    
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    float currentDepth = projCoords.z;
    
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    return shadow;
}

void main() {
    vec3 albedo = material.baseColor.rgb;
    if (material.useMetallicMap == 1) albedo *= texture(albedoMap, fragTexCoord).rgb;
    
    float metallic = material.metallic;
    if (material.useMetallicMap == 1) metallic *= texture(metallicMap, fragTexCoord).r;
    
    float roughness = material.roughness;
    if (material.useRoughnessMap == 1) roughness *= texture(roughnessMap, fragTexCoord).r;
    
    float ao = material.ao;
    if (material.useAOMap == 1) ao *= texture(aoMap, fragTexCoord).r;

    vec3 N = getNormalFromMap();
    vec3 V = normalize(fragViewPos - fragFragPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < global.lightCount; ++i) {
        Light light = global.lights[i];
        vec3 L;
        float attenuation = 1.0;
        
        if(light.position.w == 0.0) { // Directional
            L = normalize(-light.direction.xyz);
        } else { // Point
            L = normalize(light.position.xyz - fragFragPos);
            float distance = length(light.position.xyz - fragFragPos);
            attenuation = 1.0 / (distance * distance);
            
            if(light.position.w == 2.0) { // Spot
                float theta = dot(L, normalize(-light.direction.xyz));
                float epsilon = light.params.x - light.params.y;
                float intensity = clamp((theta - light.params.y) / epsilon, 0.0, 1.0);
                attenuation *= intensity;
            }
        }
        
        vec3 H = normalize(V + L);
        vec3 radiance = light.color.rgb * light.color.w * attenuation;

        float NDF = DistributionGGX(N, H, roughness);   
        float G   = GeometrySmith(N, V, L, roughness);      
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
           
        vec3 numerator    = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
        
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  

        float NdotL = max(dot(N, L), 0.0);        

        float shadow = 0.0;
        if(light.position.w == 0.0) { // Directional shadows for now
            vec4 fragPosLightSpace = global.lightSpaceMatrix * vec4(fragFragPos, 1.0);
            shadow = ShadowCalculation(fragPosLightSpace, N, L);
        }

        Lo += (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);
    }   

    // IBL ambient lighting
    vec3 ambient;
    if (global.hasIBL == 1) {
        vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
        
        vec3 kS = F;
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;	  
        
        vec3 irradiance = texture(irradianceMap, N).rgb;
        vec3 diffuse = irradiance * albedo;
        
        // Sample pre-filter map and BRDF LUT
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;    
        vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
        vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
        
        ambient = (kD * diffuse + specular) * ao;
    } else {
        ambient = vec3(0.03) * albedo * ao;
    }

    vec3 color = ambient + Lo;

    // Emissive
    vec3 emissive = material.emissiveColor.rgb * material.emissiveIntensity;
    if (material.useEmissiveMap == 1) emissive *= texture(emissiveMap, fragTexCoord).rgb;
    color += emissive;

    // HDR tone mapping and gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color, 1.0);
}
