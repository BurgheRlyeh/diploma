#include "InputHandler.h"

void InputHandler::mouseRBPressed(bool isPressed, int x, int y) {
	isMRBPressed = isPressed;
	if (!isMRBPressed) {
		return;
	}

	prevMouseX = x;
	prevMouseY = y;
}

void InputHandler::mouseMoved(int x, int y) {
	if (!isMRBPressed) {
		return;
	}

	camera->rotate(
		static_cast<float>(x - prevMouseX),
		static_cast<float>(y - prevMouseY)
	);

	prevMouseX = x;
	prevMouseY = y;
}

void InputHandler::mouseWheel(int delta) {
	camera->zoom(static_cast<float>(delta));
}

void InputHandler::keyPressed(int keyCode) {
	switch (keyCode) {
	case ' ':
		renderer->switchRotation();
		break;

	case 'B':
	case 'b':
		renderer->updateBVH();
		break;

	default:
		processMovement(keyCode, 1.f);
	}
}

void InputHandler::keyReleased(int keyCode) {
	processMovement(keyCode, -1.f);
}

void InputHandler::processMovement(int keyCode, float sign) {
	switch (keyCode) {
	case 'W':
	case 'w':
		camera->moveForward(sign);
		break;

	case 'S':
	case 's':
		camera->moveForward(-sign);
		break;

	case 'D':
	case 'd':
		camera->moveRight(-sign);
		break;

	case 'A':
	case 'a':
		camera->moveRight(sign);
		break;
	}
}