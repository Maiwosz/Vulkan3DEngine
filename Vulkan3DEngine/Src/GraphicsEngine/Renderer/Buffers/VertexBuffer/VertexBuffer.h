#pragma once
#include "../../../../Prerequisites.h"
#include "../Buffer.h"
#include <vector>

class VertexBuffer: public Buffer
{
public:
	VertexBuffer(std::vector<Vertex> vertices, Renderer* renderer);
	~VertexBuffer();
	
	void bind() override;
private:

};

