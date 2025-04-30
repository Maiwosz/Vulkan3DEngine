#include "pch.h"
#include "Converter.h"

#ifdef _DEBUG
#pragma comment(lib, "shaderc_combinedd.lib")
#pragma comment(lib, "spirv-cross-cored.lib")
#pragma comment(lib, "spirv-cross-glsld.lib") 
#else
#pragma comment(lib, "shaderc_combined.lib")
#pragma comment(lib, "spirv-cross-core.lib")
#pragma comment(lib, "spirv-cross-glsl.lib") 
#endif
#pragma comment(lib, "AssetLib.lib")

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <json.hpp>
#include <lz4.h>
#include "bc7enc/bc7enc.h"

#include <fstream>
#include <unordered_map>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>


using json = nlohmann::json;

#pragma pack(push, 1)
struct Vertex {
    float pos[3];
    float norm[3];
    float uv[2];

    bool operator==(const Vertex& other) const {
        return memcmp(this, &other, sizeof(Vertex)) == 0;
    }
};
#pragma pack(pop)

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(const Vertex& v) const {
            size_t seed = 0;
            auto hash_combine = [](size_t& seed, auto val) {
                seed ^= hash<decltype(val)>()(val) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
                };

            for (int i = 0; i < 3; i++) hash_combine(seed, v.pos[i]);
            for (int i = 0; i < 3; i++) hash_combine(seed, v.norm[i]);
            for (int i = 0; i < 2; i++) hash_combine(seed, v.uv[i]);
            return seed;
        }
    };
}


void Converter::Convert(const std::string& inputPath,
    const std::string& outputPath,
    const Settings& settings)
{
    const std::string ext = inputPath.substr(inputPath.find_last_of(".") + 1);

	AssetData asset;

    if (ext == "png" || ext == "jpg" || ext == "tga") {
        asset = ProcessTexture(inputPath, settings);
    }
    else if (ext == "obj") {
        asset = ProcessMesh(inputPath, settings);
    }
    else if (ext == "mat") {
        asset = ProcessMaterial(inputPath, settings);
    }
    else if (ext == "glsl") {
        asset = ProcessShader(inputPath, settings);
    }
    else {
        throw std::runtime_error("Unsupported file type: " + ext);
    }

	AssetLib::WriteAsset(outputPath, asset);
}

AssetData Converter::ProcessTexture(const std::string& inputPath, const Settings& settings)
{
    struct StbiDeleter {
        void operator()(stbi_uc* p) const { stbi_image_free(p); }
    };

    // Ładowanie obrazu
    int width, height, channels;
    std::unique_ptr<stbi_uc, StbiDeleter> pixels(
        stbi_load(inputPath.c_str(), &width, &height, &channels, STBI_rgb_alpha),
        StbiDeleter{}
    );

    ValidateTextureDimensions(width, height);
    if (!pixels) {
        throw std::runtime_error("Failed to load texture: " + inputPath);
    }

    // Generowanie mipmap
    std::vector<std::vector<uint8_t>> mipLevels;
    std::vector<std::pair<uint32_t, uint32_t>> mipSizes;
    uint32_t currentWidth = width;
    uint32_t currentHeight = height;
    const uint8_t* currentPixels = pixels.get();

    while (true) {
        uint32_t paddedWidth = currentWidth;
        uint32_t paddedHeight = currentHeight;
        if (settings.textureFormat == AssetLib::TextureFormat::BC7) {
            paddedWidth = PadToMultipleOf4(paddedWidth);
            paddedHeight = PadToMultipleOf4(paddedHeight);
        }

        std::vector<uint8_t> resized(paddedWidth * paddedHeight * 4, 0);
        stbir_resize_uint8(
            currentPixels, currentWidth, currentHeight, 0,
            resized.data(), paddedWidth, paddedHeight, 0, 4
        );

        mipLevels.push_back(std::move(resized));
        mipSizes.emplace_back(paddedWidth, paddedHeight);

        if ((paddedWidth <= 1 && paddedHeight <= 1) || !settings.generateMipmaps) break;

        currentWidth = std::max(paddedWidth / 2, 1U);
        currentHeight = std::max(paddedHeight / 2, 1U);
        currentPixels = mipLevels.back().data();
    }

    // Przygotowanie danych tekstury i zapis przy użyciu AssetLib
    AssetLib::TextureInfo texInfo;
    texInfo.format = settings.textureFormat;
    texInfo.width = static_cast<uint32_t>(width);
    texInfo.height = static_cast<uint32_t>(height);
    texInfo.mipLevels = static_cast<uint32_t>(mipLevels.size());
    texInfo.flags = 0;

    std::vector<uint8_t> pixelData;

    if (settings.textureFormat == AssetLib::TextureFormat::BC7) {
        // Kompresja BC7 dla każdego poziomu mipmapy
        for (size_t i = 0; i < mipLevels.size(); ++i) {
            auto compressed = CompressBC7(
                mipLevels[i].data(),
                mipSizes[i].first,
                mipSizes[i].second
            );
            pixelData.insert(pixelData.end(), compressed.begin(), compressed.end());
        }
    }
    else {
        // Składanie surowych danych RGBA8
        for (const auto& mip : mipLevels) {
            pixelData.insert(pixelData.end(), mip.begin(), mip.end());
        }
    }

    std::string filename = std::filesystem::path(inputPath).filename().string();

    // Zapis przy użyciu ustandaryzowanej funkcji
    AssetData asset = AssetLib::WriteTexture(
        filename,
        texInfo,
        pixelData,
        AssetLib::CompressionType::LZ4,
        settings.compressionLevel
    );

    return asset;
}

AssetData Converter::ProcessMesh(const std::string& inputPath, const Settings& settings)
{
    try {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inputPath.c_str())) {
            throw std::runtime_error("OBJ load error: " + warn + err);
        }

        const bool hasNormals = !attrib.normals.empty();
        const bool hasTexCoords = !attrib.texcoords.empty();

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                if (index.vertex_index < 0) throw std::runtime_error("Missing vertex index");
                if (hasNormals && index.normal_index < 0) throw std::runtime_error("Missing normal index");
                if (hasTexCoords && index.texcoord_index < 0) throw std::runtime_error("Missing UV index");
            }
        }

        std::vector<DynamicVertex> vertices;
        std::vector<uint32_t> indices;
        std::unordered_map<DynamicVertex, uint32_t, DynamicVertexHasher> uniqueVertices;

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                DynamicVertex vertex{};
                vertex.hasNormals = hasNormals;
                vertex.hasTexCoords = hasTexCoords;

                const int vi = 3 * index.vertex_index;
                vertex.pos = { attrib.vertices[vi], attrib.vertices[vi + 1], attrib.vertices[vi + 2] };

                if (hasNormals) {
                    const int ni = 3 * index.normal_index;
                    vertex.norm = { attrib.normals[ni], attrib.normals[ni + 1], attrib.normals[ni + 2] };
                }

                if (hasTexCoords) {
                    const int ti = 2 * index.texcoord_index;
                    vertex.uv = { attrib.texcoords[ti], attrib.texcoords[ti + 1] };
                }

                auto it = uniqueVertices.find(vertex);
                if (it == uniqueVertices.end()) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(uniqueVertices[vertex]);
            }
        }

        const bool use16Bit = (vertices.size() - 1) <= std::numeric_limits<uint16_t>::max();
        uint8_t indexType = use16Bit ? 0 : 1;
        std::vector<uint8_t> indexBuffer;

        if (use16Bit) {
            std::vector<uint16_t> indices16(indices.begin(), indices.end());
            indexBuffer.resize(indices16.size() * sizeof(uint16_t));
            memcpy(indexBuffer.data(), indices16.data(), indexBuffer.size());
        }
        else {
            indexBuffer.resize(indices.size() * sizeof(uint32_t));
            memcpy(indexBuffer.data(), indices.data(), indexBuffer.size());
        }

        const size_t vertexSize = 3 * sizeof(float)
            + (hasNormals ? 3 * sizeof(float) : 0)
            + (hasTexCoords ? 2 * sizeof(float) : 0);

        std::vector<uint8_t> vertexData(vertices.size() * vertexSize);
        uint8_t* ptr = vertexData.data();

        for (const auto& v : vertices) {
            memcpy(ptr, v.pos.data(), 3 * sizeof(float));
            ptr += 3 * sizeof(float);

            if (hasNormals) {
                memcpy(ptr, v.norm.data(), 3 * sizeof(float));
                ptr += 3 * sizeof(float);
            }

            if (hasTexCoords) {
                memcpy(ptr, v.uv.data(), 2 * sizeof(float));
                ptr += 2 * sizeof(float);
            }
        }

        std::vector<uint8_t> meshData;
        meshData.reserve(vertexData.size() + indexBuffer.size());
        meshData.insert(meshData.end(), vertexData.begin(), vertexData.end());
        meshData.insert(meshData.end(), indexBuffer.begin(), indexBuffer.end());

        AssetLib::MeshInfo meshInfo{};
        meshInfo.vertexCount = static_cast<uint32_t>(vertices.size());
        meshInfo.indexCount = static_cast<uint32_t>(indices.size());
        meshInfo.vertexStride = static_cast<uint32_t>(vertexSize);
        meshInfo.attributes = static_cast<uint32_t>(
            AssetLib::VertexAttribute::Position |
            (hasNormals ? AssetLib::VertexAttribute::Normal : AssetLib::VertexAttribute(0)) |
            (hasTexCoords ? AssetLib::VertexAttribute::TexCoord : AssetLib::VertexAttribute(0))
            );
        meshInfo.indexType = indexType;

        std::string filename = std::filesystem::path(inputPath).filename().string();

        // Zapis przy użyciu ustandaryzowanej funkcji
        AssetData asset = AssetLib::WriteMesh(
            filename,
            meshInfo,
            vertexData,
			indexBuffer,
            AssetLib::CompressionType::LZ4,
            settings.compressionLevel
        );

        return asset;
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Mesh processing failed: " + std::string(e.what()));
    }
}

AssetData Converter::ProcessMaterial(const std::string& inputPath, const Settings& settings)
{
    // Wczytaj plik materiału
    std::ifstream file(inputPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open material file: " + inputPath);
    }

    json materialJson;
    file >> materialJson;

    // Walidacja struktury
    if (!materialJson.contains("shader") || !materialJson.contains("parameters")) {
        throw std::runtime_error("Invalid material structure");
    }

    // Funkcje pomocnicze do parsowania
    auto ParseFilter = [](const std::string& str) {
        if (str == "Nearest") return AssetLib::SamplerFilter::Nearest;
        if (str == "Linear") return AssetLib::SamplerFilter::Linear;
        throw std::runtime_error("Invalid sampler filter: " + str);
        };

    auto ParseAddressMode = [](const std::string& str) {
        if (str == "Repeat") return AssetLib::SamplerAddressMode::Repeat;
        if (str == "MirroredRepeat") return AssetLib::SamplerAddressMode::MirroredRepeat;
        if (str == "ClampToEdge") return AssetLib::SamplerAddressMode::ClampToEdge;
        if (str == "ClampToBorder") return AssetLib::SamplerAddressMode::ClampToBorder;
        throw std::runtime_error("Invalid address mode: " + str);
        };

    // Przygotuj struktury danych
    AssetLib::MaterialInfo matInfo{};
    std::vector<AssetLib::MaterialParameter> params;
    std::vector<uint8_t> paramData;

    // Wczytaj nazwę shadera
    const std::string shaderName = materialJson["shader"].get<std::string>();
    strncpy_s(matInfo.shaderName.data(), matInfo.shaderName.size(), shaderName.c_str(), _TRUNCATE);

    // Przetwarzaj parametry
    for (auto& [key, value] : materialJson["parameters"].items()) {
        AssetLib::MaterialParameter param{};
        strncpy_s(param.name.data(), param.name.size(), key.c_str(), _TRUNCATE);
        param.arraySize = 1;

        if (value.contains("path")) { // Tylko nowy format
            param.type = AssetLib::MaterialParameter::Type::Texture;

            // Parsuj ścieżkę tekstury
            const std::string texPath = value["path"].get<std::string>();
            param.dataOffset = static_cast<uint32_t>(paramData.size());
            paramData.insert(paramData.end(), texPath.begin(), texPath.end());
            paramData.push_back('\0');

            // Parsuj sampler
            if (value.contains("sampler")) {
                auto& s = value["sampler"];
                param.samplerDesc.magFilter = ParseFilter(s.value("magFilter", "Linear"));
                param.samplerDesc.minFilter = ParseFilter(s.value("minFilter", "Linear"));
                param.samplerDesc.addressModeU = ParseAddressMode(s.value("addressModeU", "Repeat"));
                param.samplerDesc.addressModeV = ParseAddressMode(s.value("addressModeV", "Repeat"));
                param.samplerDesc.addressModeW = ParseAddressMode(s.value("addressModeW", "Repeat"));
                param.samplerDesc.anisotropy = s.value("anisotropy", 1.0f);
                param.samplerDesc.minLod = s.value("minLod", 0.0f);
                param.samplerDesc.maxLod = s.value("maxLod", 16.0f);
            }
            else {
                // Domyślny sampler jeśli nie podano
                param.samplerDesc = {
                    AssetLib::SamplerFilter::Linear,
                    AssetLib::SamplerFilter::Linear,
                    AssetLib::SamplerAddressMode::Repeat,
                    AssetLib::SamplerAddressMode::Repeat,
                    AssetLib::SamplerAddressMode::Repeat,
                    1.0f,
                    0.0f,
                    16.0f
                };
            }
        }
        else if (value.is_number()) {
            param.type = AssetLib::MaterialParameter::Type::Float;
            const float fValue = value.get<float>();
            param.dataOffset = static_cast<uint32_t>(paramData.size());
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&fValue);
            paramData.insert(paramData.end(), bytes, bytes + sizeof(float));
        }
        else if (value.is_boolean()) {
            param.type = AssetLib::MaterialParameter::Type::Bool;
            const bool bValue = value.get<bool>();
            param.dataOffset = static_cast<uint32_t>(paramData.size());
            paramData.push_back(bValue ? 1 : 0);
        }
        else if (value.is_array()) {
            const auto arr = value.get<std::vector<float>>();
            switch (arr.size()) {
            case 1: param.type = AssetLib::MaterialParameter::Type::Float; break;
            case 2: param.type = AssetLib::MaterialParameter::Type::Float2; break;
            case 3: param.type = AssetLib::MaterialParameter::Type::Float3; break;
            case 4: param.type = AssetLib::MaterialParameter::Type::Float4; break;
            default: throw std::runtime_error("Unsupported array size for parameter: " + key);
            }
            param.dataOffset = static_cast<uint32_t>(paramData.size());
            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(arr.data());
            paramData.insert(paramData.end(), bytes, bytes + arr.size() * sizeof(float));
        }
        else {
            throw std::runtime_error("Unsupported parameter type for: " + key);
        }

        params.push_back(param);
    }

    // Ustaw brakujące pola w MaterialInfo
    matInfo.parameterCount = static_cast<uint32_t>(params.size());
    matInfo.dataSize = static_cast<uint32_t>(paramData.size());
    std::string filename = std::filesystem::path(inputPath).filename().string();

    // Utwórz dane zasobu przy użyciu nowych funkcji AssetLib
    AssetLib::AssetData assetData = AssetLib::WriteMaterial(
        filename,
        matInfo,
        params,
        paramData,
        AssetLib::CompressionType::None,
        settings.compressionLevel
    );

	return assetData;
}

AssetData Converter::ProcessShader(const std::string& inputPath, const Settings& settings) {
    std::ifstream file(inputPath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + inputPath);
    }
    std::string source((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    auto stagesSources = ParseShaderStages(source);
    if (stagesSources.empty()) {
        throw std::runtime_error("No valid stages found in shader file");
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    options.SetOptimizationLevel(shaderc_optimization_level_performance);

    std::vector<AssetLib::ShaderStageInfo> stagesInfo;

    for (const auto& stageSource : stagesSources) {
        shaderc_shader_kind kind;
        switch (stageSource.stage) {
        case ShaderStage::Vertex: kind = shaderc_glsl_vertex_shader; break;
        case ShaderStage::Fragment: kind = shaderc_glsl_fragment_shader; break;
        case ShaderStage::Compute: kind = shaderc_glsl_compute_shader; break;
        case ShaderStage::Geometry: kind = shaderc_glsl_geometry_shader; break;
        case ShaderStage::TessellationControl: kind = shaderc_glsl_tess_control_shader; break;
        case ShaderStage::TessellationEvaluation: kind = shaderc_glsl_tess_evaluation_shader; break;
        default: throw std::runtime_error("Unsupported shader stage");
        }

        shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
            stageSource.code, kind, inputPath.c_str(), options);

        if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
            throw std::runtime_error("Shader compilation failed: " + module.GetErrorMessage());
        }

        std::vector<uint32_t> spirv(module.cbegin(), module.cend());
        std::vector<uint8_t> spirvBytes(spirv.size() * sizeof(uint32_t));
        memcpy(spirvBytes.data(), spirv.data(), spirvBytes.size());

        std::vector<DescriptorBinding> bindings;
        std::vector<PushConstantRange> pushConstants;
        ProcessShaderReflection(spirv, bindings, pushConstants, stageSource.stage);

        // Serializacja refleksji
        ShaderReflection refl;
        refl.descriptorSetCount = 0;
        if (!bindings.empty()) {
            uint32_t maxSet = 0;
            for (const auto& b : bindings) if (b.set > maxSet) maxSet = b.set;
            refl.descriptorSetCount = maxSet + 1;
        }
        refl.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());
        refl.descriptorBindingsCount = static_cast<uint32_t>(bindings.size());

        std::vector<uint8_t> reflectionData;
        reflectionData.resize(sizeof(refl) +
            (bindings.size() * sizeof(DescriptorBinding)) +
            (pushConstants.size() * sizeof(PushConstantRange)));

        size_t offset = 0;
        memcpy(reflectionData.data() + offset, &refl, sizeof(refl));
        offset += sizeof(refl);

        for (const auto& b : bindings) {
            memcpy(reflectionData.data() + offset, &b, sizeof(b));
            offset += sizeof(b);
        }

        for (const auto& pc : pushConstants) {
            memcpy(reflectionData.data() + offset, &pc, sizeof(pc));
            offset += sizeof(pc);
        }

        // Przygotuj informacje o etapie
        AssetLib::ShaderStageInfo stageInfo;
        stageInfo.stage = stageSource.stage;
        if (!spirv.empty()) {
            uint32_t version = spirv[0];
            stageInfo.spirvVersion = (version >> 16) & 0xFF | (version & 0xFF) << 8;
        }
        else {
            stageInfo.spirvVersion = 0;
        }
        stageInfo.spirvCode = std::move(spirvBytes);
        stageInfo.reflectionData = std::move(reflectionData);

        stagesInfo.push_back(std::move(stageInfo));
    }

    std::string filename = std::filesystem::path(inputPath).filename().string();

    return AssetLib::WriteShader(
        filename,
        stagesInfo,
        0, // flags
        AssetLib::CompressionType::LZ4,
        settings.compressionLevel
    );
}

std::vector<Converter::ShaderStageSource> Converter::ParseShaderStages(const std::string& source) {
    std::vector<ShaderStageSource> stages;
    std::istringstream stream(source);
    std::string line;
    std::string header;
    ShaderStageSource currentStage;
    bool inStage = false;
    bool headerProcessed = false;

    auto parseStage = [](const std::string& stageStr) {
        if (stageStr == "vertex") return ShaderStage::Vertex;
        if (stageStr == "fragment") return ShaderStage::Fragment;
        if (stageStr == "compute") return ShaderStage::Compute;
        if (stageStr == "geometry") return ShaderStage::Geometry;
        if (stageStr == "tesscontrol") return ShaderStage::TessellationControl;
        if (stageStr == "tesseval") return ShaderStage::TessellationEvaluation;
        throw std::runtime_error("Unknown shader stage: " + stageStr);
        };

    while (std::getline(stream, line)) {
        if (line.find("#pragma stage") != std::string::npos) {
            // Save the header before processing the first stage
            if (!headerProcessed) {
                headerProcessed = true;
            }

            if (inStage) {
                stages.push_back(currentStage);
                currentStage.code.clear();
            }

            size_t start = line.find_last_of(' ') + 1;
            std::string stageStr = line.substr(start);
            currentStage.stage = parseStage(stageStr);
            inStage = true;
        }
        else {
            if (!headerProcessed) {
                header += line + "\n";
            }
            else if (inStage) {
                currentStage.code += line + "\n";
            }
        }
    }

    if (inStage) {
        stages.push_back(currentStage);
    }

    // Prepend the header to each stage's code
    for (auto& stage : stages) {
        stage.code = header + stage.code;
    }

    return stages;
}

void Converter::ProcessShaderReflection(
    const std::vector<uint32_t>& spirv,
    std::vector<AssetLib::DescriptorBinding>& bindings,
    std::vector<AssetLib::PushConstantRange>& pushConstants,
    AssetLib::ShaderStage stage)
{
    using namespace AssetLib;
    try {
        spirv_cross::Compiler compiler(spirv);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();

        ShaderStageFlags stageFlags = static_cast<ShaderStageFlags>(1 << static_cast<uint8_t>(stage));
        std::unordered_set<uint32_t> descriptorSets;

        auto process_binding = [&](const spirv_cross::Resource& res, DescriptorType type) {
            DescriptorBinding binding{};
            binding.set = compiler.get_decoration(res.id, spv::DecorationDescriptorSet);
            binding.binding = compiler.get_decoration(res.id, spv::DecorationBinding);
            binding.type = type;
            binding.stageFlags = stageFlags;
            descriptorSets.insert(binding.set);

            const auto& base_type = compiler.get_type(res.base_type_id);

            // Obsługa rozmiaru dla buforów
            if (type == DescriptorType::UniformBuffer || type == DescriptorType::StorageBuffer) {
                binding.size = static_cast<uint32_t>(compiler.get_declared_struct_size(base_type));
            }

            // Nazwa zasobu
            std::string name = compiler.get_name(res.id);
            if (name.empty()) {
                const char* typeName = "";
                switch (type) {
                case DescriptorType::UniformBuffer: typeName = "UniformBuffer"; break;
                case DescriptorType::CombinedImageSampler: typeName = "Texture"; break;
                case DescriptorType::StorageBuffer: typeName = "StorageBuffer"; break;
                default: typeName = "Resource";
                }
                name = typeName + std::string("_Set") + std::to_string(binding.set) +
                    "_Binding" + std::to_string(binding.binding);
            }
            strncpy_s(binding.name.data(), binding.name.size(), name.c_str(), _TRUNCATE);

            bindings.push_back(binding);
            };

        // Przetwarzaj uniform buffory
        for (auto& ub : resources.uniform_buffers) {
            process_binding(ub, DescriptorType::UniformBuffer);
        }

        // Przetwarzaj storage buffory
        for (auto& sb : resources.storage_buffers) {
            process_binding(sb, DescriptorType::StorageBuffer);
        }

        // Przetwarzaj tekstury
        for (auto& tex : resources.sampled_images) {
            process_binding(tex, DescriptorType::CombinedImageSampler);
        }

        // Przetwarzaj storage images
        for (auto& img : resources.storage_images) {
            process_binding(img, DescriptorType::StorageImage);
        }

        // Push constants
        for (auto& pc : resources.push_constant_buffers) {
            PushConstantRange range{};
            range.stageFlags = stageFlags;

            const auto& type = compiler.get_type(pc.base_type_id);
            range.size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));

            // Pobierz offset jeśli istnieje
            auto& decorations = compiler.get_decoration_bitset(pc.id);
            if (decorations.get(spv::DecorationOffset)) {
                range.offset = compiler.get_decoration(pc.id, spv::DecorationOffset);
            }

            pushConstants.push_back(range);
        }

        // Logika dla descriptorSetCount (przekazywana przez referencję)
        if (!bindings.empty()) {
            uint32_t maxSet = *std::max_element(descriptorSets.begin(), descriptorSets.end());
            // Przekaż wynik przez referencję lub odpowiednią strukturę
        }
    }
    catch (const std::exception& e) {
        throw std::runtime_error("Shader reflection failed: " + std::string(e.what()));
    }
}

std::vector<uint8_t> Converter::CompressBC7(const uint8_t* rgba, uint32_t width, uint32_t height) {
    if (width % 4 != 0 || height % 4 != 0) {
        throw std::runtime_error("BC7 requires dimensions divisible by 4");
    }

    bc7enc_compress_block_params params;
    bc7enc_compress_block_params_init(&params);
    params.m_uber_level = 3; // Zmień na niższy poziom (np. 1) dla testów

    const uint32_t blocksX = (width + 3) / 4;
    const uint32_t blocksY = (height + 3) / 4;
    std::vector<uint8_t> output(blocksX * blocksY * 16);

    for (uint32_t y = 0; y < blocksY; ++y) {
        for (uint32_t x = 0; x < blocksX; ++x) {
            uint8_t block[4][4][4] = { 0 };

            // Poprawna ekstrakcja z uwzględnieniem paddingu
            for (uint32_t py = 0; py < 4; ++py) {
                const uint32_t srcY = y * 4 + py;
                if (srcY >= height) continue; // Pomijaj wiersze poza teksturą

                for (uint32_t px = 0; px < 4; ++px) {
                    const uint32_t srcX = x * 4 + px;
                    if (srcX >= width) continue; // Pomijaj kolumny poza teksturą

                    const size_t srcOffset = (srcY * width + srcX) * 4;
                    memcpy(block[py][px], &rgba[srcOffset], 4);
                }
            }

            bc7enc_compress_block(&output[(y * blocksX + x) * 16], block, &params);
        }
    }

    return output;
}

uint32_t Converter::PadToMultipleOf4(uint32_t value) {
    return (value + 3) & ~3;
}

void Converter::ValidateTextureDimensions(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument(
            "Invalid texture dimensions: " +
            std::to_string(width) + "x" + std::to_string(height)
        );
    }
}

