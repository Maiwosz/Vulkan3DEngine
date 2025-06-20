#pragma once
#include <type_traits>

class SystemManager;
class Registry;
class Engine;

// Bazowy interfejs dla wszystkich systemów
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void initialize(SystemManager& systemManager, Registry& registry, Engine& engine) = 0;
    virtual void update() = 0;
};

// Helper trait do sprawdzania czy typ jest w liście Dependencies
template<typename T, typename... Dependencies>
struct is_dependency : std::false_type {};

template<typename T, typename First, typename... Rest>
struct is_dependency<T, First, Rest...> : std::conditional_t<
    std::is_same_v<T, First>,
    std::true_type,
    is_dependency<T, Rest...>
> {
};

template<typename T, typename... Dependencies>
inline constexpr bool is_dependency_v = is_dependency<T, Dependencies...>::value;

// Szablonowa klasa bazowa dla systemów
template<typename... Dependencies>
class System : public ISystem {
public:
    System() = default;
    virtual ~System() = default;

    virtual void initialize() {}
    virtual void update() = 0;

    void initialize(SystemManager& systemManager, Registry& registry, Engine& engine) override final {
        m_systemManager = &systemManager;
        m_registry = &registry;
        m_engine = &engine;
        initialize();
    }

protected:
    // Dostęp do referencji - dostępne tylko dla klas pochodnych
    SystemManager& systemManager() { return *m_systemManager; }
    Registry& registry() { return *m_registry; }
    Engine& engine() { return *m_engine; }

    // Const wersje
    const SystemManager& systemManager() const { return *m_systemManager; }
    const Registry& registry() const { return *m_registry; }
    const Engine& engine() const { return *m_engine; }

    // Metoda pomocnicza do pobierania innych systemów
    template<typename T>
    T& getSystem() {
        static_assert(is_dependency_v<T, Dependencies...>,
            "System dependency not declared in template parameters");
        return m_systemManager->template getSystem<T>();
    }

    SystemManager* m_systemManager = nullptr;
    Registry* m_registry = nullptr;
    Engine* m_engine = nullptr;
};