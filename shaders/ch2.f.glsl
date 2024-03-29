#version 150

in vec3 Color;
in vec2 Texcoord;

out vec4 outColor;

uniform sampler2D texKitten; // I don't have to bind/set this externally??
uniform sampler2D texPuppy;

void main()
{
  vec4 colKitten = texture(texKitten, Texcoord);
  vec4 colPuppy = texture(texPuppy, Texcoord);
  vec4 texColor = mix(colKitten, colPuppy, 0.5);
  outColor = texColor * vec4(Color, 1.0);
}