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
#include "Dependencies/freeglut.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <cctype>
#include <cmath>

namespace
{
    std::unique_ptr<Renderer> renderer;
    Prototype game;
    HWND gameWindow = nullptr;
    FrameProfiler profiler;
    using FrameClock = std::chrono::steady_clock;
    auto lastFrame = FrameClock::now();
    auto nextTick = lastFrame;
    const auto framePeriod =
        std::chrono::duration_cast<FrameClock::duration>(std::chrono::duration<double>(1.0 / 60.0));

    void Display()
    {
        if (!renderer)
        {
            return;
        }
        profiler.BeginFrame();
        game.Draw(*renderer);
        profiler.RenderFinished();
        glutSwapBuffers();
        profiler.EndFrame(*renderer, game);
    }

    void Tick(int)
    {
        if (!renderer)
        {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame = now;
        const auto updateStart = std::chrono::steady_clock::now();
        // Poll both Shift keys independently of letter events: pressing/releasing
        // Shift while already moving must take effect, including after focus loss.
        game.SetRunHeld(gameWindow && GetForegroundWindow() == gameWindow
                        && (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
        game.Update((std::min)(0.05f, elapsed));
        profiler.RecordUpdate(std::chrono::duration<double, std::milli>(
                                  std::chrono::steady_clock::now() - updateStart)
                                  .count(),
                              game);
        glutPostRedisplay();
        // Reserve against an absolute deadline so update work does not add a
        // fresh 16 ms delay each tick. Missed deadlines are skipped, not replayed.
        nextTick += framePeriod;
        const auto finished = FrameClock::now();
        if (nextTick <= finished)
        {
            nextTick = finished + framePeriod;
        }
        const double remaining =
            std::chrono::duration<double, std::milli>(nextTick - finished).count();
        glutTimerFunc(static_cast<unsigned>((std::max)(1.0, std::ceil(remaining))), Tick, 0);
    }

    void Reshape(int width, int height)
    {
        if (renderer)
        {
            renderer->Resize(width, height);
        }
    }

    void KeyDown(unsigned char key, int, int)
    {
        if (key == 27)
        {
            glutLeaveMainLoop();
            return;
        }
        if (renderer)
        {
            auto& post = renderer->Post().settings;
            switch (std::tolower(key))
            {
            case 'p':
                post.enabled = !post.enabled;
                return;
            case 'b':
                post.bloom = !post.bloom;
                return;
            case 'v':
                post.vignette = !post.vignette;
                return;
            case 'n':
                post.edgeBlur = !post.edgeBlur;
                return;
            case '[':
                post.exposure = (std::max)(0.25f, post.exposure - 0.1f);
                return;
            case ']':
                post.exposure = (std::min)(3.0f, post.exposure + 0.1f);
                return;
            case '0':
                post = PostProcessSettings{};
                return;
            }
        }
        game.Key(key, true);
    }

    void KeyUp(unsigned char key, int, int)
    {
        game.Key(key, false);
    }

    void Close()
    {
        // The close callback runs while the OpenGL context is still current.
        renderer.reset();
    }

    void Entry(int state)
    {
        if (state == GLUT_LEFT)
        {
            game.SetRunHeld(false);
            for (int key = 0; key < 256; ++key)
            {
                game.Key(static_cast<unsigned char>(key), false);
            }
        }
    }
} // namespace

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(1280, 800);
    glutInitWindowPosition(70, 45);
    const int window = glutCreateWindow("The Last Ember - 2.5D Prototype");
    gameWindow = GetActiveWindow();
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glewExperimental = GL_TRUE;
    const GLenum result = glewInit();
    if (result != GLEW_OK || !GLEW_VERSION_3_3)
    {
        std::cerr << "OpenGL 3.3 is required. GLEW: " << glewGetErrorString(result) << "\n";
        glutDestroyWindow(window);
        return 1;
    }
    renderer = std::make_unique<Renderer>(1280, 800);
    if (!renderer->IsInitialized())
    {
        std::cerr << "Renderer initialization failed. See shader diagnostics above.\n";
        renderer.reset();
        glutDestroyWindow(window);
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
    glutIgnoreKeyRepeat(1);
    glutDisplayFunc(Display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(KeyDown);
    glutKeyboardUpFunc(KeyUp);
    glutEntryFunc(Entry);
    glutCloseFunc(Close);
    game.Update(0);
    lastFrame = FrameClock::now();
    nextTick = lastFrame;
    glutTimerFunc(0, Tick, 0);
    glutMainLoop();
    // Freeglut closes its windows before returning from the main loop.
    return 0;
}
