// Bento includes
#include <bento_base/security.h>
#include <bento_memory/common.h>
#include <bento_base/log.h>

// Internal includes
#include "d3d12_backend/dx12_backend.h"
#include "d3d12_backend/dx12_containers.h"
#include "gpu_backend/event_collector.h"
#include "tools/string_utilities.h"

namespace graphics_sandbox
{
	namespace d3d12
	{
		namespace window
		{
			LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
			{
				switch (message)
				{
				case WM_PAINT:
					event_collector::push_event(FrameEvent::Paint);
					break;
				case WM_CLOSE:
					event_collector::push_event(FrameEvent::Close);
					break;
				case WM_DESTROY:
					event_collector::push_event(FrameEvent::Destroy);
					break;
				default:
					return DefWindowProc(hwnd, message, wParam, lParam); // add this
				}
				return 0;
			}

			// Function to register the window
			void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName)
			{
				// Register a window class for creating our render window with.
				WNDCLASSEXW windowClass = {};

				windowClass.cbSize = sizeof(WNDCLASSEX);
				windowClass.style = CS_HREDRAW | CS_VREDRAW;
				windowClass.lpfnWndProc = &WndProc;
				windowClass.cbClsExtra = 0;
				windowClass.cbWndExtra = 0;
				windowClass.hInstance = hInst;
				windowClass.hIcon = LoadIcon(hInst, NULL);
				windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
				windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
				windowClass.lpszMenuName = NULL;
				windowClass.lpszClassName = windowClassName;
				windowClass.hIconSm = LoadIcon(hInst, NULL);

				// Register the window
				assert_msg(RegisterClassExW(&windowClass) > 0, "RegisterClassExW failed.");
			}

			void EvaluateWindowParameters(uint32_t width, uint32_t height, uint32_t& windowWidth, uint32_t& windowHeight, uint32_t& windowX, uint32_t& windowY)
			{
				// Get the size of the monitor
				int screenWidth = GetSystemMetrics(SM_CXSCREEN);
				int screenHeight = GetSystemMetrics(SM_CYSCREEN);

				// Calculates the required size of the window rectangle, based on the desired client-rectangle size.
				RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
				assert_msg(AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE), "AdjustWindowRect failed.");

				windowWidth = windowRect.right - windowRect.left;
				windowHeight = windowRect.bottom - windowRect.top;

				// Center the window within the screen. Clamp to 0, 0 for the top-left corner.
				windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
				windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);
			}

			// Function to create the window
			HWND CreateWindowInternal(const wchar_t* windowClassName, HINSTANCE hInst, const wchar_t* windowTitle, uint32_t width, uint32_t height)
			{
				// Evaluate the actual size and location of the window
				uint32_t windowWidth = 1, windowHeight = 1, windowX = 0, windowY = 0;
				EvaluateWindowParameters(width, height, windowWidth, windowHeight, windowX, windowY);

				// Center the window within the screen.
				HWND hWnd = CreateWindowExW(NULL, windowClassName, windowTitle, WS_OVERLAPPEDWINDOW, windowX, windowY, windowWidth, windowHeight, NULL, NULL, hInst, nullptr);
				assert_msg(hWnd != nullptr, "Failed to create window");

				// Return the created window
				return hWnd;
			}

			RenderWindow create_window(const TGraphicSettings& graphic_settings)
			{
				// Create the window internal structure
				DX12Window* dx12_window = bento::make_new<DX12Window>(*bento::common_allocator());

				// Convert the name from normal to wide
				std::wstring wc = convert_to_wide(graphic_settings.window_name);

				// Grab the instance
				HINSTANCE hInst = (HINSTANCE)graphic_settings.data[0];

				// Register the window
				RegisterWindowClass(hInst, wc.c_str());

				// Create the window
				dx12_window->width = graphic_settings.width;
				dx12_window->height = graphic_settings.height;
				dx12_window->window = CreateWindowInternal(wc.c_str(), hInst, wc.c_str(), dx12_window->width, dx12_window->height);
				assert_msg(dx12_window->window != nullptr, "Failed to create window.");

				// Cast the window to the opaque type
				return (RenderWindow)dx12_window;
			}

			void destroy_window(RenderWindow renderWindow)
			{
				// Grab the internal windows structure
				DX12Window* dx12_window = (DX12Window*)renderWindow;

				// Destroy the actual window
				assert_msg(DestroyWindow(dx12_window->window), "Failed to destroy window.");

				// Detroy the internal window structure
				bento::make_delete<DX12Window>(*bento::common_allocator(), dx12_window);
			}

			void show(RenderWindow renderWindow)
			{
				DX12Window* dx12_window = (DX12Window*)renderWindow;
				ShowWindow(dx12_window->window, SW_SHOWDEFAULT);
			}

			void hide(RenderWindow renderWindow)
			{
				DX12Window* dx12_window = (DX12Window*)renderWindow;
				ShowWindow(dx12_window->window, SW_HIDE);
			}
		}
	}
}
