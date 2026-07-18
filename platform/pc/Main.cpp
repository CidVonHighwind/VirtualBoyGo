// PC entry point: runs the same OpenXrApp used on Android, streamed to the
// headset via whatever OpenXR runtime is currently registered (Virtual
// Desktop, SteamVR, Oculus Link, ...). No desktop window - see the
// Rework-branch session notes for why (VD/SteamVR both stream a runtime's
// compositor output directly; a mirror window is a later nice-to-have).
#include "app/OpenXrApp.h"
#include "desktop/DesktopPlatform.h"

#include <chrono>
#include <cstdio>
#include <thread>

int main()
{
    // Unbuffered stdout/stderr so logs show up immediately even when
    // redirected to a file (buffered-until-exit output made earlier
    // debugging in this session confusing).
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);

    // Ignore stdin EOF (e.g. when launched from a script/IDE with redirected
    // stdin) instead of treating it as an immediate quit request - learned
    // the hard way debugging hello_xr earlier this session.
    static bool quitRequested = false;
    std::thread exitPollingThread([]
                                  {
        std::printf("Press Enter to quit...\n");
        int ch = std::getchar();
        if (ch != EOF) {
            quitRequested = true;
        } });
    exitPollingThread.detach();

    std::printf("VirtualBoyGo PC starting...\n");

    DesktopPlatform platform;
    OpenXrApp app;
    try
    {
        OpenXrApp::InitInfo info;
        info.platform = &platform;
        app.Initialize(info);
        std::printf("VirtualBoyGo PC initialized OK\n");
    }
    catch (const std::exception &ex)
    {
        std::fprintf(stderr, "VirtualBoyGo: init failed: %s\n", ex.what());
        return 1;
    }

    bool requestRestart = false;
    int exitCode = 0;
    while (!quitRequested)
    {
        try
        {
            bool exitRenderLoop = false;
            app.PollEvents(exitRenderLoop, requestRestart);
            if (exitRenderLoop)
            {
                std::printf("VirtualBoyGo PC: exitRenderLoop requested\n");
                break;
            }

            if (app.IsSessionRunning())
            {
                app.RenderFrame();
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
        catch (const std::exception &ex)
        {
            std::fprintf(stderr, "VirtualBoyGo: render loop failed: %s\n", ex.what());
            exitCode = 1;
            break;
        }
    }

    app.Shutdown();
    std::printf("VirtualBoyGo PC exiting\n");
    return exitCode;
}
