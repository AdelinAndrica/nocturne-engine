#include "Runtime/Engine.h"
#include "Platform/Win32/WinWindow.h"
#include "Runtime/MainLoop.h" // not used anymore, but ok if included elsewhere
#include "Core/Log.h"
#include "Core/Clock.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>

#ifndef NOC_CONTENT_ROOT
#define NOC_CONTENT_ROOT "."
#endif

// -------------------------------
// Phase 10 Host-side test state
// -------------------------------
static noc::SceneObjectHandle gParent;
static noc::SceneObjectHandle gCam;
static bool gCulling = true;

static noc::SceneObjectHandle gCullingProbe;
static float gProbeX = 0.0f;


static void BuildPhase10TestScene(noc::Engine& engine)
{
	auto& w = engine.GetWorld();

	// Camera object
	gCam = w.CreateObject();
	w.SetLocalTRS(gCam, noc::Vec3(0, 0, -5), noc::Quat::Identity(), noc::Vec3::One());
	w.SetCameraFromObject(gCam);

	// IMPORTANT: keep farZ large so GPU can still draw objects we "uncull"
	w.SetCameraParams(1.04719755f, 16.0f / 9.0f, 0.1f, 200.0f);

	// Triangle mesh handle
	noc::ResourceHandle tri = engine.Resources().RequestBinary("Meshes/triangle.nmsh");

	// Conservative bounds
	noc::AABB triBounds{ noc::Vec3(-1, -1, -1), noc::Vec3(1, 1, 1) };

	// Parent
	gParent = w.CreateObject();
	w.SetLocalTRS(gParent, noc::Vec3(-3.0f, 0.0f, 5.0f), noc::Quat::Identity(), noc::Vec3::One());
	w.SetRenderable(gParent, tri, triBounds);

	gCullingProbe = w.CreateObject();
	w.SetLocalTRS(gCullingProbe, noc::Vec3(0.0f, 0.0f, 20.0f), noc::Quat::Identity(), noc::Vec3::One());
	w.SetRenderable(gCullingProbe, tri, triBounds);


	// Children follow parent (visible baseline)
	for (int i = 0; i < 5; ++i)
	{
		noc::SceneObjectHandle child = w.CreateObject();
		w.SetParent(child, gParent);
		w.SetLocalTRS(child, noc::Vec3((float)i * 1.5f, 0.0f, 0.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(child, tri, triBounds);
	}

	// ---- CULLING DEMO OBJECTS ----
	// These are inside far plane (z=30) so GPU can draw them.
	// They are far on X so the frustum *should* reject them when culling is enabled.

	// Off-right (should be culled when ON, visible when OFF)
	{
		noc::SceneObjectHandle offRight = w.CreateObject();
		w.SetLocalTRS(offRight, noc::Vec3(60.0f, 0.0f, 30.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(offRight, tri, triBounds);
	}

	// Off-left (should be culled when ON, visible when OFF)
	{
		noc::SceneObjectHandle offLeft = w.CreateObject();
		w.SetLocalTRS(offLeft, noc::Vec3(-60.0f, 0.0f, 30.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(offLeft, tri, triBounds);
	}

	// Slightly above (tests top plane)
	{
		noc::SceneObjectHandle offUp = w.CreateObject();
		w.SetLocalTRS(offUp, noc::Vec3(0.0f, 40.0f, 30.0f), noc::Quat::Identity(), noc::Vec3::One());
		w.SetRenderable(offUp, tri, triBounds);
	}

	gCulling = true;
	w.SetCullingEnabled(gCulling);

	NOC_LOG_INFO("Host",
		"Phase 10 test scene built. Expected: culling ON shows only the hierarchy; culling OFF increases draw count.");
}



static void UpdatePhase10TestScene(noc::Engine& engine)
{
	// Animate parent so hierarchy is obvious
	static float t = 0.0f;
	t += (float)noc::GetTime().DeltaSeconds();

	const float x = std::sinf(t) * 3.0f;
	engine.GetWorld().SetLocalTRS(gParent, noc::Vec3(x, 0, 5), noc::Quat::Identity(), noc::Vec3::One());

	gProbeX += (float)noc::GetTime().DeltaSeconds() * 20.0f; // move right
	if (gProbeX > 80.0f) gProbeX = -80.0f;
	engine.GetWorld().SetLocalTRS(gCullingProbe, noc::Vec3(gProbeX, 0.0f, 20.0f), noc::Quat::Identity(), noc::Vec3::One());

	// Toggle culling with C key (host-side debug input)
	static bool prevC = false;
	const bool nowC = (GetAsyncKeyState('C') & 0x8000) != 0;
	if (nowC && !prevC)
	{
		gCulling = !gCulling;
		engine.GetWorld().SetCullingEnabled(gCulling);
		NOC_LOG_INFO("Host", "Culling toggled: %s", gCulling ? "ON" : "OFF");

	}
	prevC = nowC;

	// Exit with ESC (host-side)
	if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
	{
		PostQuitMessage(0);
	}
}

int main()
{
	noc::Engine engine;
	engine.SetContentRoot(NOC_CONTENT_ROOT);

	if (!engine.Init())
		return -1;

	noc::WinWindow window;
	noc::WinWindowDesc wd{};
	wd.title = L"NocturneHost - Phase 10 Test";
	wd.width = 1280;
	wd.height = 720;
	wd.resizable = true;

	if (!engine.CreateAndAttachMainWindow(wd, window))
	{
		engine.Shutdown();
		return -1;
	}

	BuildPhase10TestScene(engine);
	NOC_LOG_INFO("Host", "Controls: C = toggle culling, ESC = quit");

	// message pump
	MSG msg{};
	bool running = true;
	while (running)
	{
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) { running = false; break; }
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		if (!running) break;

		engine.BeginFrame();
		UpdatePhase10TestScene(engine);
		engine.Tick();
		engine.EndFrame();
		static uint32_t sFrame = 0;
		if ((sFrame++ % 60) == 0)
		{
			const auto& stats = engine.GetWorld().GetLastStats();

			NOC_LOG_INFO("Host",
				"Visible: %u / Total: %u (Culling: %s)",
				stats.visible,
				stats.total,
				gCulling ? "ON" : "OFF");
		}

	}

	window.Destroy();
	engine.Shutdown();
	return 0;
}

