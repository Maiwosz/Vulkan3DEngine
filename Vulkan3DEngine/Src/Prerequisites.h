#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vk_enum_string_helper.h>

#define GLM_FORCE_SILENT_WBERS
#define GLM_FORCE_CXX17
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>

#include <json.hpp>

#include <spdlog/spdlog.h>


#define VK_CHECK(x)                                                         \
    do {                                                                    \
        VkResult err = x;                                                   \
        if (err) {                                                          \
             SPDLOG_ERROR("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                        \
        }                                                                   \
    } while (0)

//struct Vertex {
//    glm::vec3 pos;
//    glm::vec3 color;
//    glm::vec2 texCoord;
//    glm::vec3 normal;
//
//    static VkVertexInputBindingDescription getBindingDescription() {
//        VkVertexInputBindingDescription bindingDescription{};
//        bindingDescription.binding = 0;
//        bindingDescription.stride = sizeof(Vertex);
//        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//
//        return bindingDescription;
//    }
//
//    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions() {
//        std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};
//
//        attributeDescriptions[0].binding = 0;
//        attributeDescriptions[0].location = 0;
//        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
//        attributeDescriptions[0].offset = offsetof(Vertex, pos);
//
//        attributeDescriptions[1].binding = 0;
//        attributeDescriptions[1].location = 1;
//        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
//        attributeDescriptions[1].offset = offsetof(Vertex, color);
//
//        attributeDescriptions[2].binding = 0;
//        attributeDescriptions[2].location = 2;
//        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
//        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
//
//        attributeDescriptions[3].binding = 0;
//        attributeDescriptions[3].location = 3;
//        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
//        attributeDescriptions[3].offset = offsetof(Vertex, normal);
//
//
//        return attributeDescriptions;
//    }
//
//    bool operator==(const Vertex& other) const {
//        return pos == other.pos && color == other.color && texCoord == other.texCoord && normal == other.normal;
//    }
//};
//
//namespace std {
//    template<> struct hash<Vertex> {
//        size_t operator()(Vertex const& vertex) const {
//            return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
//        }
//    };
//}

//struct Pipeline {
//    Pipeline(VkDevice device) : p_device(device) {}
//    ~Pipeline() {
//        vkDestroyPipeline(p_device, pipeline, nullptr);
//        vkDestroyPipelineLayout(p_device, layout, nullptr);
//    }
//    VkDevice p_device;
//    VkPipeline pipeline;
//    VkPipelineLayout layout;
//};

//typedef std::unique_ptr<Pipeline> PipelinePtr;
