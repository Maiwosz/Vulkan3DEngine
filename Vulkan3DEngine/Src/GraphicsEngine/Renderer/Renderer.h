#pragma once
#include "..\..\Prerequisites.h"
#include "SwapChain/SwapChain.h"

class Renderer
{
public:
	Renderer(Device& device);
	~Renderer();
private:
	Device& m_device;
	SwapChainPtr m_swap_chain;

};

