#include <Windows.h>
#include <iostream>
#include <shared_mutex>

static HHOOK keyboard_hook = NULL;

// Data must be accessible via mutex to prevent multiple threads from race condition (keyboard hooking thread & updating thread)
std::shared_mutex my_mutex{};
int IS_UP_DOWN = 0, IS_DOWN_DOWN = 0, IS_LEFT_DOWN = 0, IS_RIGHT_DOWN = 0, IS_SLOW_DOWN = 0, IS_LEFT_MOUSE_DOWN = 0, IS_RIGHT_MOUSE_DOWN = 0, IS_LEFT_CLICKING = 0, IS_RIGHT_CLICKING = 0, IS_SCROLL_DOWN_DOWN = 0, IS_SCROLL_UP_DOWN = 0;

// Adjust as needed
int MOUSE_SPEED = 10;
int SCROLL_SPEED = WHEEL_DELTA / 4;

// Move variables
int MOVE_X = 0, MOVE_Y = 0, OFFSET = 0, SCROLL_OFFSET = 0;

LRESULT CALLBACK LL_keyboard_hook(int code, WPARAM wParam, LPARAM lParam)
{    
    if (code < 0)
        return CallNextHookEx(NULL, code, wParam, lParam);

    const DWORD keycode = reinterpret_cast<LPKBDLLHOOKSTRUCT>(lParam)->vkCode;

    // Key access scope, out of scope the lock dies and the resource is free
    {
        std::lock_guard<std::shared_mutex> key_access_mutex{ my_mutex };
        switch (keycode)
        {
        case VK_NUMPAD8:
            IS_UP_DOWN = (wParam == WM_KEYDOWN ? IS_UP_DOWN = 1 : 0);
            break;
        case VK_NUMPAD5: // same functionality incase user wants to use it more like an "arrow keys" format than a long way
        case VK_NUMPAD2:
            IS_DOWN_DOWN = (wParam == WM_KEYDOWN ? IS_DOWN_DOWN = 1 : 0);
            break;
        case VK_NUMPAD4:
            IS_LEFT_DOWN = (wParam == WM_KEYDOWN ? IS_LEFT_DOWN = 1 : 0);
            break;
        case VK_NUMPAD6:
            IS_RIGHT_DOWN = (wParam == WM_KEYDOWN ? IS_RIGHT_DOWN = 1 : 0);
            break;
        case VK_NUMPAD7:
            IS_LEFT_MOUSE_DOWN = (wParam == WM_KEYDOWN ? IS_LEFT_MOUSE_DOWN = 1 : 0);
            break;
        case VK_NUMPAD9:
            IS_RIGHT_MOUSE_DOWN = (wParam == WM_KEYDOWN ? IS_RIGHT_MOUSE_DOWN = 1 : 0);
            break;
        case VK_NUMPAD0:
            IS_SLOW_DOWN = (wParam == WM_KEYDOWN ? IS_SLOW_DOWN = 1 : 0);
            break;
        case VK_PRIOR:
            IS_SCROLL_UP_DOWN = (wParam == WM_KEYDOWN ? IS_SCROLL_UP_DOWN = 1 : 0);
            break;
        case VK_NEXT:
            IS_SCROLL_DOWN_DOWN = (wParam == WM_KEYDOWN ? IS_SCROLL_DOWN_DOWN = 1 : 0);
            break;
        }
    }

    // If it's one of our virtual mouse keys we cannot let it pass through ; it's going to interfere and make things super duper annoying
    switch (keycode)
    {
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
    case VK_NUMPAD4:
    case VK_NUMPAD6:
    case VK_NUMPAD5:
    case VK_NUMPAD2:
    case VK_NUMPAD0:
    case VK_PRIOR:
    case VK_NEXT:
        return 1; // "If the hook procedure processed the message, it may return a nonzero value to prevent the system from passing the message to the rest of the hook chain or the target window procedure."
        break;
    }

    return CallNextHookEx(NULL, code, wParam, lParam);
}

// Updating thread for more smooth movements in between setter states (keyboard hook runs at a slow rate, so I control on my own)
void updating_thread()
{
    while (true)
    {
        {
            std::lock_guard<std::shared_mutex> key_access_mutex{ my_mutex };
            if (IS_SLOW_DOWN)
            {
                SCROLL_OFFSET = 0;
                OFFSET = MOUSE_SPEED / 2;
            }
            else
            {
                SCROLL_OFFSET = SCROLL_SPEED / 2;
                OFFSET = 0;
            }

            if (IS_UP_DOWN)
                MOVE_Y -= MOUSE_SPEED - OFFSET;
            if (IS_DOWN_DOWN)
                MOVE_Y += MOUSE_SPEED - OFFSET;
            if (IS_LEFT_DOWN)
                MOVE_X -= MOUSE_SPEED - OFFSET;
            if (IS_RIGHT_DOWN)
                MOVE_X += MOUSE_SPEED - OFFSET;
            if (IS_LEFT_MOUSE_DOWN && IS_LEFT_CLICKING == 0)
                IS_LEFT_CLICKING = 3;
            else if (!IS_LEFT_MOUSE_DOWN && IS_LEFT_CLICKING == 1)
            {
                IS_LEFT_CLICKING = 0;
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, GetMessageExtraInfo());
            }
            if (IS_RIGHT_MOUSE_DOWN && IS_RIGHT_CLICKING == 0)
                IS_RIGHT_CLICKING = 3;
            else if (!IS_RIGHT_MOUSE_DOWN && IS_RIGHT_CLICKING == 1)
            {
                IS_RIGHT_CLICKING = 0;
                mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, GetMessageExtraInfo());
            }
        }

        if (MOVE_X || MOVE_Y)
            mouse_event(MOUSEEVENTF_MOVE, MOVE_X, MOVE_Y, 0, GetMessageExtraInfo());

        if (IS_LEFT_CLICKING == 3)
        {
            IS_LEFT_CLICKING = 1;
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, GetMessageExtraInfo());
        }

        if (IS_RIGHT_CLICKING == 3)
        {
            IS_RIGHT_CLICKING = 1;
            mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, GetMessageExtraInfo());
        }

        if (IS_SCROLL_UP_DOWN)
        {
            // 6 portions of wheel delta to make a full rotate
            mouse_event(MOUSEEVENTF_WHEEL, 0, 0, SCROLL_SPEED + SCROLL_OFFSET, GetMessageExtraInfo());
        }

        if (IS_SCROLL_DOWN_DOWN)
        {
            mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -(SCROLL_SPEED + SCROLL_OFFSET), GetMessageExtraInfo());
        }

        MOVE_X = 0;
        MOVE_Y = 0;

        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
}

__declspec(dllexport) void setup_hook()
{
    keyboard_hook = SetWindowsHookExA(WH_KEYBOARD_LL, LL_keyboard_hook, GetModuleHandle(NULL), NULL);
    if (keyboard_hook)
    {
        std::thread(updating_thread).detach();
        std::printf("Setup global keyboard hook!\n");
    }
    else
        std::printf("Failed to setup keyboard hook! Input may be annoying as arrow keys will trigger actions in the app.\n");
}

__declspec(dllexport) void remove_hook()
{
    UnhookWindowsHookEx(keyboard_hook);
    std::printf("Removed global keyboard hook!\n");
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            std::printf("LL keyboard hook loaded!\n");
            break;
    }
    return TRUE;
}