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
#include "Dependencies/freeglut.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <cctype>

namespace {
    std::unique_ptr<Renderer> renderer;
    Prototype game;
    auto lastFrame = std::chrono::steady_clock::now();

    void Display() {
        if (!renderer) return;
        game.Draw(*renderer);
        glutSwapBuffers();
    }
    void Tick(int) {
        if (!renderer) return;
        const auto now = std::chrono::steady_clock::now();
        const float elapsed = std::chrono::duration<float>(now-lastFrame).count();
        lastFrame = now;
        game.Update((std::min)(0.05f, elapsed));
        glutPostRedisplay();
        glutTimerFunc(16, Tick, 0);
    }
    void Reshape(int width, int height) {
        if (renderer) renderer->Resize(width, height);
    }
    void KeyDown(unsigned char key, int, int) {
        if (key == 27) { glutLeaveMainLoop(); return; }
        if (renderer) {
            auto& post = renderer->Post().settings;
            switch (std::tolower(key)) {
            case 'p': post.enabled = !post.enabled; return;
            case 'b': post.bloom = !post.bloom; return;
            case 'v': post.vignette = !post.vignette; return;
            case 'n': post.edgeBlur = !post.edgeBlur; return;
            case '[': post.exposure = (std::max)(0.25f, post.exposure-0.1f); return;
            case ']': post.exposure = (std::min)(3.0f, post.exposure+0.1f); return;
            case '0': post = PostProcessSettings{}; return;
            }
        }
        game.Key(key,true);
    }
    void KeyUp(unsigned char key, int, int) { game.Key(key,false); }
    void Close() {
        // The close callback runs while the OpenGL context is still current.
        renderer.reset();
    }
    void Entry(int state) {
        if (state == GLUT_LEFT)
            for (int key = 0; key < 256; ++key) game.Key(static_cast<unsigned char>(key),false);
    }
}

int main(int argc, char** argv) {
    glutInit(&argc,argv);
    glutInitContextVersion(3,3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(1280,800);
    glutInitWindowPosition(70,45);
    const int window = glutCreateWindow("The Last Ember - 2.5D Prototype");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glewExperimental = GL_TRUE;
    const GLenum result = glewInit();
    if (result != GLEW_OK || !GLEW_VERSION_3_3) {
        std::cerr << "OpenGL 3.3 is required. GLEW: " << glewGetErrorString(result) << "\n";
        glutDestroyWindow(window);
        return 1;
    }
    renderer = std::make_unique<Renderer>(1280,800);
    if (!renderer->IsInitialized()) {
        std::cerr << "Renderer initialization failed. See shader diagnostics above.\n";
        renderer.reset();
        glutDestroyWindow(window);
        return 1;
    }
    std::cout << "The Last Ember / rendering prototype\n"
        << "WASD move | Space strike | E cleanse | R relic | K demo death\n"
        << "H help | G chunk grid | +/- zoom | Esc exit\n"
        << "P post FX | B bloom | V vignette | N edge blur | [/] exposure | 0 reset FX\n"
        << "Session only: no disk saves, no AI dialogue service.\n";
    glutIgnoreKeyRepeat(1);
    glutDisplayFunc(Display);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(KeyDown);
    glutKeyboardUpFunc(KeyUp);
    glutEntryFunc(Entry);
    glutCloseFunc(Close);
    game.Update(0);
    lastFrame = std::chrono::steady_clock::now();
    glutTimerFunc(16,Tick,0);
    glutMainLoop();
    // Freeglut closes its windows before returning from the main loop.
    return 0;
}
