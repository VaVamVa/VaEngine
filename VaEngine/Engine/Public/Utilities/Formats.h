#pragma once

// Color
#define RGBA_COLOR_BLACK	0xFF000000
#define RGBA_COLOR_RED		0xFF0000FF
#define RGBA_COLOR_GREEN	0xFF00FF00
#define RGBA_COLOR_BLUE		0xFFFF0000
#define RGBA_COLOR_WHITE	0xFFFFFFFF

// Coordinate

#define LEFTHANDED_TO_RIGHTHANDED(x, y, z)	(-x), y, z
#define RIGHTHANDED_TO_LEFTHANDED(x, y, z)	(-x), y, z

// EX: DirectX
#define COORDINATE_LEFTHANDED 

// EX: Vulkan
#define COORDINATE_RIGHTHANDED