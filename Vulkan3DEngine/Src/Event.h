#pragma once
/**
 * @file Event.h
 * @brief Thread-safe implementacja wzorca obserwatora (observer pattern) w C++.
 *
 * Klasa Event umo¿liwia rejestrowanie, powiadamianie i zarz¹dzanie funkcjami zwrotnymi (callbacks),
 * które s¹ wywo³ywane przy wyst¹pieniu okreœlonego zdarzenia. Wspiera parametry dowolnego typu
 * poprzez szablon variadic.
 *
 * Podstawowe funkcjonalnoœci:
 * - Tworzenie zdarzeñ z dowolnymi parametrami za pomoc¹ szablonu
 * - Bezpieczne subskrybowanie i anulowanie subskrypcji
 * - Subskrypcje z powi¹zanym w³aœcicielem (automatyczne czyszczenie po zniszczeniu w³aœciciela)
 * - Obs³uga wyj¹tków w funkcjach zwrotnych
 * - Thread-safety dziêki mechanizmom synchronizacji
 *
 * Przyk³ad u¿ycia:
 *
 *   // Utworzenie zdarzenia z parametrami int i string
 *   auto event = Event<int, std::string>::create();
 *
 *   // Przypisanie funkcji obs³ugi wyj¹tków
 *   event->set_exception_handler([](std::exception_ptr ex) {
 *       try {
 *           if (ex) std::rethrow_exception(ex);
 *       } catch (const std::exception& e) {
 *           std::cerr << "Caught exception: " << e.what() << std::endl;
 *       }
 *   });
 *
 *   // Subskrypcja 1: Z rêcznym zarz¹dzaniem czasem ¿ycia
 *   auto subscription = event->subscribe([](int id, const std::string& msg) {
 *       std::cout << "ID: " << id << ", Message: " << msg << std::endl;
 *   });
 *
 *   // Subskrypcja 2: Z automatycznym zarz¹dzaniem przez powi¹zanego w³aœciciela
 *   auto owner = std::make_shared<int>(42);
 *   event->subscribeWithOwner([](int id, const std::string& msg) {
 *       std::cout << "Owner subscription: " << id << ", " << msg << std::endl;
 *   }, owner);
 *
 *   // Wywo³anie wszystkich zarejestrowanych funkcji
 *   event->invoke(1, "Hello Event");
 *
 *   // Anulowanie subskrypcji
 *   subscription.unsubscribe();
 *   // lub automatycznie przy zniszczeniu obiektu subscription
 */
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <exception>

template <typename... Args>
class Event : public std::enable_shared_from_this<Event<Args...>> {
public:
    using Callback = std::function<void(Args...)>;

    class Subscription {
    public:
        Subscription() = default;

        Subscription(std::weak_ptr<Event> event,
            typename std::list<std::pair<uint64_t, Callback>>::iterator iter,
            uint64_t id)
            : event_(std::move(event)), iter_(iter), id_(id), valid_(true) {
        }

        ~Subscription() {
            unsubscribe();
        }

        // Copying is disabled
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        // Move operations
        Subscription(Subscription&& other) noexcept
            : event_(std::move(other.event_)), iter_(other.iter_),
            id_(other.id_), valid_(other.valid_) {
            other.valid_ = false;
        }

        Subscription& operator=(Subscription&& other) noexcept {
            if (this != &other) {
                unsubscribe();
                event_ = std::move(other.event_);
                iter_ = other.iter_;
                id_ = other.id_;
                valid_ = other.valid_;
                other.valid_ = false;
            }
            return *this;
        }

        void unsubscribe() {
            if (valid_) {
                if (auto e = event_.lock()) {
                    std::lock_guard<std::mutex> lock(e->m_mutex);
                    e->remove(id_);
                }
                valid_ = false;
            }
        }

        bool isValid() const { return valid_; }

    private:
        std::weak_ptr<Event> event_;
        typename std::list<std::pair<uint64_t, Callback>>::iterator iter_;
        uint64_t id_;
        bool valid_ = false;
    };

    static std::shared_ptr<Event> create() {
        return std::shared_ptr<Event>(new Event());
    }

    [[nodiscard]] Subscription subscribe(Callback callback) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t id = next_id_++;
        m_callbacks.emplace_back(id, std::move(callback));
        return Subscription(this->weak_from_this(), --m_callbacks.end(), id);
    }

    void subscribeWithOwner(Callback callback, std::shared_ptr<void> owner) {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t id = next_id_++;
        m_callbacks.emplace_back(id, std::move(callback));
        m_owners[id] = std::weak_ptr<void>(owner);
    }

    void invoke(Args... args) {
        std::vector<std::pair<uint64_t, Callback>> local_copy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            // Remove expired owners
            auto ownerIt = m_owners.begin();
            while (ownerIt != m_owners.end()) {
                if (ownerIt->second.expired()) {
                    remove(ownerIt->first);
                    ownerIt = m_owners.erase(ownerIt);
                }
                else {
                    ++ownerIt;
                }
            }

            // Copy callbacks to avoid holding the lock during invocation
            local_copy.reserve(m_callbacks.size());
            for (const auto& cb_pair : m_callbacks) {
                local_copy.push_back(cb_pair);
            }
        }

        // Invoke callbacks
        for (const auto& [id, cb] : local_copy) {
            try {
                cb(args...);
            }
            catch (...) {
                if (m_exception_handler) {
                    m_exception_handler(std::current_exception());
                }
            }
        }
    }

    void set_exception_handler(std::function<void(std::exception_ptr)> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_exception_handler = std::move(handler);
    }

private:
    Event() = default;

    void remove(uint64_t id) {
        auto it = std::find_if(m_callbacks.begin(), m_callbacks.end(),
            [id](const auto& pair) { return pair.first == id; });
        if (it != m_callbacks.end()) {
            m_callbacks.erase(it);
        }
    }

    std::list<std::pair<uint64_t, Callback>> m_callbacks;
    std::map<uint64_t, std::weak_ptr<void>> m_owners; // Requires #include <map>
    std::mutex m_mutex;
    std::function<void(std::exception_ptr)> m_exception_handler;
    uint64_t next_id_ = 1;
};