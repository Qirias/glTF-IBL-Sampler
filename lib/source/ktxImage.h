#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "ResultType.h"

struct ktxTexture1;
struct ktxTexture2;

namespace IBLLib
{
    class IKtxImage
    {
    public:
        virtual ~IKtxImage() {};

        virtual Result writeFace(const std::vector<uint8_t>& _inData, uint32_t _side, uint32_t _level) = 0;
        virtual Result save(const char* _pathOut) = 0;

        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;
        virtual uint32_t getLevels() const = 0;
        virtual bool isCubeMap() const = 0;
        virtual VkFormat getFormat() const = 0;
    };

    class KtxImage1 : public IKtxImage
    {
    public:
        // use this constructor if you want to load a ktx file
        KtxImage1();
        // use this constructor if you want to create a ktx file
        KtxImage1(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap);
        ~KtxImage1();

        Result loadKtx1(const char* _pFilePath);

        Result writeFace(const std::vector<uint8_t>& _inData, uint32_t _side, uint32_t _level) override;
        Result save(const char* _pathOut) override;

        uint32_t getWidth() const override;
        uint32_t getHeight() const override;
        uint32_t getLevels() const override;
        bool isCubeMap() const override;
        VkFormat getFormat() const override;

    private:
        ktxTexture1* m_ktxTexture = nullptr;
    };

	class KtxImage2 : public IKtxImage
	{
	public:
		// use this constructor if you want to load a ktx file
		KtxImage2();
		// use this constructor if you want to create a ktx file
		KtxImage2(uint32_t _width, uint32_t _height, VkFormat _vkFormat, uint32_t _levels, bool _isCubeMap);
		~KtxImage2();

		Result loadKtx2(const char* _pFilePath);

		Result writeFace(const std::vector<uint8_t>& _inData, uint32_t _side, uint32_t _level) override;
		Result save(const char* _pathOut) override;

		uint32_t getWidth() const override;
		uint32_t getHeight() const override;
		uint32_t getLevels() const override;
		bool isCubeMap() const override;
		VkFormat getFormat() const override;

	private:
		ktxTexture2* m_ktxTexture = nullptr;
	};

} // !IBLLIb
