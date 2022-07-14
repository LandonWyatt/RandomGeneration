#version 430
out vec4 fragColor;
uniform mat4 mv_matrix;
uniform mat4 proj_matrix;
uniform vec3 color;

void main(void) {
	fragColor = vec4(color, 1.0f);
};