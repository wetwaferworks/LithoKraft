#version 300 es
precision highp float;
precision highp int;
precision highp sampler2DArray;
precision highp sampler3D;
uniform vec3 uC; out vec4 frag;
void main() { frag = vec4(uC, 1.0); }
