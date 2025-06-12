#pragma once
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <algorithm>
#include <exception>
#include <atomic>

template <typename... Args>
class Event {
public:
    using Callback = std::function<void(Args...)>;
    using ExceptionHandler = std::function<void(std::exception_ptr)>;

    class Subscription {
    public:
        Subscription() = default;

        Subscription(std::function<void()> unsubscribe_fn)
            : unsubscribe_fn_(std::move(unsubscribe_fn)) {
        }

        ~Subscription() {
            unsubscribe();
        }

        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        Subscription(Subscription&& other) noexcept
            : unsubscribe_fn_(std::move(other.unsubscribe_fn_)) {
        }

        Subscription& operator=(Subscription&& other) noexcept {
            if (this != &other) {
                unsubscribe();
                unsubscribe_fn_ = std::move(other.unsubscribe_fn_);
            }
            return *this;
        }

        void unsubscribe() {
            if (unsubscribe_fn_) {
                unsubscribe_fn_();
                unsubscribe_fn_ = nullptr;
            }
        }

        bool isValid() const {
            return static_cast<bool>(unsubscribe_fn_);
        }

    private:
        std::function<void()> unsubscribe_fn_;
    };

    Event() = default;
    ~Event() {
        destroyed_.store(true);
        std::lock_guard<std::mutex> lock(mutex_);
    }

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    Event(Event&& other) noexcept
        : callbacks_(std::move(other.callbacks_))
        , exception_handler_(std::move(other.exception_handler_))
        , next_id_(other.next_id_)
        , destroyed_(other.destroyed_.load()) {
    }

    Event& operator=(Event&& other) noexcept {
        if (this != &other) {
            std::lock_guard<std::mutex> lock1(mutex_);
            std::lock_guard<std::mutex> lock2(other.mutex_);

            destroyed_.store(true);
            callbacks_ = std::move(other.callbacks_);
            exception_handler_ = std::move(other.exception_handler_);
            next_id_ = other.next_id_;
            destroyed_.store(other.destroyed_.load());
        }
        return *this;
    }

    [[nodiscard]] Subscription subscribe(Callback callback) {
        if (!callback || destroyed_.load()) {
            return Subscription();
        }

        std::lock_guard<std::mutex> lock(mutex_);

        if (destroyed_.load()) {
            return Subscription();
        }

        auto id = next_id_++;
        callbacks_.emplace_back(id, std::move(callback));

        return Subscription([this, id]() {
            removeCallback(id);
            });
    }

    // POPRAWIONA metoda invoke - eliminuje deadlock
    void invoke(Args... args) {
        if (destroyed_.load()) {
            return;
        }

        // Kopia callbacków i exception handlera poza mutexem
        std::vector<Callback> callbacks_to_call;
        ExceptionHandler current_exception_handler;

        // Krótka sekcja krytyczna - tylko kopiowanie
        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (destroyed_.load() || callbacks_.empty()) {
                return;
            }

            // Bezpieczne kopiowanie z obsługą wyjątków
            try {
                callbacks_to_call.reserve(callbacks_.size());
                for (const auto& [id, callback] : callbacks_) {
                    if (callback) {
                        callbacks_to_call.push_back(callback);
                    }
                }
                // Skopiuj także exception handler
                current_exception_handler = exception_handler_;
            }
            catch (const std::bad_alloc&) {
                // Jeśli nie można zaalokować, spróbuj bez reserve
                callbacks_to_call.clear();
                for (const auto& [id, callback] : callbacks_) {
                    if (callback) {
                        try {
                            callbacks_to_call.push_back(callback);
                        }
                        catch (const std::bad_alloc&) {
                            break;
                        }
                    }
                }
                current_exception_handler = exception_handler_;
            }
        }
        // MUTEX JEST JUŻ ODBLOKOWANY - callbacks mogą bezpiecznie wywoływać inne metody

        // Wywołaj callbacki bez blokowania mutexu
        for (const auto& callback : callbacks_to_call) {
            if (destroyed_.load()) {
                break;
            }

            try {
                callback(args...);
            }
            catch (...) {
                // Obsłuż wyjątek BEZ ponownego blokowania mutex
                handleException(std::current_exception(), current_exception_handler);
            }
        }
    }

    void safeInvoke(Args... args) noexcept {
        try {
            invoke(args...);
        }
        catch (...) {
            // Ignoruj wszystkie wyjątki
        }
    }

    void setExceptionHandler(ExceptionHandler handler) {
        if (destroyed_.load()) return;

        std::lock_guard<std::mutex> lock(mutex_);
        if (!destroyed_.load()) {
            exception_handler_ = std::move(handler);
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.clear();
    }

    size_t subscriberCount() const {
        if (destroyed_.load()) return 0;

        std::lock_guard<std::mutex> lock(mutex_);
        return destroyed_.load() ? 0 : callbacks_.size();
    }

    bool hasSubscribers() const {
        return subscriberCount() > 0;
    }

    bool isDestroyed() const {
        return destroyed_.load();
    }

private:
    using CallbackId = uint64_t;
    using CallbackPair = std::pair<CallbackId, Callback>;

    void removeCallback(CallbackId id) {
        if (destroyed_.load()) return;

        std::lock_guard<std::mutex> lock(mutex_);
        if (!destroyed_.load()) {
            callbacks_.erase(
                std::remove_if(callbacks_.begin(), callbacks_.end(),
                    [id](const auto& pair) { return pair.first == id; }),
                callbacks_.end());
        }
    }

    // obsługa wyjątków bez ponownego blokowania mutex
    void handleException(std::exception_ptr ex, const ExceptionHandler& handler) noexcept {
        if (destroyed_.load() || !handler) {
            return;
        }

        try {
            handler(ex);
        }
        catch (...) {
            // Ignoruj wyjątki z handlera - nie można już nic zrobić
        }
    }

    mutable std::mutex mutex_;
    std::vector<CallbackPair> callbacks_;
    ExceptionHandler exception_handler_;
    CallbackId next_id_ = 1;
    std::atomic<bool> destroyed_{ false };
};

template <typename... Args>
auto makeEvent() {
    return Event<Args...>{};
}