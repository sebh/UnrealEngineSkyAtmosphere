// Copyright Epic Games, Inc. All Rights Reserved.


#include "Dx11Base/WindowHelper.h"
#include "Dx11Base/Dx11Device.h"

#include "Game.h"

#include <imgui.h>
#include "imgui\examples\imgui_impl_dx11.h" 
#include "imgui\examples\imgui_impl_win32.h"

// Declaration of a Imgui function we need (see imgui_impl_win32.h)
IMGUI_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


// the entry point for any Windows program
int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	static bool sVSyncEnable = true;
	static float sTimerGraphWidth = 16.7f;	// 60FPS

	// Get a window size that matches the desired client size
	const unsigned int desiredPosX = 20;
	const unsigned int desiredPosY = 20;
	const unsigned int desiredClientWidth  = 1280;
	const unsigned int desiredClientHeight = 720;
	RECT clientRect;
	clientRect.left		= desiredPosX;
	clientRect.right	= desiredPosX + desiredClientWidth;
	clientRect.bottom	= desiredPosY + desiredClientHeight;
	clientRect.top		= desiredPosY;

	// Create the window
	WindowHelper win(hInstance, clientRect, nCmdShow, L"D3D11 Application");
	win.showWindow();

	// Create the d3d device (a singleton since we only consider a single window)
	Dx11Device::initialise(win.getHwnd());
	DxGpuPerformance::initialise();

	// Initialise imgui
	ImGuiContext* imguiContext = ImGui::CreateContext();
	ImGui::SetCurrentContext(imguiContext);
	ImGui_ImplWin32_Init(win.getHwnd());
	ImGui_ImplDX11_Init(g_dx11Device->getDevice(), g_dx11Device->getDeviceContext());
	// Setup style 
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic(); 

	// Create the game
	Game game;
	game.initialise();


	auto windowResizedCallback = [&](LPARAM lParam) 
	{
		// No threading mechanism so safe to do update call from here.

		uint32 newWidth = LOWORD(lParam);
		uint32 newHeight = HIWORD(lParam);

		ImGui_ImplDX11_InvalidateDeviceObjects();
		g_dx11Device->updateSwapChain(newWidth, newHeight);
		ImGui_ImplDX11_CreateDeviceObjects();

		game.reallocateResolutionDependent(newWidth, newHeight);
	};
	win.setWindowResizedCallback(windowResizedCallback);


	MSG msg = { 0 };
	while (true)
	{
		bool msgValid = win.translateSingleMessage(msg);

		if (msgValid)
		{
			// Update imgui
			ImGui_ImplWin32_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam);
			ImGuiIO& io = ImGui::GetIO();
			if (!(io.WantCaptureMouse || io.WantCaptureKeyboard))
			{
				if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
					break;// process escape key

				if (msg.message == WM_QUIT)
					break; // time to quit

				// A message has been processed and not consumed by imgui. Send the message to the WindowProc function.
				DispatchMessage(&msg);
			}
		}
		else
		{
			DxGpuPerformance::startFrame();
			const char* frameGpuTimerName = "Frame";
			DxGpuPerformance::startGpuTimer(frameGpuTimerName, 150, 150, 150);

			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// Game update
			game.update(win.getInputData());

			// Game render
			game.render();

			// Render UI
			{
				//ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
				//ImGui::ShowTestWindow();

				// Gpu performance timer graph debug print
				const DxGpuPerformance::TimerGraphNode* timerGraphRootNode = DxGpuPerformance::getLastUpdatedTimerGraphRootNode();
				if(timerGraphRootNode)
				{
					ImGui::SetNextWindowSize(ImVec2(400.0f, 400.0f), ImGuiCond_FirstUseEver);
					ImGui::Begin("GPU performance");

					//////////////////////////////////////////////////////////////

					ImGui::Checkbox("VSync", &sVSyncEnable);
					ImGui::SliderFloat("TimerGraphWidth (ms)", &sTimerGraphWidth, 1.0, 60.0);

					// Lambda function parsing the timer graph and displaying it using horizontal bars
					static bool(*imguiPrintTimerGraphRecurse)(const DxGpuPerformance::TimerGraphNode*, int, int)
						= [](const DxGpuPerformance::TimerGraphNode* node, int level, int targetLevel) -> bool
					{
						const float maxWith = ImGui::GetWindowWidth();
						const float msToPixel = maxWith / sTimerGraphWidth;

						bool printDone = false;
						if (level == targetLevel)
						{
							ImU32 color = ImColor(node->r, node->g, node->b);
							ImGui::PushStyleColor(ImGuiCol_Button, color);
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
							ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
							if (node->mLastDurationMs > 0.0f)
							{
								// Set cursor to the correct position and size according to when things started this day
								ImGui::SetCursorPosX(node->mBeginMs * msToPixel);
								ImGui::PushItemWidth(node->mLastDurationMs * msToPixel);

								char debugStr[128];
								sprintf_s(debugStr, 128, "%s %.3f ms\n", node->name.c_str(), node->mLastDurationMs);
								ImGui::Button(debugStr, ImVec2(node->mLastDurationMs * msToPixel, 0.0f));
								if (ImGui::IsItemHovered())
								{
									ImGui::SetTooltip(debugStr);
								}
								ImGui::SameLine();
								ImGui::PopItemWidth();
							}
							printDone = true;
							ImGui::PopStyleColor(3);
						}

						if (level >= targetLevel)
							return printDone;

						for (auto& node : node->subGraph)
						{
							printDone |= imguiPrintTimerGraphRecurse(node, level + 1, targetLevel);
						}

						return printDone;
					};

					ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 2.0f));
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
					ImGui::BeginChild("Timer graph", ImVec2(0, 150), true, ImGuiWindowFlags_HorizontalScrollbar);
					for (int targetLevel = 0; targetLevel < 16; ++targetLevel)
					{
						bool printDone = false;
						for (auto& node : timerGraphRootNode->subGraph)
						{
							printDone |=  imguiPrintTimerGraphRecurse(node, 0, targetLevel);
						}
						if(printDone)
							ImGui::NewLine(); // start a new line if anything has been printed
					}
					ImGui::EndChild();
					ImGui::PopStyleVar(3);

					//////////////////////////////////////////////////////////////

					// Lambda function parsing the timer graph and displaying it as text 
					static void(*textPrintTimerGraphRecurse)(const DxGpuPerformance::TimerGraphNode*, int)
						= [](const DxGpuPerformance::TimerGraphNode* node, int level) -> void
					{
						char* levelOffset = "---------------";	// 16 chars
						unsigned int levelShift = 16 - 2 * level - 1;
						char* levelOffsetPtr = levelOffset + (levelShift<0 ? 0 : levelShift); // cheap way to add shifting to a printf

						char debugStr[128];
						sprintf_s(debugStr, 128, "%s%s %.3f ms\n", levelOffsetPtr, node->name.c_str(), node->mLastDurationMs);
					#if 0
						OutputDebugStringA(debugStr);
					#else
						ImGui::TextColored(ImVec4(node->r, node->g, node->b, 1.0f), debugStr);
					#endif

						for (auto& node : node->subGraph)
						{
							textPrintTimerGraphRecurse(node, level + 1);
						}
					};
					for (auto& node : timerGraphRootNode->subGraph)
					{
						textPrintTimerGraphRecurse(node, 0);
					}

					//////////////////////////////////////////////////////////////

					ImGui::End();
				}

				GPU_SCOPED_TIMEREVENT(Imgui, 0, 162, 232);
				ImGui::Render();
				ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			}

			// Swap the back buffer
			g_dx11Device->swap(sVSyncEnable);
			DxGpuPerformance::endGpuTimer(frameGpuTimerName);
			DxGpuPerformance::endFrame();

			// Events have all been processed in this path by the game
			win.clearInputEvents();
		}
	}

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext(imguiContext);

	game.shutdown();
	DxGpuPerformance::shutdown();
	Dx11Device::shutdown();

	// End of application
	return (int)msg.wParam;
}


