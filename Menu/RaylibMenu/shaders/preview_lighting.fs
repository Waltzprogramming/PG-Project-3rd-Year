#version 330

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 lightDirection;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform vec3 viewPosition;

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord)*colDiffuse*fragColor;
    if (texel.a < 0.08) discard;

    vec3 normal = normalize(fragNormal);
    vec3 lightVector = normalize(-lightDirection);
    float diffuse = max(dot(normal, lightVector), 0.0);

    vec3 viewVector = normalize(viewPosition - fragPosition);
    float rim = pow(1.0 - max(dot(normal, viewVector), 0.0), 2.4);
    vec3 lighting = ambientColor + lightColor*diffuse + vec3(0.22, 0.28, 0.36)*rim;

    finalColor = vec4(texel.rgb*lighting, texel.a);
}
