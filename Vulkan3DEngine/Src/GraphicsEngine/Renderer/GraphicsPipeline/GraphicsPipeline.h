#pragma once
#include "..\..\..\Prerequisites.h"
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

	Renderer* m_renderer = nullptr;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;
};