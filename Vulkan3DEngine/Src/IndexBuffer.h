#pragma once
#include "Prerequisites.h"
#include "Buffer.h"
#include <vector>

class IndexBuffer : public Buffer
{
public:
	IndexBuffer(std::vector<uint32_t> indices, Renderer* renderer);
	~IndexBuffer();
	
	void bind() override;
private:
	
};

