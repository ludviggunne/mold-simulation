#version 450 core

in vec2 v_TexCoord;

layout (location = 0) out vec4 f_Color;

uniform sampler2D u_Sampler;

void main() {

    f_Color = texture(u_Sampler, v_TexCoord);
}