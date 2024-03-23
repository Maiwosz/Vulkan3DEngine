#pragma once
#include "..\..\..\Prerequisites.h"
#include "..\..\GraphicsEngine.h"
#include "..\..\Device\Device.h"
#include "..\SwapChain\SwapChain.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <fstream>

class GraphicsPipeline
{
public:
	GraphicsPipeline(Device& device, SwapChainPtr swapchain);
	~GraphicsPipeline();
	VkPipeline get() { return m_graphicsPipeline; };
private:
	void createGraphicsPipeline();
	static std::vector<char> readFile(const std::string& filename);
	VkShaderModule createShaderModule(const std::vector<char>& code);
	

	Device& m_device;
	SwapChainPtr m_swapchain;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;

};