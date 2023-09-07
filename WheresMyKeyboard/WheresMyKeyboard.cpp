#include <Windows.h>
#include <iostream>
#include <shared_mutex>
#include <unordered_map>

static HHOOK keyboard_hook = NULL;

// Data must be accessible via mutex to prevent multiple threads from race condition (keyboard hooking thread & updating thread)
std::shared_mutex my_mutex{};
int IS_UP_DOWN = 0, IS_DOWN_DOWN = 0, IS_LEFT_DOWN = 0, IS_RIGHT_DOWN = 0, IS_SLOW_DOWN = 0, IS_LEFT_MOUSE_DOWN = 0, IS_RIGHT_MOUSE_DOWN = 0, IS_LEFT_CLICKING = 0, IS_RIGHT_CLICKING = 0, IS_SCROLL_DOWN_DOWN = 0, IS_SCROLL_UP_DOWN = 0;

// Adjust as needed
int MOUSE_SPEED = 10;
int SCROLL_SPEED = WHEEL_DELTA / 4;

// Move variables
int MOVE_X = 0, MOVE_Y = 0, OFFSET = 0, SCROLL_OFFSET = 0;

// Use physical variant to ignore things such as shift key doing other functionality (most games use shift as run, this would interfere and break the mouse)
const UINT SLOW_KEY = MapVirtualKeyA(VK_NUMPAD0, MAPVK_VK_TO_VSC);
const UINT UP_KEY = MapVirtualKeyA(VK_NUMPAD8, MAPVK_VK_TO_VSC);
const UINT DOWN_KEY = MapVirtualKeyA(VK_NUMPAD2, MAPVK_VK_TO_VSC);
const UINT DOWN_KEY_2 = MapVirtualKeyA(VK_NUMPAD5, MAPVK_VK_TO_VSC);
const UINT LEFT_KEY = MapVirtualKeyA(VK_NUMPAD4, MAPVK_VK_TO_VSC);
const UINT RIGHT_KEY = MapVirtualKeyA(VK_NUMPAD6, MAPVK_VK_TO_VSC);
const UINT LEFT_CLICK_KEY = MapVirtualKeyA(VK_NUMPAD7, MAPVK_VK_TO_VSC_EX);
const UINT RIGHT_CLICK_KEY = MapVirtualKeyA(VK_NUMPAD9, MAPVK_VK_TO_VSC);
const UINT SCROLL_UP_KEY = MapVirtualKeyA(VK_PRIOR, MAPVK_VK_TO_VSC);
const UINT SCROLL_DOWN_KEY = MapVirtualKeyA(VK_NEXT, MAPVK_VK_TO_VSC);

// Constant enums so the switch statement won't complain
// Make sure if they are the same mapping you mark them the same value
enum struct key_type_t : std::uint8_t
{
    NONE = 0, SLOW, UP, DOWN, LEFT, RIGHT, LEFT_CLICK, RIGHT_CLICK = 69, SCROLL_UP = 69, SCROLL_DOWN
};

// Mappings from scancode to constant key codes
std::unordered_map<UINT, key_type_t> key_mappings = 
{
    {SLOW_KEY, key_type_t::SLOW}, {UP_KEY, key_type_t::UP}, {DOWN_KEY, key_type_t::DOWN}, {DOWN_KEY_2, key_type_t::DOWN}, {LEFT_KEY, key_type_t::LEFT}, {RIGHT_KEY, key_type_t::RIGHT},
    {LEFT_CLICK_KEY, key_type_t::LEFT_CLICK}, {RIGHT_CLICK_KEY, key_type_t::RIGHT_CLICK}, {SCROLL_UP_KEY, key_type_t::SCROLL_UP}, {SCROLL_DOWN_KEY, key_type_t::SCROLL_DOWN}
};

LRESULT CALLBACK LL_keyboard_hook(int code, WPARAM wParam, LPARAM lParam)
{    
    if (code < 0)
        return CallNextHookEx(NULL, code, wParam, lParam);

    LPKBDLLHOOKSTRUCT hook_struct = reinterpret_cast<LPKBDLLHOOKSTRUCT>(lParam);
    const UINT scancode = hook_struct->scanCode;
    const bool IS_KEY_DOWN = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
    const bool IS_EXTENDED_KEY = hook_struct->flags & LLKHF_EXTENDED; // is it a functionality key?

    key_type_t translated_key = key_type_t::NONE;
    if (const auto found = key_mappings.find(scancode); found != key_mappings.end())
        translated_key = found->second;

    // Key access scope, out of scope the lock dies and the resource is free
    if (translated_key != key_type_t::NONE)
    {
        std::lock_guard<std::shared_mutex> key_access_mutex{ my_mutex };

        if (!IS_EXTENDED_KEY)
        {
            switch (translated_key)
            {
            case key_type_t::UP:
                IS_UP_DOWN = (IS_KEY_DOWN ? IS_UP_DOWN = 1 : 0);
                break;
            case key_type_t::DOWN: // same functionality incase user wants to use it more like an "arrow keys" format than a long way
                IS_DOWN_DOWN = (IS_KEY_DOWN ? IS_DOWN_DOWN = 1 : 0);
                break;
            case key_type_t::LEFT:
                IS_LEFT_DOWN = (IS_KEY_DOWN ? IS_LEFT_DOWN = 1 : 0);
                break;
            case key_type_t::RIGHT:
                IS_RIGHT_DOWN = (IS_KEY_DOWN ? IS_RIGHT_DOWN = 1 : 0);
                break;
            case key_type_t::LEFT_CLICK:
                IS_LEFT_MOUSE_DOWN = (IS_KEY_DOWN ? IS_LEFT_MOUSE_DOWN = 1 : 0);
                break;
            case key_type_t::RIGHT_CLICK:
                IS_RIGHT_MOUSE_DOWN = (IS_KEY_DOWN ? IS_RIGHT_MOUSE_DOWN = 1 : 0);
                break;
            case key_type_t::SLOW:
                IS_SLOW_DOWN = (IS_KEY_DOWN ? IS_SLOW_DOWN = 1 : 0);
                break;
            }

            // If it's one of our virtual mouse keys we cannot let it pass through ; it's going to interfere and make things super duper annoying
            return 1;
        }
        else if (IS_EXTENDED_KEY && (translated_key == key_type_t::SCROLL_DOWN || translated_key == key_type_t::SCROLL_UP)) // functionality version character keys
        {
            switch (translated_key)
            {
            case key_type_t::SCROLL_UP:
                IS_SCROLL_UP_DOWN = (IS_KEY_DOWN ? IS_SCROLL_UP_DOWN = 1 : 0);
                break;
            case key_type_t::SCROLL_DOWN:
                IS_SCROLL_DOWN_DOWN = (IS_KEY_DOWN ? IS_SCROLL_DOWN_DOWN = 1 : 0);
                break;
            }

            // If it's one of our virtual mouse keys we cannot let it pass through ; it's going to interfere and make things super duper annoying
            return 1;
        }
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
                SCROLL_OFFSET = SCROLL_SPEED / 2;
                OFFSET = MOUSE_SPEED / 2;
            }
            else
            {
                SCROLL_OFFSET = 0;
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
        if (IS_SCROLL_UP_DOWN)
            mouse_event(MOUSEEVENTF_WHEEL, 0, 0, SCROLL_SPEED + SCROLL_OFFSET, GetMessageExtraInfo());
        if (IS_SCROLL_DOWN_DOWN)
            mouse_event(MOUSEEVENTF_WHEEL, 0, 0, -(SCROLL_SPEED + SCROLL_OFFSET), GetMessageExtraInfo());

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
        std::printf("Failed to setup keyboard hook! Input may be annoying as movement keys will trigger actions in the app.\n");
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
