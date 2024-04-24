#pragma once
#include "..\..\..\Prerequisites.h"
#include <vector>
#include <fstream>
#include <shaderc/shaderc.hpp>

class GraphicsPipeline
{
public:
	GraphicsPipeline(Renderer* renderer);
	~GraphicsPipeline();
	VkPipeline& get() { return m_graphicsPipeline; };
	VkPipelineLayout& getLayout() { return m_pipelineLayout; };
private:
	static std::vector<char> readFile(const std::string& filename);
	VkShaderModule createShaderModule(const std::vector<uint32_t>& code);
	VkShaderModule compileShader(const std::string& filename, shaderc_shader_kind kind);

	Renderer* m_renderer = nullptr;
	VkPipelineLayout m_pipelineLayout;
	VkPipeline m_graphicsPipeline;
};