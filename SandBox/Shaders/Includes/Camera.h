struct Wind
{
	float strength;
	vec3 direction;
    float frequency;
};

struct Camera
{
	mat4 viewProjectionMat4;
	mat4 projectionMat4;
	mat4 viewMat4;
	mat4 inverseViewMat4;
	mat4 inverseRotationMat4;
	vec4 frustumPlanes[6];
	vec2 viewportSize;
	float aspectRatio;
	float tanHalfFOV;
	float time;
	float deltaTime;
	float zFar;
	float zNear;
	vec3 positionViewSpace;
	vec3 positionWorldSpace;
	Wind wind;
};

vec3 CalculatePositionFromDepth(
	in float depth,
	in mat4 projectionMat4,
	in vec2 viewRay)
{
    float z = -projectionMat4[3][2] / (depth + projectionMat4[2][2]);
    float x = viewRay.x * z;
    float y = viewRay.y * z;
    return vec3(x, y, z);
}

vec2 OctEncode(vec3 n)
{
	n.xy /= (abs(n.x) + abs(n.y) + abs(n.z));

	if (n.z < 0.0f)
	{
	    n.xy = (1.0f - abs(n.yx)) * sign(n.xy);
	}

	return n.xy * 0.5f + 0.5f;
}

vec3 OctDecode(vec2 f)
{
	f = f * 2.0f - 1.0f;
	vec3 n = vec3(f.x, f.y, 1.0f - abs(f.x) - abs(f.y));

	if (n.z < 0.0f)
	{
	    n.xy = (1.0f - abs(n.yx)) * sign(n.xy);
	}

	return normalize(n);
}

#ifndef POISSON_DISK_DEFINED
#define POISSON_DISK_DEFINED
const vec2 poissonDisk[16] = vec2[](
   vec2(-0.94201624, -0.39906216),
   vec2(0.94558609, -0.76890725),
   vec2(-0.094184101, -0.92938870),
   vec2(0.34495938, 0.29387760),
   vec2(-0.91588581, 0.45771432),
   vec2(-0.81544232, -0.87912464),
   vec2(-0.38277543, 0.27676845),
   vec2(0.97484398, 0.75648379),
   vec2(0.44323325, -0.97511554),
   vec2(0.53742981, -0.47373420),
   vec2(-0.26496911, -0.41893023),
   vec2(0.79197514, 0.19090188),
   vec2(-0.24188840, 0.99706507),
   vec2(-0.81409955, 0.91437590),
   vec2(0.19984126, 0.78641367),
   vec2(0.14383161, -0.14100790)
);
#endif