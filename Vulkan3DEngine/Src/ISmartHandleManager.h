#pragma once
#include <functional>
#include "IResourceManager.h"

// Forward declarations
template<typename HandleType, typename ResourceType>
class ISmartHandleManager;

template<typename HandleType, typename ResourceType>
class SmartHandle {
public:
    // Default constructor - invalid handle
    SmartHandle() : m_handle(), m_manager(nullptr) {}

    // Copy constructor
    SmartHandle(const SmartHandle& other) : m_handle(other.m_handle), m_manager(other.m_manager) {
        if (m_manager && m_handle.isValid()) {
            addReference();
        }
    }

    // Move constructor
    SmartHandle(SmartHandle&& other) noexcept
        : m_handle(other.m_handle), m_manager(other.m_manager) {
        other.m_handle = HandleType{}; // Reset to invalid
        other.m_manager = nullptr;
    }

    // Copy assignment
    SmartHandle& operator=(const SmartHandle& other) {
        if (this != &other) {
            reset();
            m_handle = other.m_handle;
            m_manager = other.m_manager;
            if (m_manager && m_handle.isValid()) {
                addReference();
            }
        }
        return *this;
    }

    // Move assignment
    SmartHandle& operator=(SmartHandle&& other) noexcept {
        if (this != &other) {
            reset();
            m_handle = other.m_handle;
            m_manager = other.m_manager;
            other.m_handle = HandleType{};
            other.m_manager = nullptr;
        }
        return *this;
    }

    // Destructor
    ~SmartHandle() {
        reset();
    }

    // Resource access
    ResourceType* get() const;
    ResourceType* operator->() const { return get(); }
    ResourceType& operator*() const { return *get(); }

    // Handle access
    HandleType handle() const { return m_handle; }

    // Validity check
    bool isValid() const;
    explicit operator bool() const { return isValid(); }

    // Comparison operators
    bool operator==(const SmartHandle& other) const {
        return m_handle == other.m_handle;
    }

    bool operator!=(const SmartHandle& other) const {
        return !(*this == other);
    }

    // Reset to invalid state
    void reset();

private:
    // Constructor for manager use only
    SmartHandle(HandleType handle, ISmartHandleManager<HandleType, ResourceType>* manager)
        : m_handle(handle), m_manager(manager) {
    }

    void addReference();
    void removeReference();

    friend class ISmartHandleManager<HandleType, ResourceType>;

    HandleType m_handle;
    ISmartHandleManager<HandleType, ResourceType>* m_manager;
};

// Rozszerzony interfejs dla managerów obsługujących smart handle'y
// 
// FILOZOFIA DESIGNU:
// - IResourceManager implementuje podstawowy system referencji, ale wymaga RĘCZNEGO zarządzania
//   przez wywołanie addReference()/removeReference()
// - ISmartHandleManager rozszerza IResourceManager o AUTOMATYCZNE zarządzanie referencjami
//   poprzez SmartHandle, które same wywołują addReference() w konstruktorze i removeReference()
//   w destruktorze
// - Pozwala to na współistnienie zwykłych handles (ręczne zarządzanie) ze SmartHandles
//   (automatyczne zarządzanie) w tym samym managerze
// - Manager musi implementować PEŁNY interfejs IResourceManager - wszystkie metody są wymagane
template<typename HandleType, typename ResourceType>
class ISmartHandleManager : public IResourceManager<HandleType, ResourceType> {
public:
    virtual ~ISmartHandleManager() = default;

    // Factory method for creating smart handles from existing handles
    SmartHandle<HandleType, ResourceType> createSmartHandle(HandleType handle) {
        if (this->isValid(handle)) {
            this->addReference(handle);
            return SmartHandle<HandleType, ResourceType>(handle, this);
        }
        return SmartHandle<HandleType, ResourceType>(); // Invalid handle
    }

    // Factory method for creating smart handles with automatic resource acquisition
    // (implementacja zależna od konkretnego managera - np. acquireSmartBuffer w UniformBufferManager)

    // UWAGA: Metody addReference() i removeReference() są już wymagane przez IResourceManager
    // Smart handles będą ich używać automatycznie, podczas gdy zwykłe handles wymagają
    // ręcznego wywołania przez programistę

protected:
    // Protected factory method for derived classes to create smart handles
    // This allows derived managers to create smart handles using their own 'this' pointer
    SmartHandle<HandleType, ResourceType> makeSmartHandle(HandleType handle) {
        return SmartHandle<HandleType, ResourceType>(handle, this);
    }

private:
    friend class SmartHandle<HandleType, ResourceType>;
};

// Implementacje metod SmartHandle (muszą być po definicji ISmartHandleManager)
template<typename HandleType, typename ResourceType>
ResourceType* SmartHandle<HandleType, ResourceType>::get() const {
    return m_manager ? m_manager->getResource(m_handle) : nullptr;
}

template<typename HandleType, typename ResourceType>
bool SmartHandle<HandleType, ResourceType>::isValid() const {
    return m_manager && m_handle.isValid() && m_manager->isValid(m_handle);
}

template<typename HandleType, typename ResourceType>
void SmartHandle<HandleType, ResourceType>::reset() {
    if (m_manager && m_handle.isValid()) {
        removeReference();
    }
    m_handle = HandleType{};
    m_manager = nullptr;
}

template<typename HandleType, typename ResourceType>
void SmartHandle<HandleType, ResourceType>::addReference() {
    if (m_manager) {
        m_manager->addReference(m_handle);
    }
}

template<typename HandleType, typename ResourceType>
void SmartHandle<HandleType, ResourceType>::removeReference() {
    if (m_manager) {
        m_manager->removeReference(m_handle);
    }
}

// Hash specialization for smart handles
namespace std {
    template<typename HandleType, typename ResourceType>
    struct hash<SmartHandle<HandleType, ResourceType>> {
        size_t operator()(const SmartHandle<HandleType, ResourceType>& smartHandle) const {
            return hash<HandleType>()(smartHandle.handle());
        }
    };
}