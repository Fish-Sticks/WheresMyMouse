#include <Windows.h>
#include <iostream>
#include <shared_mutex>
#include <unordered_map>

// Controller support patch, only took me 3 years!
#include <Xinput.h>
#include <winuser.h>
#include "resource.h"
#pragma comment(lib, "Xinput.lib")

static HMODULE dll = NULL;
static HHOOK keyboard_hook = NULL;

// Data must be accessible via mutex to prevent multiple threads from race condition (keyboard hooking thread & updating thread)
std::shared_mutex my_mutex{};
int IS_UP_DOWN = 0, IS_DOWN_DOWN = 0, IS_LEFT_DOWN = 0, IS_RIGHT_DOWN = 0, IS_SLOW_DOWN = 0, IS_LEFT_MOUSE_DOWN = 0, IS_RIGHT_MOUSE_DOWN = 0, IS_LEFT_CLICKING = 0, IS_RIGHT_CLICKING = 0, IS_SCROLL_DOWN_DOWN = 0, IS_SCROLL_UP_DOWN = 0;
int IS_USING_CONTROLLER = 0;

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
    if (translated_key != key_type_t::NONE && !IS_USING_CONTROLLER)
    {
        std::lock_guard<std::shared_mutex> key_access_mutex{ my_mutex };

        if (!IS_EXTENDED_KEY)
        {
            switch (translated_key)
            {
            case key_type_t::UP:
                IS_UP_DOWN = IS_KEY_DOWN;
                break;
            case key_type_t::DOWN: // same functionality incase user wants to use it more like an "arrow keys" format than a long way
                IS_DOWN_DOWN = IS_KEY_DOWN;
                break;
            case key_type_t::LEFT:
                IS_LEFT_DOWN = IS_KEY_DOWN;
                break;
            case key_type_t::RIGHT:
                IS_RIGHT_DOWN = IS_KEY_DOWN;
                break;
            case key_type_t::LEFT_CLICK:
                IS_LEFT_MOUSE_DOWN = IS_KEY_DOWN;
                break;
            case key_type_t::RIGHT_CLICK:
                IS_RIGHT_MOUSE_DOWN = IS_KEY_DOWN;
                break;
            case key_type_t::SLOW:
                IS_SLOW_DOWN = IS_KEY_DOWN;
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


// Added controller support into its own thread!
void update_controller_state()
{
    static int previousPacketID = 0;
    static XINPUT_STATE controllerState{ 0 };

    // Only supports one controller so we use index 0. If there's no controller state then we exit
    if (XInputGetState(0, &controllerState) != ERROR_SUCCESS)
    {
        if (IS_USING_CONTROLLER) {
            std::printf("CONTROLLER HAS BEEN DISCONNECTED, SWITCHING TO KEYBOARD MODE!\n");
            SystemParametersInfoA(SPI_SETCURSORS, NULL, nullptr, NULL);
        }

        IS_USING_CONTROLLER = 0;
        return;
    }
    
    if (!IS_USING_CONTROLLER) {
        std::printf("CONTROLLER HAS BEEN DETECTED! SWITCHING INPUT MODES TO CONTROLLER, KEYBOARD WILL NOT BE READ.\n");
        HCURSOR myCursor = LoadCursorA(dll, MAKEINTRESOURCEA(IDC_CURSOR1));
        HCURSOR myCopyCursor = CopyCursor(myCursor);
        
        SetSystemCursor(myCopyCursor, 32512);
        SetSystemCursor(myCopyCursor, 32513);
        SetSystemCursor(myCopyCursor, 32515);
        SetSystemCursor(myCopyCursor, 32516);
        SetSystemCursor(myCopyCursor, 32646);
        SetSystemCursor(myCopyCursor, 32649);
    }
    IS_USING_CONTROLLER = 1;

    // Only process an individual packet once, no point in processing the dupes (could be bad!)
    if (controllerState.dwPacketNumber == previousPacketID)
        return;

    float lX = controllerState.Gamepad.sThumbLX, lY = controllerState.Gamepad.sThumbLY;
    float rX = controllerState.Gamepad.sThumbRX, rY = controllerState.Gamepad.sThumbRY;

    // Important controller info
    float NLX = 0.f, NLY = 0.f;
    float NRX = 0.f, NRY = 0.f;
    bool leftBumper = controllerState.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD, rightBumper = controllerState.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

    // Calculated normalized for left
    {
        float magnitude = sqrt(lX * lX + lY * lY);

        if (magnitude > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
        {
            // Calculate normalized after checking for dead zone
            NLX = (lX / 32767);
            NLY = (lY / 32767);
        }
        else
        {
            NLX = 0.f;
            NLY = 0.f;
        }
    }

    // Calculate normalized for right
    {
        float magnitude = sqrt(rX * rX + rY * rY);

        if (magnitude > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
        {
            // Calculate normalized after checking for dead zone
            NRX = (rX / 32767);
            NRY = (rY / 32767);
        }
        else
        {
            NRX = 0.f;
            NRY = 0.f;
        }
    }
    

    IS_SLOW_DOWN = controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
    IS_SCROLL_UP_DOWN = ((NRY != 0.f) && NRY > 0.f);
    IS_SCROLL_DOWN_DOWN = ((NRY != 0.f) && NRY < 0.f);
  
    if (NRY < 0.f)
        NRY = NRY * -1.f;

 
    IS_LEFT_MOUSE_DOWN = leftBumper;
    IS_RIGHT_MOUSE_DOWN = rightBumper;

    int VARIABLE_MOUSE_SPEED = IS_SLOW_DOWN ? MOUSE_SPEED / 2 : MOUSE_SPEED;

    MOVE_X = (int)(NLX * (float)(VARIABLE_MOUSE_SPEED));
    MOVE_Y = (int)((NLY*-1.f) * (float)(VARIABLE_MOUSE_SPEED)); // Y must be inverse since its opposite on controller

    int VARIABLE_SCROLL_SPEED = IS_SLOW_DOWN ? (SCROLL_SPEED / 2) : SCROLL_SPEED;

    // Negate the scroll speed from keyboard based scrolling and add analog values
    SCROLL_OFFSET = (-SCROLL_SPEED) + (int)((float)VARIABLE_SCROLL_SPEED * NRY);
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

            // Controller hook here since it will dynamically be changing these move values
            update_controller_state(); // Must be called after acquiring the mutex

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
    SystemParametersInfoA(SPI_SETCURSORS, NULL, nullptr, NULL);

    std::printf("Removed global keyboard hook!\n");
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    dll = hModule;
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
            std::printf("LL keyboard hook loaded!\n");
            break;
    }
    return TRUE;
}
