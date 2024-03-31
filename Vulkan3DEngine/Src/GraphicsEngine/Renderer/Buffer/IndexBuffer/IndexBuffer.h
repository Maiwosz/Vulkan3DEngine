#pragma once
#include "../Buffer.h"
#include <vector>

class IndexBuffer : public Buffer
{
public:
	IndexBuffer(std::vector<uint16_t> indices, Renderer* renderer);
	~IndexBuffer();
	
	void bind() override;
private:
	
};

