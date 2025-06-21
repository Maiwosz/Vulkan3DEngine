#pragma once
#include "Component.h"
#include "AssetHandle.h"
#include "BinaryWriter.h"
#include "AssetLoader.h"
#include "Paths.h"
#include <filesystem>
#include <vector>

struct MeshComponent : public Component {
public:
    const char* getName() const override {
        return "MeshComponent";
    }

    void setMesh(AssetHandle mesh) {
        m_mesh = mesh;
        incrementVersion();
    }

    AssetHandle getMesh() {
        return m_mesh;
    }

    // ISerializable implementation
    json serialize() const override {
        json j;
        j["assetType"] = static_cast<int>(m_mesh.type);
        j["filename"] = m_mesh.filename;
        return j;
    }

    void deserialize(const json& j) override {
        if (j.contains("assetType") && j["assetType"].is_number_integer() &&
            j.contains("filename") && j["filename"].is_string()) {
            m_mesh.type = static_cast<AssetType>(j["assetType"]);
            m_mesh.filename = j["filename"];
            incrementVersion();
        }
    }

    void renderUI() override {
        ImGui::Text("Mesh Asset");

        // Display current mesh info
        if (!m_mesh.filename.empty()) {
            ImGui::Text("File: %s", m_mesh.filename.c_str());
            ImGui::Text("Type: %s", std::string(m_mesh.GetTypeName()).c_str());
        }
        else {
            ImGui::TextDisabled("No mesh assigned");
        }

        // Get available mesh files
        std::vector<std::string> meshFiles = getAvailableMeshFiles();

        if (meshFiles.empty()) {
            ImGui::TextDisabled("No mesh files found in assets directory");
        }
        else {
            // Find current selection index
            int currentSelection = -1;
            for (int i = 0; i < meshFiles.size(); i++) {
                if (meshFiles[i] == m_mesh.filename) {
                    currentSelection = i;
                    break;
                }
            }

            // Create combo items
            std::vector<const char*> items;
            items.push_back("None"); // First option for no selection
            for (const auto& file : meshFiles) {
                items.push_back(file.c_str());
            }

            int comboSelection = currentSelection + 1; // +1 because "None" is at index 0

            if (ImGui::Combo("Mesh File", &comboSelection, items.data(), items.size())) {
                if (comboSelection == 0) {
                    // "None" selected
                    setMesh(AssetHandle());
                }
                else {
                    // Mesh file selected
                    AssetHandle newHandle(AssetType::Mesh, meshFiles[comboSelection - 1]);
                    setMesh(newHandle);
                }
            }
        }

        // Manual input fallback
        ImGui::Separator();
        ImGui::Text("Manual Input:");

        char filenameBuffer[256];
        size_t copyLen = std::min(m_mesh.filename.length(), sizeof(filenameBuffer) - 1);
        m_mesh.filename.copy(filenameBuffer, copyLen);
        filenameBuffer[copyLen] = '\0';

        if (ImGui::InputText("Filename (without extension)", filenameBuffer, sizeof(filenameBuffer))) {
            AssetHandle newHandle(AssetType::Mesh, std::string(filenameBuffer));
            setMesh(newHandle);
        }
    }

private:
    AssetHandle m_mesh;

    std::vector<std::string> getAvailableMeshFiles() {
        std::vector<std::string> files;

        try {
            std::string meshDir = std::string(ASSETS_COMP) + AssetLoader::GetAssetSubdirectory(AssetType::Mesh);

            if (std::filesystem::exists(meshDir) && std::filesystem::is_directory(meshDir)) {
                for (const auto& entry : std::filesystem::directory_iterator(meshDir)) {
                    if (entry.is_regular_file()) {
                        std::string filename = entry.path().stem().string(); // Get filename without extension
                        files.push_back(filename);
                    }
                }
            }
        }
        catch (const std::filesystem::filesystem_error& e) {
            // Handle filesystem errors silently in UI context
        }

        std::sort(files.begin(), files.end());
        return files;
    }
};