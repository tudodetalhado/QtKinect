#version 430 core

uniform vec3 color;

in vec4 v_color;
out vec4 fragColor;

void main()
{
	//fragColor = vec4(color, 1);
	fragColor = v_color;
}

