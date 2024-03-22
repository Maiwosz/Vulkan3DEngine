#include "Renderer.h"

Renderer::Renderer(Device& device) : m_device(device)
{
	m_swap_chain = std::make_unique<SwapChain>(device);
	m_graphics_pipeline = std::make_shared<GraphicsPipeline>(device, m_swap_chain);
}

Renderer::~Renderer()
{
}
