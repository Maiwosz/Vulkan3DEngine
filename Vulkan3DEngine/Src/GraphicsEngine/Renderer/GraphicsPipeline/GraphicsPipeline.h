#pragma once
#include "..\..\..\Prerequisites.h"
#include "..\..\GraphicsEngine.h"
#include "..\Device\Device.h"
#include "..\SwapChain\SwapChain.h"
#include "..\..\ResourceManager\MeshManager\Mesh.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <fstream>

class GraphicsPipeline
{
public:
	GraphicsPipeline(Renderer* renderer);
	~GraphicsPipeline();
	VkPipeline& get() { return m_graphicsPipeline; };
	VkPipelineLayout& getLayout() { return m_pipelineLayout; };
private:
	static std::vector<char> readFile(const std::string& filename);
	VkShaderModule createShaderModule(const std::vector<char>& code);
	void createDescriptorSetLayout();

	Renderer* m_renderer = nullptr;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;
};