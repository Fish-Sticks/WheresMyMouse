#include <iostream>
#include <Windows.h>
#include <thread>
#include <Xinput.h>
#pragma comment(lib, "Xinput.lib")

__declspec(dllimport) void setup_hook();
__declspec(dllimport) void remove_hook();

void setup()
{
	// Allow mouse the highest priority to preform necessary tasks immediately
	SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(), REALTIME_PRIORITY_CLASS);

	// Setup global keyboard hooks to block input used by the virtual mouse
	std::thread([&]() -> void
	{
		setup_hook();

		// Hooking thread must have a message loop
		MSG msg{};
		while (GetMessageA(&msg, NULL, 0, 0) != 0)
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
	}).detach();

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

int main()
{
	SetConsoleTitleA("Where's My Mouse?");
	std::printf("Welcome to project WMM (Where's My Mouse?)\nThis was coded entirely without a mouse and it's purpose while running is to provide a virtual mouse.\n");
	std::printf("How does it work?\nThis injects mouse input into your hardware input stream to emulate real mouse movement from software.\n");
	std::printf("To stop the virtual mouse press the delete key on your keyboard labeled \"DEL\"\n");
	std::printf("Use the numpad to move the mouse, hold NUMPAD0 to make it move slower and refine what you want to click.\n");
	std::printf("Make sure numlock is on.\n");
	std::printf("2026 update! We finally support controllers :)\n");

	// Set up all initialization shit
	setup();

	// Main program loop
	while (true)
	{
		// Do not instantly preform operation, this allows for multiple operations in one cycle of the loop and keeps control flow nice
		if (GetAsyncKeyState(VK_DELETE))
			break;

		std::this_thread::sleep_for(std::chrono::microseconds(50));
	}

	// Free the global keyboard hook
	remove_hook();
	std::printf("Program break, exiting!\n");
}