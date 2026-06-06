#version 330 core

struct Material {
    vec3 baseColor;
    vec3 emissive;
    float roughness;
    float checkerStrength;
    float fogAmount;
    bool hasTexture;
    float opacity;
    sampler2D albedoMap;
};

struct DirectionalLight {
    vec3 direction;
    vec3 color;
};

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};

const int MAX_LIGHTS = 24;

in vec3 vWorldPosition;
in vec3 vNormal;
in vec2 vUv;
in vec4 vColor;

uniform Material uMaterial;
uniform DirectionalLight uDirectionalLight;
uniform PointLight uPointLights[MAX_LIGHTS];
uniform int uPointLightCount;
uniform vec3 uAmbientColor;
uniform vec3 uCameraPosition;
uniform vec3 uFogColor;
uniform float uTime;
uniform float uSceneExposure;

out vec4 FragColor;

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec3 normal = normalize(vNormal);
    vec3 viewDirection = normalize(uCameraPosition - vWorldPosition);
    vec3 albedo = uMaterial.baseColor * vColor.rgb;
    float alpha = vColor.a * uMaterial.opacity;

    if (uMaterial.hasTexture) {
        vec4 texel = texture(uMaterial.albedoMap, vUv);
        albedo *= texel.rgb;
        alpha *= texel.a;
    }

    if (alpha < 0.08) {
        discard;
    }

    float checker = step(0.5, fract(vUv.x * 6.0) + fract(vUv.y * 6.0));
    float grain = hash21(vUv * 32.0 + vWorldPosition.xz * 0.2);
    albedo *= mix(1.0, 0.72 + 0.18 * checker + 0.10 * grain, uMaterial.checkerStrength);

    vec3 light = uAmbientColor;
    float ndl = max(dot(normal, normalize(-uDirectionalLight.direction)), 0.0);
    light += uDirectionalLight.color * (0.15 + ndl * 0.65);

    for (int i = 0; i < uPointLightCount; ++i) {
        vec3 toLight = uPointLights[i].position - vWorldPosition;
        float distanceToLight = length(toLight);
        vec3 lightDirection = toLight / max(distanceToLight, 0.001);
        float attenuation = pow(max(1.0 - distanceToLight / uPointLights[i].radius, 0.0), 2.0);
        float diffuse = max(dot(normal, lightDirection), 0.0);
        float rim = pow(max(1.0 - dot(viewDirection, normal), 0.0), 2.0) * 0.16;
        light += uPointLights[i].color * uPointLights[i].intensity * attenuation * (diffuse + rim);
    }

    float heightShade = smoothstep(-1.2, 4.8, vWorldPosition.y);
    light *= mix(1.22, 0.86, heightShade);

    vec3 color = (albedo * light + uMaterial.emissive) * uSceneExposure;
    float fog = smoothstep(18.0, 78.0, length(uCameraPosition - vWorldPosition)) * uMaterial.fogAmount;
    color = mix(color, uFogColor, fog);
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, alpha);
}
