#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>

/**
 * Attachment slot descriptor for dynamic template building.
 */
struct AttachmentSlot {
    enum class Type {
        Input,      // Read-only input from previous passes
        Output,     // Render target (color or depth)
        Transient   // Temporary attachment used within this pass
    };

    enum class Role {
        Color,           // Color attachment
        Depth,           // Depth attachment
        DepthStencil,    // Combined depth-stencil
        Stencil,         // Stencil only
        Resolve          // MSAA resolve target
    };

    Type type;
    Role role;
    std::string name;  // For debugging

    AttachmentSlot(Type t, Role r, std::string n = "")
        : type(t), role(r), name(std::move(n)) {
    }
};

/**
 * Dynamically buildable attachment specification.
 */
class RenderNodeAttachmentSpec {
public:
    RenderNodeAttachmentSpec() = default;

    void addInput(AttachmentSlot::Role role, const std::string& name = "") {
        inputs.emplace_back(AttachmentSlot::Type::Input, role, name);
    }

    void addOutput(AttachmentSlot::Role role, const std::string& name = "") {
        outputs.emplace_back(AttachmentSlot::Type::Output, role, name);
    }

    const std::vector<AttachmentSlot>& getInputs() const { return inputs; }
    const std::vector<AttachmentSlot>& getOutputs() const { return outputs; }

    uint32_t getInputCount() const { return static_cast<uint32_t>(inputs.size()); }
    uint32_t getOutputCount() const { return static_cast<uint32_t>(outputs.size()); }
    uint32_t getTotalAttachmentCount() const { return getInputCount() + getOutputCount(); }

private:
    std::vector<AttachmentSlot> inputs;
    std::vector<AttachmentSlot> outputs;
};

/**
 * Dynamically built render node template.
 * Can be constructed programmatically or loaded from a file.
 */
class RenderNodeTemplate {
public:
    RenderNodeTemplate() = default;
    explicit RenderNodeTemplate(std::string name) : m_name(std::move(name)) {}

    // Builder-style methods
    RenderNodeTemplate& setName(std::string name) {
        m_name = std::move(name);
        return *this;
    }

    RenderNodeTemplate& addInputAttachment(AttachmentSlot::Role role, const std::string& name = "") {
        m_attachmentSpec.addInput(role, name);
        return *this;
    }

    RenderNodeTemplate& addOutputAttachment(AttachmentSlot::Role role, const std::string& name = "") {
        m_attachmentSpec.addOutput(role, name);
        return *this;
    }

    // Accessors
    const RenderNodeAttachmentSpec& getAttachmentSpec() const { return m_attachmentSpec; }
    const std::string& getName() const { return m_name; }

    // Validation
    bool isValid() const {
        return !m_name.empty() && m_attachmentSpec.getTotalAttachmentCount() > 0;
    }

private:
    std::string m_name;
    RenderNodeAttachmentSpec m_attachmentSpec;
};