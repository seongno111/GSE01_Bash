/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/
#include "stdafx.h"
#include "Renderer.h"
#include "Prototype.h"
#include "FrameProfiler.h"
#define GLFW_INCLUDE_NONE
#define GLFW_DLL
#include "Dependencies/GLFW/include/GLFW/glfw3.h"
#include <Windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>

namespace
{
    using FrameClock = std::chrono::steady_clock;
    const auto framePeriod =
        std::chrono::duration_cast<FrameClock::duration>(std::chrono::duration<double>(1.0 / 60.0));

    std::unique_ptr<Renderer> renderer;
    Prototype game;
    FrameProfiler profiler;

    void GlfwError(int code, const char* message)
    {
        std::cerr << "GLFW error " << code << " - " << message << '\n';
    }

    void KeyChanged(GLFWwindow* window, int key, int, int action, int)
    {
        if (action == GLFW_REPEAT)
        {
            return;
        }

        const bool pressed = action == GLFW_PRESS;
        if (pressed)
        {
            auto& post = renderer->Post().settings;
            switch (key)
            {
            case GLFW_KEY_ESCAPE:
                glfwSetWindowShouldClose(window, GLFW_TRUE);
                return;
            case GLFW_KEY_P:
                post.enabled = !post.enabled;
                return;
            case GLFW_KEY_B:
                post.bloom = !post.bloom;
                return;
            case GLFW_KEY_V:
                post.vignette = !post.vignette;
                return;
            case GLFW_KEY_N:
                post.edgeBlur = !post.edgeBlur;
                return;
            case GLFW_KEY_LEFT_BRACKET:
                post.exposure = (std::max)(0.25f, post.exposure - 0.1f);
                return;
            case GLFW_KEY_RIGHT_BRACKET:
                post.exposure = (std::min)(3.0f, post.exposure + 0.1f);
                return;
            case GLFW_KEY_0:
            case GLFW_KEY_KP_0:
                post = PostProcessSettings{};
                return;
            }
        }

        // GLFW emits release events for held keys when the window loses focus.
        if (key == GLFW_KEY_KP_ADD)
        {
            game.Key('+', pressed);
            return;
        }
        if (key == GLFW_KEY_KP_SUBTRACT || key == GLFW_KEY_MINUS)
        {
            const bool zoomOutHeld = glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS
                                     || glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS;
            game.Key('-', zoomOutHeld);
            return;
        }
        if (key >= GLFW_KEY_SPACE && key <= GLFW_KEY_GRAVE_ACCENT)
        {
            game.Key(static_cast<unsigned char>(key), pressed);
        }
    }

    void RunFrames(GLFWwindow* window, int width, int height)
    {
        // Win32 event waits use the system timer resolution.
        const bool preciseTimer = timeBeginPeriod(1) == TIMERR_NOERROR;
        auto lastFrame = FrameClock::now();
        auto nextFrame = lastFrame + framePeriod;

        while (!glfwWindowShouldClose(window))
        {
            const auto now = FrameClock::now();
            if (now < nextFrame)
            {
                // GLFW truncates the Win32 timeout to milliseconds. Round up to avoid spinning.
                const auto delay = std::chrono::ceil<std::chrono::milliseconds>(nextFrame - now);
                glfwWaitEventsTimeout((delay.count() + 0.5) / 1000.0);
                continue;
            }

            glfwPollEvents();
            if (glfwWindowShouldClose(window))
            {
                break;
            }

            int pixelWidth;
            int pixelHeight;
            glfwGetFramebufferSize(window, &pixelWidth, &pixelHeight);
            if (pixelWidth <= 0 || pixelHeight <= 0 || glfwGetWindowAttrib(window, GLFW_ICONIFIED))
            {
                glfwWaitEvents();
                lastFrame = FrameClock::now();
                nextFrame = lastFrame;
                continue;
            }

            if (pixelWidth != width || pixelHeight != height)
            {
                width = pixelWidth;
                height = pixelHeight;
                renderer->Resize(width, height);
            }

            const auto updateStart = FrameClock::now();
            const float elapsed = std::chrono::duration<float>(updateStart - lastFrame).count();
            lastFrame = updateStart;

            // Keep the 60 Hz deadline and skip missed slots after an overrun.
            nextFrame += framePeriod;
            if (nextFrame <= updateStart)
            {
                nextFrame = updateStart + framePeriod;
            }

            game.SetRunHeld(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                            || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
            game.Update((std::min)(0.05f, elapsed));
            profiler.RecordUpdate(
                std::chrono::duration<double, std::milli>(FrameClock::now() - updateStart).count(),
                game);

            profiler.BeginFrame();
            game.Draw(*renderer);
            profiler.RenderFinished();
            glfwSwapBuffers(window);
            profiler.EndFrame(*renderer, game);
        }

        if (preciseTimer)
        {
            timeEndPeriod(1);
        }
    }
} // namespace

int main()
{
    glfwSetErrorCallback(GlfwError);
    if (!glfwInit())
    {
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window =
        glfwCreateWindow(1280, 800, "The Last Ember - 2.5D Prototype", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    glfwSetWindowPos(window, 70, 45);
    glfwMakeContextCurrent(window);
    // The frame loop owns pacing. Avoid a second limit at buffer swap.
    glfwSwapInterval(0);

    glewExperimental = GL_TRUE;
    const GLenum result = glewInit();
    if (result != GLEW_OK || !GLEW_VERSION_3_3)
    {
        std::cerr << "OpenGL 3.3 is required. GLEW: " << glewGetErrorString(result) << "\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    renderer = std::make_unique<Renderer>(width, height);
    if (!renderer->IsInitialized())
    {
        std::cerr << "Renderer initialization failed. See shader diagnostics above.\n";
        renderer.reset();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }

    profiler.Initialize(*renderer);

    std::cout << "The Last Ember / rendering prototype\n"
              << "WASD move | Hold Shift run | Hold Space auto aim | F auto fire | U upgrade | Q "
                 "potion | T "
                 "cleansing stone\n"
              << "E sanctuary/report | R relic | K demo death\n"
              << "NPC: C select | I inspector | J PK | Y trade | O blessing | L AI log\n"
              << "H help | G chunk grid | +/- zoom | Esc exit\n"
              << "P post FX | B bloom | V vignette | N edge blur | [/] exposure | 0 reset FX\n"
              << "Session only: no gameplay saves, no AI dialogue service.\n"
              << "Mesh files: MeshCache/ | Performance captures: Profiling/\n"
              << "[Profile] Logs every second: FPS and glDraw calls per frame "
                 "(scene + UI + post processing; last/avg/min/max).\n";
    glfwSetKeyCallback(window, KeyChanged);
    game.Update(0);
    RunFrames(window, width, height);

    // Release GPU resources while their context is current.
    renderer.reset();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
