#pragma once
#include "../../../../Prerequisites.h"
#include "../Buffer.h"

class UniformBuffer : public Buffer
{
public:
	UniformBuffer(Renderer* renderer);
	~UniformBuffer();
	
	void bind() override;
private:
	
};

