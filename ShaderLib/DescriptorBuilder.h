#pragma once
#include "ShaderLib.h"

namespace ShaderLib
{
    class DescriptorSetBuilder {
    private:
        uint32_t setNumber_;
        std::vector<DescriptorSlot> slots_;
        std::unordered_map<std::string, BufferObject> buffers_;

    public:
        DescriptorSetBuilder(uint32_t setNumber) : setNumber_(setNumber) {}

        // Add buffer with full BufferObject
        DescriptorSetBuilder& AddBuffer(uint32_t binding,
            BufferObject buffer,
            StageFlags stages) {
            DescriptorType type = buffer.IsUniformBuffer()
                ? DescriptorType::UniformBuffer
                : DescriptorType::StorageBuffer;

            std::string name = buffer.name;

            // Add slot
            slots_.push_back({
                binding,
                type,
                stages,
                name
                });

            // Store buffer data
            buffers_[name] = std::move(buffer);

            return *this;
        }

        // Add sampler/image (no buffer data needed)
        DescriptorSetBuilder& AddSampler(uint32_t binding,
            const std::string& name,
            DescriptorType samplerType,
            StageFlags stages) {
            slots_.push_back({
                binding,
                samplerType,
                stages,
                name
                });
            return *this;
        }

        // Add descriptor slot only (for samplers/images)
        DescriptorSetBuilder& AddDescriptor(uint32_t binding,
            const std::string& name,
            DescriptorType type,
            StageFlags stages) {
            if (type == DescriptorType::UniformBuffer ||
                type == DescriptorType::StorageBuffer) {
                throw std::invalid_argument(
                    "Use AddBuffer() for buffer descriptors"
                );
            }

            slots_.push_back({
                binding,
                type,
                stages,
                name
                });
            return *this;
        }

        DescriptorSet Build() {
            DescriptorSet set;
            set.setNumber = setNumber_;
            set.slots = std::move(slots_);
            set.buffers = std::move(buffers_);

            if (set.HasBindingConflict()) {
                throw std::runtime_error(
                    "Binding conflict in descriptor set " +
                    std::to_string(setNumber_)
                );
            }

            if (!set.ValidateBuffers()) {
                throw std::runtime_error(
                    "Buffer slot without buffer data in set " +
                    std::to_string(setNumber_)
                );
            }

            return set;
        }
    };
}

