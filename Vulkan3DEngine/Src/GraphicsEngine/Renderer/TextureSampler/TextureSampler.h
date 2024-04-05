#pragma once
#include "..\..\..\Prerequisites.h"

class TextureSampler
{
public:
	TextureSampler(Renderer* renderer);
	~TextureSampler();

	VkSampler& get() { return m_textureSampler; }
private:
	Renderer* m_renderer = nullptr;
	VkSampler m_textureSampler;
};

