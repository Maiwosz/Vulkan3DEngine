#pragma once
#include "RenderGraphTemplate.h"
#include "RenderNodeTemplate.h"
#include <json.hpp>

/**
 * Serialization/deserialization utilities for RenderGraphTemplate.
 * Converts templates to/from JSON format for AssetLib storage.
 */
namespace RenderGraphSerialization {

    /**
     * Serialize AttachmentSlot to JSON
     */
    inline nlohmann::json SerializeAttachmentSlot(const AttachmentSlot& slot) {
        nlohmann::json j;
        j["type"] = static_cast<int>(slot.type);
        j["role"] = static_cast<int>(slot.role);
        j["name"] = slot.name;
        return j;
    }

    /**
     * Deserialize AttachmentSlot from JSON
     */
    inline AttachmentSlot DeserializeAttachmentSlot(const nlohmann::json& j) {
        AttachmentSlot::Type type = static_cast<AttachmentSlot::Type>(j["type"].get<int>());
        AttachmentSlot::Role role = static_cast<AttachmentSlot::Role>(j["role"].get<int>());
        std::string name = j["name"].get<std::string>();

        return AttachmentSlot(type, role, name);
    }

    /**
     * Serialize AttachmentSpec to JSON
     */
    inline nlohmann::json SerializeAttachmentSpec(const RenderNodeAttachmentSpec& spec) {
        nlohmann::json j;

        // Serialize inputs
        nlohmann::json inputs = nlohmann::json::array();
        for (const auto& input : spec.getInputs()) {
            inputs.push_back(SerializeAttachmentSlot(input));
        }
        j["inputs"] = inputs;

        // Serialize outputs
        nlohmann::json outputs = nlohmann::json::array();
        for (const auto& output : spec.getOutputs()) {
            outputs.push_back(SerializeAttachmentSlot(output));
        }
        j["outputs"] = outputs;

        return j;
    }

    /**
     * Deserialize AttachmentSpec from JSON
     */
    inline RenderNodeAttachmentSpec DeserializeAttachmentSpec(const nlohmann::json& j) {
        RenderNodeAttachmentSpec spec;

        // Deserialize inputs
        if (j.contains("inputs")) {
            for (const auto& input : j["inputs"]) {
                auto slot = DeserializeAttachmentSlot(input);
                spec.addInput(slot.role, slot.name);
            }
        }

        // Deserialize outputs
        if (j.contains("outputs")) {
            for (const auto& output : j["outputs"]) {
                auto slot = DeserializeAttachmentSlot(output);
                spec.addOutput(slot.role, slot.name);
            }
        }

        return spec;
    }

    /**
     * Serialize RenderNodeTemplate to JSON
     */
    inline nlohmann::json SerializeNodeTemplate(const RenderNodeTemplate& nodeTemplate) {
        nlohmann::json j;
        j["name"] = nodeTemplate.getName();
        j["attachmentSpec"] = SerializeAttachmentSpec(nodeTemplate.getAttachmentSpec());
        return j;
    }

    /**
     * Deserialize RenderNodeTemplate from JSON
     */
    inline std::unique_ptr<RenderNodeTemplate> DeserializeNodeTemplate(const nlohmann::json& j) {
        std::string name = j["name"].get<std::string>();
        auto nodeTemplate = std::make_unique<RenderNodeTemplate>(name);

        // Deserialize attachment spec
        if (j.contains("attachmentSpec")) {
            RenderNodeAttachmentSpec spec = DeserializeAttachmentSpec(j["attachmentSpec"]);

            // Apply inputs
            for (const auto& input : spec.getInputs()) {
                nodeTemplate->addInputAttachment(input.role, input.name);
            }

            // Apply outputs
            for (const auto& output : spec.getOutputs()) {
                nodeTemplate->addOutputAttachment(output.role, output.name);
            }
        }

        return nodeTemplate;
    }

    /**
     * Serialize NodeConnection to JSON
     */
    inline nlohmann::json SerializeConnection(const NodeConnection& connection) {
        nlohmann::json j;
        j["sourceNode"] = connection.sourceNodeIndex;
        j["sourceOutput"] = connection.sourceOutputIndex;
        j["targetNode"] = connection.targetNodeIndex;
        j["targetInput"] = connection.targetInputIndex;
        return j;
    }

    /**
     * Deserialize NodeConnection from JSON
     */
    inline NodeConnection DeserializeConnection(const nlohmann::json& j) {
        return NodeConnection(
            j["sourceNode"].get<uint32_t>(),
            j["sourceOutput"].get<uint32_t>(),
            j["targetNode"].get<uint32_t>(),
            j["targetInput"].get<uint32_t>()
        );
    }

    /**
     * Serialize complete RenderGraphTemplate to JSON
     */
    inline nlohmann::json SerializeGraphTemplate(const RenderGraphTemplate& graphTemplate) {
        nlohmann::json j;

        // Basic info
        j["name"] = graphTemplate.getName();

        // Serialize nodes
        nlohmann::json nodes = nlohmann::json::array();
        for (const auto& nodeTemplate : graphTemplate.getNodeTemplates()) {
            nodes.push_back(SerializeNodeTemplate(*nodeTemplate));
        }
        j["nodes"] = nodes;

        // Serialize connections
        nlohmann::json connections = nlohmann::json::array();
        for (const auto& connection : graphTemplate.getConnections()) {
            connections.push_back(SerializeConnection(connection));
        }
        j["connections"] = connections;

        return j;
    }

    /**
     * Deserialize complete RenderGraphTemplate from JSON
     */
    inline std::unique_ptr<RenderGraphTemplate> DeserializeGraphTemplate(const nlohmann::json& j) {
        std::string name = j["name"].get<std::string>();
        auto graphTemplate = std::make_unique<RenderGraphTemplate>(name);

        // Deserialize nodes
        if (j.contains("nodes")) {
            for (const auto& nodeJson : j["nodes"]) {
                auto nodeTemplate = DeserializeNodeTemplate(nodeJson);
                graphTemplate->addNode(std::move(nodeTemplate));
            }
        }

        // Deserialize connections
        if (j.contains("connections")) {
            for (const auto& connJson : j["connections"]) {
                NodeConnection conn = DeserializeConnection(connJson);
                graphTemplate->connect(
                    conn.sourceNodeIndex, conn.sourceOutputIndex,
                    conn.targetNodeIndex, conn.targetInputIndex
                );
            }
        }

        return graphTemplate;
    }

} // namespace RenderGraphSerialization