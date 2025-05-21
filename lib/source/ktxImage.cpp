#include "ktxImage.h"

#include <stdio.h>

#include <ktx.h>
#include <ktxvulkan.h>
#include <vulkan/vulkan.h>

#include <cassert>

constexpr uint32_t GL_RGBA8 = 0x8058;
constexpr uint32_t GL_RGBA16F = 0x881A;
constexpr uint32_t GL_RGBA32F = 0x8814;
constexpr uint32_t GL_R11F_G11F_B10F = 0x8C3A; // 35898 decimal

uint32_t toOpenGL(VkFormat _vkFormat)
{
    switch (_vkFormat)
    {
    case VK_FORMAT_R8G8B8A8_UNORM:
        return GL_RGBA8;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return GL_RGBA16F;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return GL_RGBA32F;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        return GL_R11F_G11F_B10F;
    }

    return 0;
}

VkFormat fromOpenGL(uint32_t _glFormat)
{
    switch (_glFormat)
    {
    case GL_RGBA8:
         return VK_FORMAT_R8G8B8A8_UNORM;
    case GL_RGBA16F:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case GL_RGBA32F:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case GL_R11F_G11F_B10F:
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    }

    return VK_FORMAT_UNDEFINED;
}

using namespace IBLLib;

KtxImage1::KtxImage1()
{
}

KtxImage1::KtxImage1(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap)
{
    ktxTextureCreateInfo createInfo;
    createInfo.baseWidth = _width;
    createInfo.baseHeight = _height;
    createInfo.glInternalformat = toOpenGL(_vkFormat);
    createInfo.baseDepth = 1u;
    createInfo.numDimensions = 2u;
    createInfo.numLevels = _levels;
    createInfo.numLayers = 1u;
    createInfo.numFaces = _isCubeMap ? 6u : 1u;
    createInfo.isArray = KTX_FALSE;
    createInfo.generateMipmaps = KTX_FALSE;

    KTX_error_code result;
    result = ktxTexture1_Create(&createInfo,
        KTX_TEXTURE_CREATE_ALLOC_STORAGE,
        &m_ktxTexture);
    if (result != KTX_SUCCESS)
    {
        printf("Could not create ktx texture\n");
        m_ktxTexture = nullptr;
    }
}

KtxImage1::~KtxImage1()
{
    ktxTexture_Destroy(ktxTexture(m_ktxTexture));
}

Result KtxImage1::loadKtx1(const char* _pFilePath)
{
    assert(((void)"m_ktxTexture must be uninitialized.", m_ktxTexture != nullptr));

    KTX_error_code result;
    result = ktxTexture1_CreateFromNamedFile(_pFilePath,
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &m_ktxTexture);

    if (result != KTX_SUCCESS)
    {
        printf("Could not load ktx file at %s \n", _pFilePath);
    }

    return Result::Success;
}

Result KtxImage1::writeFace(const std::vector<uint8_t>& _inData, uint32_t _side, uint32_t _level)
{
    KTX_error_code result = ktxTexture_SetImageFromMemory(ktxTexture(m_ktxTexture), _level, 0u, _side, _inData.data(), _inData.size());

    if (result != KTX_SUCCESS)
    {
        printf("Could not write image data to ktx texture\n");
        return Result::KtxError;
    }

    return Success;
}

Result KtxImage1::save(const char* _pathOut)
{

    KTX_error_code result = ktxTexture_WriteToNamedFile(ktxTexture(m_ktxTexture), _pathOut);

    if (result != KTX_SUCCESS)
    {
        printf("Could not write ktx file\n");
        return Result::KtxError;
    }

    return Success;
}

uint32_t KtxImage1::getWidth() const
{
    assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
    return m_ktxTexture->baseWidth;
}

uint32_t KtxImage1::getHeight() const
{
    assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
    return m_ktxTexture->baseHeight;
}

uint32_t KtxImage1::getLevels() const
{
    assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
    return m_ktxTexture->numLevels;
}

bool KtxImage1::isCubeMap() const
{
    assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
    return m_ktxTexture->numFaces == 6u;
}

VkFormat KtxImage1::getFormat() const
{
    assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
    return  static_cast<VkFormat>(fromOpenGL(m_ktxTexture->glInternalformat));
}


KtxImage2::KtxImage2()
{
}

KtxImage2::KtxImage2(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap)
{
		// fill the create info for ktx2 (we don't support ktx 1)
	ktxTextureCreateInfo createInfo;
	createInfo.vkFormat = _vkFormat;
	createInfo.baseWidth = _width;
	createInfo.baseHeight = _height;
	createInfo.baseDepth = 1u;
	createInfo.numDimensions = 2u;
	createInfo.numLevels = _levels;
	createInfo.numLayers = 1u;
	createInfo.numFaces = _isCubeMap ? 6u : 1u;
	createInfo.isArray = KTX_FALSE;
	createInfo.generateMipmaps = KTX_FALSE;

	KTX_error_code result;
	result = ktxTexture2_Create(&createInfo,
															KTX_TEXTURE_CREATE_ALLOC_STORAGE,
															&m_ktxTexture);
	if(result != KTX_SUCCESS)
	{
		printf("Could not create ktx texture\n");
		m_ktxTexture = nullptr;
	}
}


KtxImage2::~KtxImage2()
{
	ktxTexture_Destroy(ktxTexture(m_ktxTexture));
}

Result KtxImage2::loadKtx2(const char* _pFilePath)
{
	assert(((void)"m_ktxTexture must be uninitialized.", m_ktxTexture != nullptr));

	KTX_error_code result;
	result = ktxTexture2_CreateFromNamedFile(_pFilePath,
																					KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
																					&m_ktxTexture);

	if(result != KTX_SUCCESS)
	{
		printf("Could not load ktx file at %s \n", _pFilePath);
	}

	return Result::Success;
}

Result KtxImage2::writeFace(const std::vector<uint8_t>& _inData, uint32_t _side, uint32_t _level)
{
	KTX_error_code result = ktxTexture_SetImageFromMemory(ktxTexture(m_ktxTexture), _level, 0u, _side, _inData.data(), _inData.size());

	if(result != KTX_SUCCESS)
	{
		printf("Could not write image data to ktx texture\n");
		return Result::KtxError;
	}

	return Success;
}

Result KtxImage2::save(const char* _pathOut)
{

	KTX_error_code result = ktxTexture_WriteToNamedFile(ktxTexture(m_ktxTexture), _pathOut);

	if(result != KTX_SUCCESS)
	{
		printf("Could not write ktx file\n");
		return Result::KtxError;
	}

	return Success;
}

uint32_t KtxImage2::getWidth() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return m_ktxTexture->baseWidth;
}

uint32_t KtxImage2::getHeight() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return m_ktxTexture->baseHeight;
}

uint32_t KtxImage2::getLevels() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return m_ktxTexture->numLevels;
}

bool KtxImage2::isCubeMap() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return m_ktxTexture->numFaces == 6u;
}

VkFormat KtxImage2::getFormat() const
{
	assert(((void)"Ktx texture must be initialized", m_ktxTexture == nullptr));
	return static_cast<VkFormat>(m_ktxTexture->vkFormat);
}
