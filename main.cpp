#include <windows.h>
#include <atomic>
#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <thread>

class ScancodeInjector {
public:
    static bool SendKeyPress(WORD scanCode, bool isExtended = false) {
        return SendInputState(scanCode, false, isExtended) && 
               SendInputState(scanCode, true, isExtended);
    }

private:
    static bool SendInputState(WORD scanCode, bool keyUp, bool isExtended) {
        INPUT input{};
        input.type = INPUT_KEYBOARD;
        input.ki.wScan = scanCode;
        input.ki.dwFlags = KEYEVENTF_SCANCODE;

        if (keyUp) {
            input.ki.dwFlags |= KEYEVENTF_KEYUP;
        }
        if (isExtended) {
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }

        return SendInput(1, &input, sizeof(INPUT)) == 1;
    }
};

class KeyboardHook {
public:
    using KeyHandler = void(*)(DWORD vkCode);

    explicit KeyboardHook(KeyHandler handler) {
        activeHandler = handler;
        hookHandle = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
        if (!hookHandle) {
            throw std::runtime_error("Failed to install low-level keyboard hook.");
        }
    }

    ~KeyboardHook() {
        if (hookHandle) {
            UnhookWindowsHookEx(hookHandle);
        }
        activeHandler = nullptr;
    }

    KeyboardHook(const KeyboardHook&) = delete;
    KeyboardHook& operator=(const KeyboardHook&) = delete;

private:
    static inline HHOOK hookHandle = nullptr;
    static inline KeyHandler activeHandler = nullptr;

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && activeHandler) {
            const auto* keyData = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
            const bool isInjected = (keyData->flags & LLKHF_INJECTED) != 0;

            if (!isInjected && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
                activeHandler(keyData->vkCode);
            }
        }
        return CallNextHookEx(hookHandle, nCode, wParam, lParam);
    }
};

class StreamDeckEngine {
public:
    StreamDeckEngine() = default;

    ~StreamDeckEngine() {
        Stop();
    }

    void Start() {
        workerThread = std::thread(&StreamDeckEngine::RunSequence, this);
    }

    void Stop() {
        shuttingDown.store(true);
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }

    void HandleHotkey(DWORD vkCode) {
        if (vkCode == VK_F6) {
            const bool newState = !running.load();
            running.store(newState);
            std::cout << (newState ? "Sequence started.\n" : "Sequence stopped.\n");
        } else if (vkCode == VK_F10) {
            const bool newState = !paused.load();
            paused.store(newState);
            std::cout << (newState ? "Sequence paused.\n" : "Sequence resumed.\n");
        }
    }

private:
    static constexpr WORD ScanCodeZ = 0x2C;
    static constexpr WORD ScanCodeX = 0x2D;
    static constexpr auto KeyDelay = std::chrono::milliseconds(30);

    std::atomic<bool> running{false};
    std::atomic<bool> paused{false};
    std::atomic<bool> shuttingDown{false};
    std::thread workerThread;

    void RunSequence() {
        while (!shuttingDown.load()) {
            if (!running.load() || paused.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            if (!ExecuteKeyCombination()) {
                std::cerr << "Error sending key sequence.\n";
                running.store(false);
                continue;
            }

            std::this_thread::sleep_for(KeyDelay);
        }
    }

    bool ExecuteKeyCombination() {
        return ScancodeInjector::SendKeyPress(ScanCodeZ) && 
               ScancodeInjector::SendKeyPress(ScanCodeX);
    }
};

namespace {
    std::unique_ptr<StreamDeckEngine> g_engine;

    void OnHotkeyPressed(DWORD vkCode) {
        if (g_engine) {
            g_engine->HandleHotkey(vkCode);
        }
    }
}

int main() {
    try {
        g_engine = std::make_unique<StreamDeckEngine>();
        KeyboardHook hook(OnHotkeyPressed);

        g_engine->Start();

        std::cout << "Stream Deck Engine Active.\n";
        std::cout << "F6: Start/Stop | F10: Pause/Resume\n";

        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal Error: " << ex.what() << '\n';
        return 1;
    }

    return 0;
}