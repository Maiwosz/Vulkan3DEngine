#include "pch.h"
#include "UBODefinitions.h"
#include "TypeConversions.h"

namespace ShaderLib {

    TypeInfo GetStructTypeInfo(const std::string& structName) {
        // Standard light structures we know about
        if (structName == "DirectionalLight") {
            return { sizeof(DirectionalLight), 16 };
        }
        else if (structName == "PointLight") {
            return { sizeof(PointLight), 16 };
        }
        else if (structName == "SpotLight") {
            return { sizeof(SpotLight), 16 };
        }

        // Default for unknown structs - assume 16-byte alignment
        return { 64, 16 }; // Default size of 64 bytes, can be adjusted as needed
    }

    // Build the UBO from the builder
    UniformBufferObject UBOBuilder::Build() {
        UniformBufferObject ubo;
        ubo.name = name;
        ubo.set = set;
        ubo.binding = binding;

        uint32_t currentOffset = 0;

        // Process each field
        for (const auto& field : fields) {
            UniformVariable var;
            var.name = field.name;
            var.type = field.type;
            var.typeName = field.typeName;
            var.arraySize = field.arraySize;

            // Handle different field types
            if (field.isStruct) {
                // Get struct type info
                TypeInfo structTypeInfo = GetStructTypeInfo(field.typeName);

                if (field.arraySize > 0) {
                    // Array of structs
                    var.type = UniformType::Array;

                    // In std140, array elements are aligned to at least 16 bytes
                    uint32_t elementSize = AlignTo(structTypeInfo.size, 16);
                    var.size = field.arraySize * elementSize;

                    // Arrays are aligned to 16 bytes or the base type's alignment, whichever is greater
                    uint32_t alignment = std::max(16u, structTypeInfo.alignment);
                    currentOffset = AlignTo(currentOffset, alignment);
                    var.offset = currentOffset;
                    currentOffset += var.size;
                }
                else {
                    // Single struct
                    var.type = UniformType::Struct;
                    var.size = structTypeInfo.size;

                    // Align according to struct alignment
                    currentOffset = AlignTo(currentOffset, structTypeInfo.alignment);
                    var.offset = currentOffset;
                    currentOffset += var.size;
                }
            }
            else {
                // For basic types
                TypeInfo typeInfo = TypeConversion::GetTypeInfo(var.type);

                if (field.arraySize > 0) {
                    // Array of basic types
                    // In std140, array elements are aligned to at least 16 bytes or the base type's alignment
                    uint32_t elementSize = AlignTo(typeInfo.size, std::max(16u, typeInfo.alignment));
                    var.size = field.arraySize * elementSize;

                    // Arrays are aligned to 16 bytes or the base type's alignment, whichever is greater
                    uint32_t alignment = std::max(16u, typeInfo.alignment);
                    currentOffset = AlignTo(currentOffset, alignment);
                    var.offset = currentOffset;
                    currentOffset += var.size;
                }
                else {
                    // Single basic type
                    var.size = typeInfo.size;

                    // Align according to the type's alignment requirements
                    currentOffset = AlignTo(currentOffset, typeInfo.alignment);
                    var.offset = currentOffset;
                    currentOffset += var.size;
                }
            }

            ubo.variables.push_back(var);
        }

        // Set the total size of the UBO
        ubo.size = AlignTo(currentOffset, 16); // Ensure the UBO size is a multiple of 16

        return ubo;
    }

    // Generate GLSL code for the UBO
    std::string UBOBuilder::GenerateGLSL() {
        std::stringstream ss;

        // Add struct definitions first
        for (const auto& structDef : structDefinitions) {
            ss << structDef;
        }

        // Generate the UBO definition
        ss << "layout(std140, set = " << set << ", binding = " << binding << ") uniform " << name << " {\n";

        for (const auto& field : fields) {
            ss << "    ";

            if (field.isStruct) {
                if (field.arraySize > 0) {
                    ss << field.typeName << " " << field.name << "[" << field.arraySize << "]";
                }
                else {
                    ss << field.typeName << " " << field.name;
                }
            }
            else {
                if (field.arraySize > 0) {
                    ss << TypeConversion::UniformTypeToString(field.type) << " " << field.name << "[" << field.arraySize << "]";
                }
                else {
                    ss << TypeConversion::UniformTypeToString(field.type) << " " << field.name;
                }
            }

            if (!field.comment.empty()) {
                ss << "; // " << field.comment;
            }
            else {
                ss << ";";
            }

            ss << "\n";
        }

        ss << "};\n\n";

        return ss.str();
    }

    // Register a UBO definition in the registry
    void UBORegistry::RegisterUBO(const UniformBufferObject& ubo) {
        ubos[ubo.name] = ubo;
    }

    // Get a registered UBO by name
    const UniformBufferObject* UBORegistry::GetUBO(const std::string& name) const {
        auto it = ubos.find(name);
        if (it != ubos.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // Generate GLSL code for a registered UBO
    std::string UBORegistry::GenerateGLSL(const std::string& uboName) const {
        const UniformBufferObject* ubo = GetUBO(uboName);
        if (!ubo) {
            return "// UBO '" + uboName + "' not found in registry\n";
        }

        // Generate struct definitions first
        std::stringstream ss;

        // Add standard light struct definitions if needed
        bool hasDirectionalLight = false;
        bool hasPointLight = false;
        bool hasSpotLight = false;

        // Check if we need to include struct definitions
        for (const auto& var : ubo->variables) {
            if (var.typeName == "DirectionalLight") hasDirectionalLight = true;
            else if (var.typeName == "PointLight") hasPointLight = true;
            else if (var.typeName == "SpotLight") hasSpotLight = true;
        }

        // Add the necessary struct definitions
        if (hasDirectionalLight) ss << DirectionalLight::GetGLSLDefinition();
        if (hasPointLight) ss << PointLight::GetGLSLDefinition();
        if (hasSpotLight) ss << SpotLight::GetGLSLDefinition();

        // Generate the UBO definition
        ss << "layout(std140, set = " << ubo->set << ", binding = " << ubo->binding << ") uniform " << ubo->name << " {\n";

        // Make sure this part is correct:
        for (const auto& var : ubo->variables) {
            ss << "    ";

            if (var.type == UniformType::Struct) {
                // Handle struct type
                ss << var.typeName << " " << var.name << ";";
            }
            else if (var.type == UniformType::Array && !var.typeName.empty()) {
                // Handle struct array
                ss << var.typeName << " " << var.name << "[" << var.arraySize << "];";
            }
            else if (var.type == UniformType::Array) {
                // Handle basic type array
                ss << TypeConversion::UniformTypeToString(UniformType::Float) << " " << var.name << "[" << var.arraySize << "];";
            }
            else {
                // Handle basic type
                ss << TypeConversion::UniformTypeToString(var.type) << " " << var.name << ";";
            }

            ss << "\n";
        }

        ss << "};\n\n";

        return ss.str();
    }

    // Initialize standard UBOs in the registry
    void UBORegistry::InitializeStandardUBOs() {
        RegisterUBO(CreateGlobalUBO());
        RegisterUBO(CreateObjectUBO());
    }

    // Create GlobalUBO definition
    UniformBufferObject UBORegistry::CreateGlobalUBO() {
        UBOBuilder builder("GlobalUBO", GLOBAL_DESCRIPTOR_SET, 0);

        // Add common global variables
        builder.AddField<glm::mat4>("view", "View matrix")
            .AddField<glm::mat4>("proj", "Projection matrix")
            .AddField<glm::vec3>("cameraPosition", "Camera position in world space")

            // Add directional light
            .AddStructField<DirectionalLight>("directionalLight", "Main directional light")

            // Add point lights array
            .AddArrayField<PointLight>("pointLights", 64, "Array of point lights")

            // Add spot lights array
            .AddArrayField<SpotLight>("spotLights", 16, "Array of spot lights")

            // Add light counters
            .AddField<int>("activePointLights", "Number of active point lights")
            .AddField<int>("activeSpotLights", "Number of active spot lights");

        return builder.Build();
    }

    // Create ObjectUBO definition
    UniformBufferObject UBORegistry::CreateObjectUBO() {
        UBOBuilder builder("ObjectUBO", OBJECT_DESCRIPTOR_SET, 0);

        // Add object-specific variables
        builder.AddField<glm::mat4>("model", "Model matrix")
            .AddField<glm::vec4>("color", "Object color/tint");

        return builder.Build();
    }
} // namespace ShaderLib