#pragma once
#include "Prerequisites.h"
#include "Buffer.h"

struct StagingBuffer : public Buffer
{
public:
	StagingBuffer(VkDeviceSize bufferSize, Renderer* renderer);
	~StagingBuffer();
	
	void bind() override;
private:
	
};

