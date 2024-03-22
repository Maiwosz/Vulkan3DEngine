#pragma once
#include "../../Prerequisites.h"
#include "SwapChain/SwapChain.h"
#include "GraphicsPipeline/GraphicsPipeline.h"

class Renderer
{
public:
	Renderer(Device& device);
	~Renderer();
private:
	Device& m_device;
	SwapChainPtr m_swap_chain;
	GraphicsPipelinePtr m_graphics_pipeline;
};

