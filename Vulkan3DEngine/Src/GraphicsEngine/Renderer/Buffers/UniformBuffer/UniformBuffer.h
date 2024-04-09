#pragma once
#include "../../../../Prerequisites.h"
#include "../Buffer.h"

class UniformBuffer : public Buffer
{
public:
	UniformBuffer(VkDeviceSize bufferSize, Renderer* renderer);
	~UniformBuffer();
	
	void bind() override;
private:
	
};

