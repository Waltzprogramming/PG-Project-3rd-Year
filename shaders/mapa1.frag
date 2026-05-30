#version 330 core

in vec4 vertexColor;
in vec2 texCoord;

out vec4 FragColor;

uniform sampler2D tex0;
uniform int modoRender;
uniform vec4 colorMaterial;

void main() {
    if (modoRender == 0) {
        vec4 colorTextura = texture(tex0, texCoord);
        if (colorTextura.a < 0.08) {
            discard;
        }
        FragColor = colorTextura;
    } else if (modoRender == 1) {
        FragColor = vertexColor;
    } else {
        FragColor = colorMaterial;
    }
}
