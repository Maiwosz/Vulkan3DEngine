#pragma once
#include <string>
#include <vector>
#include <spdlog/spdlog.h>

// ===== DEBUG TRACKING INFRASTRUCTURE =====
#if defined(_WIN32) && defined(_DEBUG)
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")

inline std::string GetSimpleCallStack() {
    void* stack[15];
    HANDLE process = GetCurrentProcess();
    static bool symbolsInitialized = false;

    if (!symbolsInitialized) {
        SymInitialize(process, NULL, TRUE);
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        symbolsInitialized = true;
    }

    WORD frames = CaptureStackBackTrace(0, 15, stack, NULL);
    std::string result;

    for (WORD i = 0; i < frames; i++) {
        DWORD64 address = (DWORD64)(stack[i]);

        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
        PSYMBOL_INFO symbol = (PSYMBOL_INFO)buffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;

        DWORD64 displacement = 0;

        if (SymFromAddr(process, address, &displacement, symbol)) {
            // Pomiń wewnętrzne funkcje trackingu
            std::string name = symbol->Name;
            if (name.find("GetSimpleCallStack") != std::string::npos ||
                name.find("addReference") != std::string::npos ||
                name.find("removeReference") != std::string::npos ||
                name.find("trackReference") != std::string::npos) {
                continue;
            }

            result += "    " + name;

            // Dodaj linię jeśli dostępna
            IMAGEHLP_LINE64 line;
            line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
            DWORD lineDisplacement = 0;

            if (SymGetLineFromAddr64(process, address, &lineDisplacement, &line)) {
                std::string filename = line.FileName;
                size_t lastSlash = filename.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    filename = filename.substr(lastSlash + 1);
                }
                result += " (" + filename + ":" + std::to_string(line.LineNumber) + ")";
            }

            result += "\n";
        }
    }

    return result;
}
#else
inline std::string GetSimpleCallStack() {
    return "    [Stack trace not available on this platform]\n";
}
#endif
// ===== END DEBUG TRACKING INFRASTRUCTURE =====

// Struktura przechowująca informacje o referencjach dla debugowania
struct ReferenceTrackingInfo {
    std::vector<std::string> addReferenceStacks;
    std::vector<std::string> removeReferenceStacks;

    void trackAdd() {
        addReferenceStacks.push_back(GetSimpleCallStack());
    }

    void trackRemove() {
        removeReferenceStacks.push_back(GetSimpleCallStack());
    }

    size_t getBalance() const {
        return addReferenceStacks.size() - removeReferenceStacks.size();
    }
};

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

    // Debug tracking - opcjonalne, domyślnie wyłączone
    virtual void enableDebugTracking(bool enable) {
        m_debugTrackingEnabled = enable;
    }

    virtual bool isDebugTrackingEnabled() const {
        return m_debugTrackingEnabled;
    }

    // Ustawienie konkretnego ID do śledzenia (0 = śledź wszystkie)
    virtual void setTrackedHandleId(uint32_t id) {
        m_trackedHandleId = id;
    }

    virtual uint32_t getTrackedHandleId() const {
        return m_trackedHandleId;
    }

protected:
    // Helper do trackowania referencji - można wywołać z derived class
    void trackReference(ReferenceTrackingInfo& info, bool isAdd, uint32_t handleId,
        const std::string& resourceName, uint32_t refCount) {
        if (!m_debugTrackingEnabled) {
            return;
        }

        // Jeśli ustawiono konkretne ID, śledź tylko to ID
        if (m_trackedHandleId != 0 && handleId != m_trackedHandleId) {
            return;
        }

        if (isAdd) {
            info.trackAdd();
        }
        else {
            info.trackRemove();
        }

        // Jeśli to śledzony zasób, wypisz szczegółowe info
        if (m_trackedHandleId == 0 || handleId == m_trackedHandleId) {
            logReferenceChange(isAdd, handleId, resourceName, refCount,
                isAdd ? info.addReferenceStacks.back() : info.removeReferenceStacks.back());
        }
    }

    // Helper do wypisywania pełnej historii referencji
    void dumpReferenceTracking(uint32_t handleId, const std::string& resourceName,
        uint32_t currentRefCount, const ReferenceTrackingInfo& info) const {
        if (!m_debugTrackingEnabled) {
            return;
        }

        SPDLOG_WARN("║");
        SPDLOG_WARN("║ Resource: '{}' (ID: {})", resourceName, handleId);
        SPDLOG_WARN("║ Current references: {}", currentRefCount);
        SPDLOG_WARN("║ Total adds: {}", info.addReferenceStacks.size());
        SPDLOG_WARN("║ Total removes: {}", info.removeReferenceStacks.size());
        SPDLOG_WARN("║ Balance: {} (should be 0 when released)", info.getBalance());
        SPDLOG_WARN("║");

        // Wypisz wszystkie dodania
        if (!info.addReferenceStacks.empty()) {
            SPDLOG_WARN("╟─── 📈 ALL REFERENCE ADDITIONS ({}) ───", info.addReferenceStacks.size());
            for (size_t i = 0; i < info.addReferenceStacks.size(); i++) {
                SPDLOG_WARN("║");
                SPDLOG_WARN("║ Add #{}", i + 1);
                SPDLOG_WARN("╟─────────────────────────────────────────────────────────");
                SPDLOG_WARN("{}", info.addReferenceStacks[i]);
            }
        }

        // Wypisz wszystkie usunięcia
        if (!info.removeReferenceStacks.empty()) {
            SPDLOG_WARN("╟─── 📉 ALL REFERENCE REMOVALS ({}) ───", info.removeReferenceStacks.size());
            for (size_t i = 0; i < info.removeReferenceStacks.size(); i++) {
                SPDLOG_WARN("║");
                SPDLOG_WARN("║ Remove #{}", i + 1);
                SPDLOG_WARN("╟─────────────────────────────────────────────────────────");
                SPDLOG_WARN("{}", info.removeReferenceStacks[i]);
            }
        }

        // Analiza leaków
        if (currentRefCount > 0 || info.getBalance() > 0) {
            SPDLOG_WARN("║");
            SPDLOG_WARN("╟─── 🔴 LEAK ANALYSIS ───");
            SPDLOG_WARN("║");
            SPDLOG_WARN("║ {} references were added but not removed!", info.getBalance());

            if (info.getBalance() > 0) {
                SPDLOG_WARN("║ Last {} add(s) without matching remove(s):", info.getBalance());

                size_t leakedCount = info.getBalance();
                size_t startIdx = info.addReferenceStacks.size() - leakedCount;

                for (size_t i = startIdx; i < info.addReferenceStacks.size(); i++) {
                    SPDLOG_WARN("║");
                    SPDLOG_WARN("║ 🔴 LEAKED Add #{}", i + 1);
                    SPDLOG_WARN("╟─────────────────────────────────────────────────────────");
                    SPDLOG_WARN("{}", info.addReferenceStacks[i]);
                }
            }
        }
    }

private:
    void logReferenceChange(bool isAdd, uint32_t handleId, const std::string& resourceName,
        uint32_t refCount, const std::string& callStack) const {
        SPDLOG_WARN("\n╔═════════════════════════════════════════════════════════");
        SPDLOG_WARN("║ {} REFERENCE {} for '{}' (ID: {})",
            isAdd ? "📈" : "📉",
            isAdd ? "ADDED" : "REMOVED",
            resourceName, handleId);
        SPDLOG_WARN("║ New ref count: {}", refCount);
        SPDLOG_WARN("╠═════════════════════════════════════════════════════════");
        SPDLOG_WARN("║ Call stack:");
        SPDLOG_WARN("╟─────────────────────────────────────────────────────────");
        SPDLOG_WARN("{}", callStack);
        SPDLOG_WARN("╚═════════════════════════════════════════════════════════\n");
    }

    bool m_debugTrackingEnabled = false;
    uint32_t m_trackedHandleId = 0;  // 0 = track all, otherwise track specific ID
};
