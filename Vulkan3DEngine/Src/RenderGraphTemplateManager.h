#pragma once
#include "IAssetHandler.h"
#include "RenderGraphTemplate.h"
#include "RenderGraphSerialization.h"
#include "Handle.h"
#include <AssetLib.h>
#include <unordered_map>
#include <memory>
#include <string>

// Define handle type for render graph templates
DEFINE_HANDLE_TYPE(RenderGraphTemplateHandle, uint32_t)

/**
 * Asset handler for RenderGraphTemplate assets.
 * Manages loading, caching, and accessing render graph templates.
 * Provides built-in templates and supports loading from asset files.
 *
 * Note: RenderGraphTemplates are lightweight CPU-side data structures,
 * so they don't use VRAM and don't have dependencies on other assets.
 */
    class RenderGraphTemplateManager : public ISmartAssetHandler<RenderGraphTemplateHandle, RenderGraphTemplate> {
    public:
        RenderGraphTemplateManager();
        ~RenderGraphTemplateManager() override = default;

        // IAssetHandler interface implementation
        bool prepareAsset(const AssetHandle& handle, const AssetLib::AssetData& data, AssetManager& manager) override;
        void unloadAsset(const std::string& filename) override;
        bool isAssetReady(const std::string& filename) const override;

        uint64_t getAssetSize(const std::string& filename) const override;
        bool isInVram() const override { return false; } // Templates are CPU-side only

        std::vector<AssetDependency> getDependencies(const AssetHandle& handle, const AssetLib::AssetData& data) const override;

        // Resource retrieval (IAssetHandler interface)
        std::any getResourceInternal(const AssetHandle& handle) const override;
        std::any getHandleInternal(const std::string& filename) const override;

        // ISmartAssetHandler typed interface
        RenderGraphTemplate* getResource(RenderGraphTemplateHandle handle) const override;
        bool isAssetReady(RenderGraphTemplateHandle handle) const override;

        // Direct access methods by name (convenience methods)
        const RenderGraphTemplate* getTemplateByName(const std::string& name) const;
        RenderGraphTemplateHandle getHandleByName(const std::string& name) const;

        // Get smart handle by template name
        SmartAssetHandle<RenderGraphTemplateHandle, RenderGraphTemplate> getTemplateSmartHandle(const std::string& name) const;

        /**
         * Create a copy of the template for modification.
         * Useful when you want to programmatically modify a loaded template.
         */
        std::unique_ptr<RenderGraphTemplate> cloneTemplate(const std::string& name) const;
        std::unique_ptr<RenderGraphTemplate> cloneTemplate(RenderGraphTemplateHandle handle) const;

        // Check if a template with given name exists (file-based or built-in)
        bool hasTemplate(const std::string& name) const;

    private:
        struct CachedTemplate {
            std::unique_ptr<RenderGraphTemplate> template_;
            uint64_t memorySize;  // Approximate memory usage
            RenderGraphTemplateHandle handle;
            std::string name;     // Template name (for lookup)
            bool isBuiltIn;       // Whether this is a built-in template
        };

        // Storage maps
        mutable std::unordered_map<std::string, CachedTemplate> m_templatesByFilename;  // For file-based templates
        mutable std::unordered_map<std::string, RenderGraphTemplateHandle> m_templatesByName;  // Name -> handle lookup
        mutable std::unordered_map<RenderGraphTemplateHandle, CachedTemplate*> m_templatesByHandle;  // Handle -> template lookup

        // Handle generation
        RenderGraphTemplateHandle m_nextHandle{ 1 };

        // Initialize built-in templates
        void initializeBuiltInTemplates();

        // Register a template (used for both file-based and built-in)
        RenderGraphTemplateHandle registerTemplate(
            std::unique_ptr<RenderGraphTemplate> tmpl,
            const std::string& filename,
            bool isBuiltIn);

        // Helper to estimate memory usage of a template
        uint64_t estimateTemplateSize(const RenderGraphTemplate& tmpl) const;

        // Get cached template by handle
        const CachedTemplate* getCachedTemplate(RenderGraphTemplateHandle handle) const;
};