#include "DescriptorAllocator.h"
#include "Prerequisites.h"

DescriptorAllocator::DescriptorAllocator(LogicalDevice& device, const PoolConfig& config):
	m_device(device), m_config(config), m_nextSetCount(config.initialSets) 
{
    m_readyPools.push_back(getPool());
}

void DescriptorAllocator::reset() {
    for (auto pool : m_readyPools) {
        vkResetDescriptorPool(m_device.get(), pool, 0);
    }
    for (auto pool : m_fullPools) {
        vkResetDescriptorPool(m_device.get(), pool, 0);
        m_readyPools.push_back(pool);
    }
    m_fullPools.clear();
}

void DescriptorAllocator::destroy() {
    for (auto pool : m_readyPools) {
        vkDestroyDescriptorPool(m_device.get(), pool, nullptr);
    }
    for (auto pool : m_fullPools) {
        vkDestroyDescriptorPool(m_device.get(), pool, nullptr);
    }
    m_readyPools.clear();
    m_fullPools.clear();
}

VkDescriptorSet DescriptorAllocator::allocate(VkDescriptorSetLayout layout) {
    VkDescriptorPool pool = getPool();

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };

    VkDescriptorSet descriptorSet;
    VkResult result = vkAllocateDescriptorSets(m_device.get(), &allocInfo, &descriptorSet);

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY) {
        m_fullPools.push_back(pool);
        pool = getPool();
        allocInfo.descriptorPool = pool;
        VK_CHECK(vkAllocateDescriptorSets(m_device.get(), &allocInfo, &descriptorSet));
    }

    m_readyPools.push_back(pool);
    return descriptorSet;
}

VkResult DescriptorAllocator::createPool(uint32_t setCount, VkDescriptorPool* outPool) const {
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (const auto& ratio : m_config.ratios) {
        uint32_t descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount);
        if (descriptorCount == 0) continue;

        poolSizes.push_back({
            .type = ratio.type,
            .descriptorCount = descriptorCount
            });
    }

    if (poolSizes.empty()) {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = setCount,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    return vkCreateDescriptorPool(m_device.get(), &poolInfo, nullptr, outPool);
}

VkDescriptorPool DescriptorAllocator::getPool() {
    if (!m_readyPools.empty()) {
        VkDescriptorPool pool = m_readyPools.back();
        m_readyPools.pop_back();
        return pool;
    }

    const uint32_t minSetCount = computeMinSetCount();
    uint32_t newSetCount = m_nextSetCount;

    while (newSetCount >= minSetCount) {
        VkDescriptorPool pool;
        VkResult result = createPool(newSetCount, &pool);

        if (result == VK_SUCCESS) {
            m_nextSetCount = static_cast<uint32_t>(newSetCount * m_config.growthFactor);
            return pool;
        }
        else if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
            newSetCount = std::max(newSetCount / 2, minSetCount);
        }
        else {
            VK_CHECK(result);
        }
    }

    VK_CHECK(VK_ERROR_OUT_OF_DEVICE_MEMORY);
    return VK_NULL_HANDLE;
}

uint32_t DescriptorAllocator::computeMinSetCount() const {
    float minSets = 1.0f;
    for (const auto& ratio : m_config.ratios) {
        if (ratio.ratio <= 0.0f) continue;
        minSets = std::max(minSets, std::ceil(1.0f / ratio.ratio));
    }
    return static_cast<uint32_t>(minSets);
}