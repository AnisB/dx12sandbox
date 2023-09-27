// Windows include
#include <Windows.h>
#include <ctime>
#include <iostream>

// Graphics API include
#include "gpu_backend_c_api.h"

int CALLBACK main(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
    // Create the window
    uint64_t data[5];
    data[0] = (uint64_t)hInstance;
    GSWindow window = gs_create_window("Test C API", 1980, 1080, false, data);

    // Show the window
    gs_show_window(window);

    // Create the graphics device
    GSGraphicsDevice graphicsDevice = gs_create_graphics_device(true, UINT32_MAX, false);

    // Destroy the graphics device
    gs_destroy_graphics_device(graphicsDevice);

    // Destroy the window
    gs_destroy_window(window);

    return 0;
}