#include "Renderer.h"

Renderer::Renderer(Device& device) : m_device(device)
{
	m_swap_chain = std::make_shared<SwapChain>(&device);
}

Renderer::~Renderer()
{
}
