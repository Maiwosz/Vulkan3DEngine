#pragma once
#include "Prerequisites.h"
#include "Buffer.h"

struct UniformBuffer : public Buffer
{
public:
	UniformBuffer(VkDeviceSize bufferSize, Renderer* renderer);
	~UniformBuffer();
	
	void bind() override;
private:
	
};

