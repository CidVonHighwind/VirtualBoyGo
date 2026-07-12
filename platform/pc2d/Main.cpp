// Flat desktop window - no OpenXR, no headset required at all. Draws the
// same AppMenu/UiRenderer content the composition-layer quad shows on the
// headset builds, presented into a normal window swapchain instead of an
// OpenXR session, driven by arrow keys/Enter/Escape instead of controller
// input. This is the fast local-iteration debug build the emulator/menu
// rendering will eventually show up in without needing to put the headset on.
#include "VulkanRenderer.h"
#include "AssetLoader.h"
#include "Emulator.h"
#include "ui/AppMenu.h"
#include "ui/ButtonMapping.h"
#include "ui/UiRenderer.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// Only need stbi_info_from_memory here, to size the window to the game
// image before a Vulkan device/surface exist (STB_IMAGE_IMPLEMENTATION is
// defined once in VulkanRenderer.cpp, same vbgo_app link unit).
#include "third_party/stb_image.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

    void CheckVk(VkResult result, const char *what)
    {
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error(std::string("Vulkan call failed: ") + what + " (" + std::to_string(result) + ")");
        }
    }

    void PollKeyboardButtonState(GLFWwindow *window, uint32_t buttonStates[3])
    {
        using namespace ButtonMapper;
        buttonStates[DeviceGamepad] = 0;
        buttonStates[DeviceLeftTouch] = 0;
        buttonStates[DeviceRightTouch] = 0;

        uint32_t &bits = buttonStates[DeviceRightTouch];
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_Up];
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_Down];
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_Left];
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_Right];
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_A];
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            bits |= ButtonMapping[EmuButton_B];
    }

    // VB gameplay input - separate key layout from menu navigation (the VB
    // controller has two D-pads plus A/B/L/R/Start/Select, more buttons than
    // the menu's 6). Bit positions match VBButtonBit (see Emulator.h).
    // Left D-pad: arrows. Right D-pad: WASD. A/B: X/Z (SNES-style layout).
    // L/R: Q/E. Start/Select: Enter/Backspace.
    uint32_t PollGameplayInput(GLFWwindow *window)
    {
        uint32_t bits = 0;
        auto setIf = [&](int key, uint32_t bit) {
            if (glfwGetKey(window, key) == GLFW_PRESS)
                bits |= (1u << bit);
        };
        setIf(GLFW_KEY_UP, VBButtonBit::LeftUp);
        setIf(GLFW_KEY_DOWN, VBButtonBit::LeftDown);
        setIf(GLFW_KEY_LEFT, VBButtonBit::LeftLeft);
        setIf(GLFW_KEY_RIGHT, VBButtonBit::LeftRight);
        setIf(GLFW_KEY_W, VBButtonBit::RightUp);
        setIf(GLFW_KEY_S, VBButtonBit::RightDown);
        setIf(GLFW_KEY_A, VBButtonBit::RightLeft);
        setIf(GLFW_KEY_D, VBButtonBit::RightRight);
        setIf(GLFW_KEY_X, VBButtonBit::A);
        setIf(GLFW_KEY_Z, VBButtonBit::B);
        setIf(GLFW_KEY_Q, VBButtonBit::L);
        setIf(GLFW_KEY_E, VBButtonBit::R);
        setIf(GLFW_KEY_ENTER, VBButtonBit::Start);
        setIf(GLFW_KEY_BACKSPACE, VBButtonBit::Select);
        return bits;
    }

} // namespace

int main()
{
    // Peek the game image's dimensions up front (pure file IO, no Vulkan
    // device needed yet) so its native resolution can size the window
    // before glfwCreateWindow - the window must exist before the Vulkan
    // instance/surface/device can be created, and Emulator::Initialize
    // (which actually uploads the texture) needs that device. Emulator
    // re-reads/re-decodes the same file itself once the device exists.
    const std::vector<uint8_t> gameImageBytes = LoadAssetBytes("game_image.png");
    int gameImageNativeWidth = 0, gameImageNativeHeight = 0;
    if (!gameImageBytes.empty())
    {
        int comp = 0;
        stbi_info_from_memory(gameImageBytes.data(), static_cast<int>(gameImageBytes.size()), &gameImageNativeWidth,
                              &gameImageNativeHeight, &comp);
    }
    else
    {
        std::fprintf(stderr, "VirtualBoyGo 2D: game_image.png not found next to the exe\n");
    }
    // Window is sized to exactly fit the upscaled game screen; the menu is
    // a smaller fixed-size (kMenuWidth*kMenuScale x kMenuHeight*kMenuScale
    // physical pixels - kMenuWidth/kMenuHeight alone are logical units, see
    // AppMenuLayout.h) panel composited (rounded corners and all) at a
    // centered offset within it, not the window's full size.
    const int windowWidth = gameImageNativeWidth > 0 ? gameImageNativeWidth * Emulator::kScale
                                                      : static_cast<int>(kMenuWidth * kMenuScale);
    const int windowHeight = gameImageNativeHeight > 0 ? gameImageNativeHeight * Emulator::kScale
                                                        : static_cast<int>(kMenuHeight * kMenuScale);

    if (!glfwInit())
    {
        std::fprintf(stderr, "VirtualBoyGo 2D: glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, "VirtualBoyGo (2D debug)", nullptr, nullptr);
    if (!window)
    {
        std::fprintf(stderr, "VirtualBoyGo 2D: glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }

    VulkanRenderer renderer;
    UiRenderer uiRenderer;
    Emulator emulator;
    AppMenu appMenu;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFence acquireFence = VK_NULL_HANDLE;
    int exitCode = 0;

    try
    {
        uint32_t glfwExtCount = 0;
        const char **glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
        const std::vector<const char *> instanceExtensions(glfwExts, glfwExts + glfwExtCount);

        VkInstance instance = renderer.CreateInstanceStandalone(instanceExtensions);
        CheckVk(glfwCreateWindowSurface(instance, window, nullptr, &surface), "glfwCreateWindowSurface");
        renderer.CreateDeviceForSurface(surface);

        // Prefer an sRGB surface format - matches the color-space handling
        // UiRenderer's image-loading path (UiRenderer::LoadImage) expects.
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.GetPhysicalDevice(), surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.GetPhysicalDevice(), surface, &formatCount, formats.data());
        VkSurfaceFormatKHR chosen = formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB} : formats[0];
        for (const auto &f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB || f.format == VK_FORMAT_R8G8B8A8_SRGB)
            {
                chosen = f;
                break;
            }
        }

        VkExtent2D extent{};
        std::vector<VkImage> swapchainImages;

        // (Re)creates the swapchain at the window's current framebuffer
        // size - called once up front and again whenever that size changes
        // (see the resize check in the render loop below).
        auto recreateSwapchain = [&]()
        {
            int fbWidth = 0, fbHeight = 0;
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            extent = {static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight)};

            vkDeviceWaitIdle(renderer.GetDevice());

            VkSwapchainCreateInfoKHR swapchainInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
            swapchainInfo.surface = surface;
            swapchainInfo.minImageCount = 2;
            swapchainInfo.imageFormat = chosen.format;
            swapchainInfo.imageColorSpace = chosen.colorSpace;
            swapchainInfo.imageExtent = extent;
            swapchainInfo.imageArrayLayers = 1;
            swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
            swapchainInfo.clipped = VK_TRUE;
            swapchainInfo.oldSwapchain = swapchain;

            VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
            CheckVk(vkCreateSwapchainKHR(renderer.GetDevice(), &swapchainInfo, nullptr, &newSwapchain),
                    "vkCreateSwapchainKHR");
            if (swapchain != VK_NULL_HANDLE)
                vkDestroySwapchainKHR(renderer.GetDevice(), swapchain, nullptr);
            swapchain = newSwapchain;

            // The old swapchain's images (and UiRenderer's per-image
            // framebuffer cache for them) are now invalid - a new swapchain
            // image can be handed back the same VkImage handle value, which
            // would otherwise hit a stale cache entry pointing at a
            // destroyed framebuffer/view (empty no-op before the first call,
            // since uiRenderer isn't initialized yet at that point).
            uiRenderer.InvalidateRenderTargets();

            uint32_t imageCount = 0;
            vkGetSwapchainImagesKHR(renderer.GetDevice(), swapchain, &imageCount, nullptr);
            swapchainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(renderer.GetDevice(), swapchain, &imageCount, swapchainImages.data());
        };
        recreateSwapchain();

        uiRenderer.Initialize(renderer.GetDevice(), renderer.GetPhysicalDevice(), renderer.GetQueue(),
                              renderer.GetQueueFamilyIndex(), renderer.GetCommandPool(), renderer.GetCommandBuffer());
        emulator.Initialize(uiRenderer);
        appMenu.Initialize(uiRenderer, chosen.format, emulator);

        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        CheckVk(vkCreateFence(renderer.GetDevice(), &fenceInfo, nullptr, &acquireFence), "vkCreateFence");

        std::printf("VirtualBoyGo 2D debug window running (%ux%u)\n", extent.width, extent.height);

        uint32_t buttonStates[3]{};
        uint32_t lastButtonStates[3]{};
        auto lastFrameTime = std::chrono::steady_clock::now();

        // No real battery to read on desktop - cycle a fake percentage
        // through the indicator instead, mostly so the battery block/text
        // rendering itself gets exercised without a headset. One full
        // 0-100 sweep every 10 seconds.
        float batteryCycleSeconds = 0.0f;

        // Tab toggles the menu open/closed - not part of buttonStates
        // (that's the menu-navigation/emulator button set, see
        // ButtonMapping.h) since this is an app-level concern AppMenu itself
        // doesn't read input for (see AppMenu::Show/Hide/ToggleOpen).
        // Edge-triggered so holding the key doesn't spam-toggle every frame.
        bool tabWasPressed = false;

        // TEMP debug: alternate eyes every second instead of always showing
        // Left - a quick visual check for whether the core's side-by-side
        // frame actually has different content per eye (if the picture
        // visibly changes each toggle, the data is real and any "flat in
        // the headset" bug is downstream of this - OpenXR quad layer
        // eyeVisibility handling, not the emulator/core).
        float eyeToggleSeconds = 0.0f;
        Emulator::Eye debugEye = Emulator::Eye::Left;

        while (!glfwWindowShouldClose(window))
        {
            glfwPollEvents();

            // Minimized (0x0 framebuffer) - a zero-extent swapchain is
            // invalid, so just wait for the window to become usable again
            // instead of spinning a render loop that can't present anything.
            int fbWidth = 0, fbHeight = 0;
            glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
            if (fbWidth == 0 || fbHeight == 0)
            {
                glfwWaitEvents();
                continue;
            }

            // Recreate the swapchain when the window has actually been
            // resized (cheap check - only rebuilds on an actual size change).
            if (static_cast<uint32_t>(fbWidth) != extent.width || static_cast<uint32_t>(fbHeight) != extent.height)
                recreateSwapchain();

            const auto now = std::chrono::steady_clock::now();
            const float deltaSeconds = std::chrono::duration<float>(now - lastFrameTime).count();
            lastFrameTime = now;

            const bool tabPressed = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
            if (tabPressed && !tabWasPressed)
                appMenu.ToggleOpen();
            tabWasPressed = tabPressed;

            std::memcpy(lastButtonStates, buttonStates, sizeof(buttonStates));
            PollKeyboardButtonState(window, buttonStates);
            appMenu.Update(buttonStates, lastButtonStates, deltaSeconds);
            // Only feed the game keyboard input while the menu is closed -
            // otherwise menu navigation (also arrow keys) would leak through
            // as gameplay input at the same time.
            emulator.SetGameplayInput(appMenu.IsOpen() ? 0 : PollGameplayInput(window));
            emulator.RunFrame(deltaSeconds);

            eyeToggleSeconds += deltaSeconds;
            if (eyeToggleSeconds >= 1.0f)
            {
                eyeToggleSeconds = 0.0f;
                debugEye = (debugEye == Emulator::Eye::Left) ? Emulator::Eye::Right : Emulator::Eye::Left;
                std::printf("[debug] showing %s eye\n", debugEye == Emulator::Eye::Left ? "LEFT" : "RIGHT");
            }

            batteryCycleSeconds += deltaSeconds;
            appMenu.SetBatteryPercent(static_cast<int>(std::fmod(batteryCycleSeconds * 10.0f, 100.0f)));

            // The menu renders at the largest integer logical-to-physical
            // scale (see AppMenuLayout.h's kMenuScale) that still fits the
            // current window, so it's always as big as possible without
            // ever needing to upscale (and blur) its offscreen texture.
            const int scaleX = static_cast<int>(fbWidth / kMenuWidth);
            const int scaleY = static_cast<int>(fbHeight / kMenuHeight);
            const float menuScale = static_cast<float>(std::max(1, std::min(scaleX, scaleY)));
            appMenu.SetMenuScale(uiRenderer, menuScale);
            const float menuX = (static_cast<float>(fbWidth) - kMenuWidth * menuScale) / 2.0f;
            const float menuY = (static_cast<float>(fbHeight) - kMenuHeight * menuScale) / 2.0f;

            if (appMenu.IsOpen())
                appMenu.RenderToBuffer(uiRenderer);

            vkResetFences(renderer.GetDevice(), 1, &acquireFence);
            uint32_t imageIndex = 0;
            const VkResult acquireResult = vkAcquireNextImageKHR(renderer.GetDevice(), swapchain, UINT64_MAX,
                                                                 VK_NULL_HANDLE, acquireFence, &imageIndex);
            if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
            {
                recreateSwapchain();
                continue;
            }
            if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
                continue;
            vkWaitForFences(renderer.GetDevice(), 1, &acquireFence, VK_TRUE, UINT64_MAX);

            // UiRenderer::EndFrame blocks internally (vkQueueWaitIdle) until
            // rendering is complete, so presenting right after is safe
            // without a rendering-finished semaphore.
            uiRenderer.BeginFrame(swapchainImages[imageIndex], chosen.format, extent.width, extent.height,
                                  appMenu.GetBackgroundColor());
            if (emulator.HasScreen())
            {
                // TEMP debug: alternates Left/Right every second (see
                // debugEye above) - normally this would just always be
                // Emulator::Eye::Left (flat window, no second eye to show
                // the other half to). TODO: make eye choice configurable
                // once the debug toggle is removed (see Emulator::Eye's doc
                // comment).
                emulator.DrawScreen(uiRenderer, 0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height),
                                    debugEye);
            }
            if (appMenu.IsOpen())
                appMenu.Draw(uiRenderer, menuX, menuY);
            uiRenderer.EndFrame();

            VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &swapchain;
            presentInfo.pImageIndices = &imageIndex;
            const VkResult presentResult = vkQueuePresentKHR(renderer.GetQueue(), &presentInfo);
            if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
                recreateSwapchain();
        }

        vkDeviceWaitIdle(renderer.GetDevice());
    }
    catch (const std::exception &ex)
    {
        std::fprintf(stderr, "VirtualBoyGo 2D: %s\n", ex.what());
        exitCode = 1;
    }

    if (acquireFence != VK_NULL_HANDLE)
        vkDestroyFence(renderer.GetDevice(), acquireFence, nullptr);
    if (swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(renderer.GetDevice(), swapchain, nullptr);
    // Surface must be destroyed before the instance - renderer.Shutdown()
    // destroys the instance, so this has to happen first. uiRenderer also
    // owns Vulkan resources backed by renderer's device, so it must go first.
    if (surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(renderer.GetInstance(), surface, nullptr);
    uiRenderer.Shutdown();
    renderer.Shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
    return exitCode;
}
