#pragma once

#include "framework.h"

#include "Renderer.h"
#include "Camera.h"

class Renderer;
class Camera;

class InputHandler {
	Renderer* renderer;
	Camera* camera;

	bool isMRBPressed{};
	int prevMouseX{};
	int prevMouseY{};

public:
	InputHandler() = delete;
	InputHandler(Renderer* renderer, Camera* camera) :
		renderer(renderer),
		camera(camera)
	{}

	void mouseRBPressed(bool isPressed, int x, int y);
	void mouseMoved(int x, int y);
	void mouseWheel(int delta);

	void keyPressed(int keyCode);
	void keyReleased(int keyCode);

private:
	void processMovement(int keyCode, float sign);
};
