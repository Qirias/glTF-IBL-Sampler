#pragma once
#include "ResultType.h"

namespace IBLLib
{
	enum class OutputFormat
	{
		R8G8B8A8_UNORM = 37,
		R16G16B16A16_SFLOAT = 97,
		R32G32B32A32_SFLOAT = 109,
		B10G11R11_UFLOAT_PACK32 = 122
	};

	enum class Distribution : unsigned int 
	{
		Lambertian = 0,
		GGX = 1,
		Charlie = 2,
		GGXCubeMap = 3
	};

	Result sample(const char* _inputPath, const char* _outputPathCubeMap, const char* _outputPathLUT, const char* _outputPathSH, Distribution _distribution, unsigned int  _cubemapResolution, unsigned int _mipmapCount, unsigned int _sampleCount, OutputFormat _targetFormat, float _lodBias, bool _debugOutput);
} // !IBLLib