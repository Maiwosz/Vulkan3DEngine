#pragma once
#include "ShaderLib.h"

namespace ShaderLib
{
    class DescriptorSetBuilder {
    private:
        uint32_t setNumber_;
        std::vector<DescriptorSlot> slots_;
        std::unordered_map<std::string, std::shared_ptr<BufferObjectDefinition>> buffers_;

    public:
        DescriptorSetBuilder(uint32_t setNumber) : setNumber_(setNumber) {}

        // Add buffer with BufferObjectDefinition
        DescriptorSetBuilder& AddBuffer(
            uint32_t binding,
            std::shared_ptr<const BufferObjectDefinition> buffer,
            StageFlags stages) {

            DescriptorType type = buffer->GetBufferType() == BufferType::Uniform
                ? DescriptorType::UniformBuffer
                : DescriptorType::StorageBuffer;

            std::string name = buffer->GetName();

            // Add slot
            slots_.push_back({
                binding,
                type,
                stages,
                name
                });

            // Store buffer definition (need to cast away const for storage)
            buffers_[name] = std::const_pointer_cast<BufferObjectDefinition>(buffer);

            return *this;
        }

        // Add sampler/image (no buffer data needed)
        DescriptorSetBuilder& AddSampler(
            uint32_t binding,
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
        DescriptorSetBuilder& AddDescriptor(
            uint32_t binding,
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
