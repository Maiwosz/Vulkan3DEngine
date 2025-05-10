#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <atomic>
#include <unordered_map>

template <typename... Args>
class Event : public std::enable_shared_from_this<Event<Args...>> {
public:
    using Callback = std::function<void(Args...)>;
    using CallbackId = uint64_t;

    class Subscription {
    public:
        Subscription() = default;

        Subscription(std::shared_ptr<Event<Args...>> event, CallbackId id)
            : m_event(std::move(event)), m_id(id), m_valid(true) {
        }

        ~Subscription() {
            unsubscribe();
        }

        // Kopiowanie zabronione
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        // Przenoszenie dozwolone
        Subscription(Subscription&& other) noexcept
            : m_event(std::move(other.m_event)), m_id(other.m_id), m_valid(other.m_valid) {
            other.m_valid = false;
        }

        Subscription& operator=(Subscription&& other) noexcept {
            if (this != &other) {
                unsubscribe();
                m_event = std::move(other.m_event);
                m_id = other.m_id;
                m_valid = other.m_valid;
                other.m_valid = false;
            }
            return *this;
        }

        void unsubscribe() {
            if (m_valid && m_event) {
                m_event->unsubscribe(m_id);
                m_valid = false;
            }
        }

        bool isValid() const { return m_valid; }

    private:
        std::shared_ptr<Event<Args...>> m_event;
        CallbackId m_id = 0;
        bool m_valid = false;
    };

    Event() : m_isActive(true) {}
    ~Event() {
        // Mark event as inactive before destroying
        m_isActive.store(false);

        // Clear all callbacks under lock to ensure thread safety
        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.clear();
        m_owners.clear();
    }

    static std::shared_ptr<Event<Args...>> create() {
        return std::make_shared<Event<Args...>>();
    }

    [[nodiscard]] Subscription subscribe(Callback callback) {
        // Check if event is still active
        if (!m_isActive.load()) {
            return Subscription(); // Return invalid subscription
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        CallbackId id = m_nextId++;
        m_callbacks.push_back({ id, std::move(callback) });
        return Subscription(this->shared_from_this(), id);
    }

    void subscribeWithOwner(Callback callback, std::shared_ptr<void> owner) {
        // Check if event is still active
        if (!m_isActive.load()) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        CallbackId id = m_nextId++;
        m_callbacks.push_back({ id, std::move(callback) });
        m_owners[id] = std::move(owner);
    }

    void unsubscribe(CallbackId id) {
        // Check if event is still active
        if (!m_isActive.load()) {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
            [id](const auto& pair) { return pair.first == id; });
        if (it != m_callbacks.end()) {
            m_callbacks.erase(it);
        }

        // Usuń także z mapy właścicieli, jeśli istnieje
        m_owners.erase(id);
    }

    void invoke(Args... args) {
        // Quick check without lock first
        if (!m_isActive.load()) {
            return;
        }

        // Kopia callbacków, aby uniknąć blokowania mutexu podczas wywołań
        std::vector<std::pair<CallbackId, Callback>> callbacksCopy;

        {
            // Critical section - only take lock if event is active
            std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
            if (!lock.owns_lock() || !m_isActive.load()) {
                // If we couldn't acquire the lock or the event became inactive, just return
                return;
            }

            // Usuń callbacki, których właściciele zostali zniszczeni
            for (auto it = m_owners.begin(); it != m_owners.end();) {
                if (it->second.expired()) {
                    // Znajdź i usuń odpowiedni callback
                    auto cbIt = std::find_if(m_callbacks.begin(), m_callbacks.end(),
                        [id = it->first](const auto& pair) { return pair.first == id; });
                    if (cbIt != m_callbacks.end()) {
                        m_callbacks.erase(cbIt);
                    }
                    it = m_owners.erase(it);
                }
                else {
                    ++it;
                }
            }

            // Kopiuj aktualne callbacki
            callbacksCopy = m_callbacks;
        }

        // Wywołuj callbacki bez blokowania mutexu
        for (const auto& [id, callback] : callbacksCopy) {
            try {
                callback(args...);
            }
            catch (const std::exception& e) {
                if (m_exceptionHandler) {
                    m_exceptionHandler(std::current_exception());
                }
            }
        }
    }

    template <typename... Args>
    void safeInvokeEvent(std::shared_ptr<Event<Args...>> event, Args... args) {
        if (event && event->isActive()) {
            event->invoke(std::forward<Args>(args)...);
        }
    }

    void setExceptionHandler(std::function<void(std::exception_ptr)> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_exceptionHandler = std::move(handler);
    }

    void clear() {
        // Mark event as inactive before clearing
        m_isActive.store(false);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_callbacks.clear();
        m_owners.clear();

        // Reactivate event after clearing
        m_isActive.store(true);
    }

    size_t subscriberCount() const {
        if (!m_isActive.load()) {
            return 0;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        return m_callbacks.size();
    }

    bool isActive() const {
        return m_isActive.load();
    }

private:
    // Flag to track if this event is active
    std::atomic<bool> m_isActive;

    // Wektor par (id, callback) - użycie wektora zamiast listy dla lepszej wydajności cache
    std::vector<std::pair<CallbackId, Callback>> m_callbacks;

    // Mapa id -> właściciel
    std::unordered_map<CallbackId, std::weak_ptr<void>> m_owners;

    // Handler wyjątków
    std::function<void(std::exception_ptr)> m_exceptionHandler;

    // Mutex chroniący wewnętrzne struktury danych
    mutable std::mutex m_mutex;

    // Licznik dla przydzielania unikalnych identyfikatorów
    std::atomic<CallbackId> m_nextId{ 1 };
};