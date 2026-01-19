#version 450

layout(location = 0) in vec3 positionA;
layout(location = 1) in vec2 uvA;
layout(location = 2) in mat4 transformA;
layout(location = 6) in uint layersA;
layout(location = 7) in int materialIndexA;

layout(location = 0) out vec2 uv;
layout(location = 1) flat out uint layers;
layout(location = 2) flat out int materialIndex;

void main()
{
	gl_Position = transformA * vec4(positionA, 1.0f);
	materialIndex = materialIndexA;
	layers = layersA;
	uv = uvA;
}
