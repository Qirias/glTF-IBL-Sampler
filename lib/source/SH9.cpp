// https://graphics.stanford.edu/papers/envmap/prefilter.c

#include "SH9.h"

float SH9::coeffs[9][4] = { 0 }; // 4 for alignment
int SH9::width = 0;
int SH9::height = 0;
int SH9::channels = 0;
float* SH9::data = nullptr;
std::string SH9::shOutputPath = "sh9.txt";
unsigned int SH9::maxHdrValue = 10u;

void SH9::init(const char* filename, const char* outputPath, const unsigned int hdrValue) {
	if (outputPath) {
		shOutputPath = outputPath;
	}
	maxHdrValue = hdrValue;

	stbi_set_flip_vertically_on_load(true);

	data = stbi_loadf(filename, &width, &height, &channels, 0);
	if (!data) {
		std::cerr << "Failed to load HDR image: " << filename << "\n";
		exit(1);
	}

	prefilter();
}

void SH9::updateCoeffs(const vec3& hdrColor, float domega, float x, float y, float z) {
	// The coefficients are meant to be evaluated at runtime by simply multiplying with the
	// direction-based terms (1, y, z, x, x*y, ...). The c * cosA * c folding does three things:
	//   - project the radiance sample onto the SH basis (radiance SH, the first c)
	//   - convolve the SH with the clamped cosine kernel (irradiance SH, cosA)
	//   - project the runtime sample direction onto the SH basis (the second c)
	// cosA also folds in the Lambert 1/pi (cosA = A_l / pi: 1, 2/3, 1/4) so the runtime does not
	// need to multiply by 1/pi when applying irradiance (Lambert diffuse BRDF).
	const float cosA0 = 1.0f;
	const float cosA1 = 2.0f / 3.0f;
	const float cosA2 = 1.0f / 4.0f;
	for (int col = 0; col < 3; col++) {
		float c;

		c = 0.282095f;
		coeffs[0][col] += hdrColor[col] * c * cosA0 * c * domega;

		c = 0.488603f;
		coeffs[1][col] += hdrColor[col] * (c * cosA1 * c * y) * domega;
		coeffs[2][col] += hdrColor[col] * (c * cosA1 * c * z) * domega;
		coeffs[3][col] += hdrColor[col] * (c * cosA1 * c * x) * domega;

		c = 1.092548f;
		coeffs[4][col] += hdrColor[col] * (c * cosA2 * c * x * y) * domega;
		coeffs[5][col] += hdrColor[col] * (c * cosA2 * c * y * z) * domega;
		coeffs[7][col] += hdrColor[col] * (c * cosA2 * c * x * z) * domega;

		c = 0.315392f;
		coeffs[6][col] += hdrColor[col] * (c * cosA2 * c * (3 * z * z - 1)) * domega;

		c = 0.546274f;
		coeffs[8][col] += hdrColor[col] * (c * cosA2 * c * (x * x - y * y)) * domega;
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

			float theta = ww * 3.14159265359f;
			float phi = hh * 3.14159265359f * 0.5f; // pi/2

			// Convert to cartesian coordinates (unit vector)
			float x = cos(phi) * sin(theta);
			float y = sin(phi);
			float z = cos(phi) * cos(theta);

			// Calculate solid angle for this pixel
			// The cos(phi) term accounts for the changing density of pixels near poles
			float domega = (2.0f * 3.14159265359f / width) * (3.14159265359f / height) * cos(phi);

			vec3 color = clamp(getPixel(j, i), 0.0, static_cast<float>(maxHdrValue));

			vec3 direction = vec3(x, y, z);

			direction[0] = -direction[0];
			direction[1] = -direction[1];

			updateCoeffs(color, domega, direction[0], direction[1], direction[2]);
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