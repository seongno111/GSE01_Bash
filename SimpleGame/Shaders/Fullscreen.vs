#version 330 core
out vec2 v_UV;
void main() {
    // One oversized triangle avoids a diagonal seam; UV origin stays bottom-left.
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    v_UV = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
