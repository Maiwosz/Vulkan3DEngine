#pragma once
#include <tuple>
#include <type_traits>

class SystemManager;
class Registry;

template<typename... Dependencies>
class SystemContext;

// Bazowy interfejs dla wszystkich systemów
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void update(class SystemManager& systemManager, Registry& registry) = 0;
};

// Szablonowa klasa bazowa dla systemów
template<typename... Dependencies>
class System : public ISystem {
public:
    using ContextType = SystemContext<Dependencies...>;

    virtual void update(ContextType& context) = 0;

    void update(SystemManager& systemManager, Registry& registry) override {
        ContextType context(systemManager, registry);
        update(context);
    }
};

template<typename... Dependencies>
class SystemContext {
public:
    SystemContext(SystemManager& systemManager, Registry& registry)
        : m_systemManager(systemManager), m_registry(registry) {
    }

    template<typename T>
    T& getSystem() {
        static_assert((std::is_same_v<T, Dependencies> || ...),
            "System dependency not declared");
        return m_systemManager.template getSystem<T>();
    }

    Registry& getRegistry() { return m_registry; }

private:
    SystemManager& m_systemManager;
    Registry& m_registry;
};