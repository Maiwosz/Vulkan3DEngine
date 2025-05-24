#pragma once

// Podstawowy interfejs dla managerów zasobów z systemem referencji
template<typename HandleType, typename ResourceType>
class IResourceManager {
public:
    virtual ~IResourceManager() = default;

    // Podstawowe operacje zarządzania zasobami
    virtual ResourceType* getResource(HandleType handle) = 0;
    virtual bool isValid(HandleType handle) const = 0;
    virtual void releaseResource(HandleType handle) = 0;

    // System referencji - wymagane ręczne zarządzanie
    virtual void addReference(HandleType handle) = 0;
    virtual void removeReference(HandleType handle) = 0;
};