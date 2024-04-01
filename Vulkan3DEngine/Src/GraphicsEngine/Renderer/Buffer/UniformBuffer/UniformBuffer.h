#pragma once
#include "../Buffer.h"
#include <vector>

class UniformBuffer : public Buffer
{
public:
	UniformBuffer(Renderer* renderer);
	~UniformBuffer();
	
	void bind() override;
private:
	
};

