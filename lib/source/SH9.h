#pragma once

#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include "vec3.h"
#include <stb_image.h>

class SH9 {
public:
	static float coeffs[9][4];
	static int width, height, channels;
	static float* data;
	static std::string shOutputPath;

	static void init(const char* filename, const char* outputPath = "sh.txt");
	static void updateCoeffs(const vec3& hdrCOlor, float domega, float x, float y, float z);
	static void prefilter();
	static vec3 getPixel(int x, int y);
};