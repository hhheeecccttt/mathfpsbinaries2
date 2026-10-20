#include "Mouse.h"

void Mouse::OnMouseMove(unsigned int x, unsigned int y) noexcept
{
    lastX = X;
    lastY = Y;
    X = x;
    Y = y;
    buffer.push(Mouse::Event(Event::Type::Move, *this));
    TrimBuffer();
}

void Mouse::MouseEnter() noexcept
{
    IsInWindow = true;
    buffer.push(Mouse::Event(Event::Type::Enter, *this));
    TrimBuffer();
}

void Mouse::MouseExit() noexcept
{
    IsInWindow = false;
    buffer.push(Mouse::Event(Event::Type::Exit, *this));
    TrimBuffer();
}

void Mouse::RightDown() noexcept
{
    RightIsDown = true;
    buffer.push(Mouse::Event(Event::Type::RPress, *this));
    TrimBuffer();
}

void Mouse::RightUp() noexcept
{
    RightIsDown = false;
    buffer.push(Mouse::Event(Event::Type::RRelease, *this));
    TrimBuffer();
}

void Mouse::LeftDown() noexcept
{
    LeftIsDown = true;
    buffer.push(Mouse::Event(Event::Type::LPress, *this));
    TrimBuffer();
}

void Mouse::LeftUp() noexcept
{
    LeftIsDown = false;
    buffer.push(Mouse::Event(Event::Type::LRelease, *this));
    TrimBuffer();
}

void Mouse::WheelUp(int x, int y) noexcept {
    buffer.push(Mouse::Event(Mouse::Event::Type::WheelUp, *this));
    TrimBuffer();
}

void Mouse::WheelDown(int x, int y) noexcept {
    buffer.push(Mouse::Event(Mouse::Event::Type::WheelDown, *this));
    TrimBuffer();
}

void Mouse::OnWheelDelta(int x, int y, int delta) noexcept {
    wheeldeltacarry += delta;
    while (wheeldeltacarry >= 120) {
        wheeldeltacarry -= 120;
        WheelUp(x, y);
    }
    while (wheeldeltacarry <= -120) {
        wheeldeltacarry += 120;
        WheelDown(x, y);
    }
}

Mouse::Event Mouse::Read() noexcept {
    if (buffer.size() > 0u) {
        Mouse::Event e = buffer.front();
        buffer.pop();
        return e;
    }
    else {
        return Mouse::Event();
    }
}