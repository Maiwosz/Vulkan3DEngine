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
private:
	void createGraphicsPipeline();
	static std::vector<char> readFile(const std::string& filename);
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void createRenderPass();

	Device& m_device;
	SwapChainPtr m_swapchain;
	VkRenderPass m_renderPass;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;

};