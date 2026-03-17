#ifndef FXAA_UV_H
#define FXAA_UV_H

void FXAAUV(
	in vec2 fragmentPosition,
	in vec2 viewportSize,
	out vec2 v_rgbNW,
	out vec2 v_rgbNE,
	out vec2 v_rgbSW,
	out vec2 v_rgbSE,
	out vec2 v_rgbM)
{
	vec2 inverseViewport = 1.0f / viewportSize.xy;
	v_rgbNW = (fragmentPosition + vec2(-1.0f, -1.0f)) * inverseViewport;
	v_rgbNE = (fragmentPosition + vec2(1.0f, -1.0f)) * inverseViewport;
	v_rgbSW = (fragmentPosition + vec2(-1.0f, 1.0f)) * inverseViewport;
	v_rgbSE = (fragmentPosition + vec2(1.0f, 1.0f)) * inverseViewport;
	v_rgbM = fragmentPosition * inverseViewport;
}

#endif
