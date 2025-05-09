#include "DescriptorLayoutManager.h"
#include "Prerequisites.h"

DescriptorLayoutManager::DescriptorLayoutManager(const LogicalDevice& device)
    : m_device(device) {
    createBuiltInLayouts();
}

DescriptorLayoutManager::~DescriptorLayoutManager() {
    // Clean up all descriptor set layouts
    for (const auto& [handle, layout] : m_layouts) {
        vkDestroyDescriptorSetLayout(m_device.get(), layout, nullptr);
    }
    m_layouts.clear();
}

void DescriptorLayoutManager::createBuiltInLayouts() {
    // Create Global layout (uniform buffer at binding 0)
    DescriptorLayoutBuilder globalBuilder;
    globalBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_globalLayout = create(globalBuilder, VK_SHADER_STAGE_ALL);

    // Create Object layout (uniform buffer at binding 0)
    DescriptorLayoutBuilder objectBuilder;
    objectBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_objectLayout = create(objectBuilder, VK_SHADER_STAGE_ALL);
}

DescriptorLayoutHandle DescriptorLayoutManager::create(
    const DescriptorLayoutBuilder& builder,
    VkShaderStageFlags shaderStages,
    void* pNext,
    VkDescriptorSetLayoutCreateFlags flags
) {
    std::vector<VkDescriptorSetLayoutBinding> bindings = builder.getBindings();
    for (auto& b : bindings) {
        b.stageFlags |= shaderStages;
    }

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.pNext = pNext;
    createInfo.flags = flags;
    createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    createInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(m_device.get(), &createInfo, nullptr, &layout));

    DescriptorLayoutHandle handle(m_nextHandle++);
    m_layouts.emplace(handle, layout);

    return handle;
}

void DescriptorLayoutManager::destroy(DescriptorLayoutHandle handle) {
    // Don't allow destruction of built-in layouts
    if (handle == m_globalLayout || handle == m_objectLayout) {
        return;
    }

    auto it = m_layouts.find(handle);
    if (it != m_layouts.end()) {
        vkDestroyDescriptorSetLayout(m_device.get(), it->second, nullptr);
        m_layouts.erase(it);
    }
}

VkDescriptorSetLayout DescriptorLayoutManager::get(DescriptorLayoutHandle handle) const {
    auto it = m_layouts.find(handle);
    return (it != m_layouts.end()) ? it->second : VK_NULL_HANDLE;
}

bool DescriptorLayoutManager::isValid(DescriptorLayoutHandle handle) const {
    return m_layouts.contains(handle);
}

DescriptorLayoutHandle DescriptorLayoutManager::getBuiltInLayout(BuiltInLayout layout) const {
    switch (layout) {
    case BuiltInLayout::Global:
        return m_globalLayout;
    case BuiltInLayout::Object:
        return m_objectLayout;
    default:
        return DescriptorLayoutHandle(0); // Invalid handle
    }
}

VkDescriptorSetLayout DescriptorLayoutManager::getBuiltInVkLayout(BuiltInLayout layout) const {
    return get(getBuiltInLayout(layout));
}