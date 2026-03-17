#version 450 core

#include "Shaders/Includes/Camera.h"
#include "Shaders/Includes/CSM.h"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3 * MAX_CASCADE_COUNT) out;

layout(set = 2, binding = 0) uniform LightSpaceMatrices
{
    mat4 lightSpaceMatrices[MAX_CASCADE_COUNT];
    int cascadeCount;
};

layout(location = 0) in vec2 uvG[];
layout(location = 1) flat in uint layersG[];
layout(location = 2) flat in int materialIndexG[];

layout(location = 0) out vec2 uv;
layout(location = 1) flat out int materialIndex;

void main()
{
    for (int i = 0; i < cascadeCount; ++i)
    {
		if (((layersG[0] >> i) & 1) == 1)
		{
			for (int j = 0; j < 3; ++j)
			{
				gl_Position = lightSpaceMatrices[i] * gl_in[j].gl_Position;
				gl_Layer = i;
				uv = uvG[j];
				materialIndex = materialIndexG[j];
				EmitVertex();
			}
        	EndPrimitive();
		}
    }
}
