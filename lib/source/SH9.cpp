// https://graphics.stanford.edu/papers/envmap/prefilter.c

#include "SH9.h"

float SH9::coeffs[9][4] = { 0 }; // 4 for alignment
int SH9::width = 0;
int SH9::height = 0;
int SH9::channels = 0;
float* SH9::data = nullptr;
std::string SH9::shOutputPath = "sh9.txt";

void SH9::init(const char* filename, const char* outputPath) {
	if (outputPath) {
		shOutputPath = outputPath;
	}

	stbi_set_flip_vertically_on_load(true);

	data = stbi_loadf(filename, &width, &height, &channels, 0);
	if (!data) {
		std::cerr << "Failed to load HDR image: " << filename << "\n";
		exit(1);
	}

	prefilter();
}

void SH9::updateCoeffs(const vec3& hdrColor, float domega, float x, float y, float z) {
	for (int col = 0; col < 3; col++) {
		float c;

		c = 0.282095f;
		coeffs[0][col] += hdrColor[col] * c * domega;

		c = 0.488603f;
		coeffs[1][col] += hdrColor[col] * (c * y) * domega;
		coeffs[2][col] += hdrColor[col] * (c * z) * domega;
		coeffs[3][col] += hdrColor[col] * (c * x) * domega;

		c = 1.092548f;
		coeffs[4][col] += hdrColor[col] * (c * x * y) * domega;
		coeffs[5][col] += hdrColor[col] * (c * y * z) * domega;
		coeffs[7][col] += hdrColor[col] * (c * x * z) * domega;

		c = 0.375392f;
		coeffs[6][col] += hdrColor[col] * (c * (3 * z * z - 1)) * domega;

		c = 0.546274f;
		coeffs[8][col] += hdrColor[col] * (c * (x * x - y * y)) * domega;
	}
}

void SH9::prefilter() {
	std::ofstream shFile(shOutputPath);
	if (!shFile.is_open()) {
		std::cerr << "Error: Failed to open sh.txt!\n";
		return;
	}

	// Sample the envrionment map
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			// Convert equirectangular coordinates to spherical coordinates
			float ww = (2.0f * j / width - 1.0f); // [-1, 1]
			float hh = (1.0f - 2.0f * i / float(height)); // [1, -1]

			float theta = ww * 3.14159265359;
			float phi = hh * 3.14159265359 * 0.5; // pi/2

			// Convert to cartesian coordinates (unit vector)
			float x = cos(phi) * sin(theta);
			float y = sin(phi);
			float z = cos(phi) * cos(theta);

			// Calculate solid angle for this pixel
			// The cos(phi) term accounts for the changing density of pixels near poles
			float domega = (2.0f * 3.14159265359 / width) * (3.14159265359 / height) * cos(phi);

			vec3 color = getPixel(j, i);

			updateCoeffs(color, domega, x, -y, z);
		}
	}

	for (int i = 0; i < 9; i++) {
		shFile << coeffs[i][0] << ", " << coeffs[i][1] << ", " << coeffs[i][2] << "\n";
	}
	shFile.close();
}

vec3 SH9::getPixel(int x, int y) {
	x = std::min(std::max(x, 0), width - 1);
	y = std::min(std::max(y, 0), height - 1);

	int idx = (y * width + x) * channels;
	return vec3(data[idx], data[idx + 1], data[idx + 2]);
}