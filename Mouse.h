#ifndef MOUSE_CLASS
#define MOUSE_CLASS

#include <bitset>
#include <queue>
#include <optional>

class Mouse {
	friend class Window;
public:
	//EVENT CLASS
	class Event {
	public:
		enum class Type {
			LPress,
			LRelease,
			RPress,
			RRelease,
			WheelUp,
			WheelDown,
			Move,
			Invalid,
			Enter,
			Exit
		};
		Event() noexcept : type(Type::Invalid), LeftIsPressed(false), RightIsDown(false), x(0), y(0) {}
		Event(Type t, const Mouse& parent) noexcept : type(t), LeftIsPressed(parent.LeftIsDown), RightIsDown(parent.RightIsDown), x(parent.X), y(parent.Y) {}
		bool const IsValid() const noexcept {
			return type != Type::Invalid;
		}
		Type GetType() noexcept {
			return type;
		}
		bool IsLeftDown() noexcept {
			return LeftIsPressed;
		}

		bool IsRightDown() noexcept {
			return RightIsDown;
		}
		int GetPosX() noexcept {
			return x;
		}
		int GetPosY() noexcept {
			return y;
		}
		std::pair<int, int> GetPos() noexcept {
			return { x,y };
		}

		explicit operator bool() const {
			return IsValid();
		}
	private:
		Type type;
		int x;
		int y;
		bool LeftIsPressed;
		bool RightIsDown;
	};

public:
	Mouse() = default;
	Mouse(const Mouse&) = delete;
	Mouse& operator = (const Mouse&) = delete;

	//I <3 HARDCODING

	std::pair<unsigned int, unsigned int> GetPos() {
		return { X,Y };
	};
	int GetX() const noexcept {
		return X;
	}
	int GetY() const noexcept {
		return Y;
	}
	int GetLastX() const noexcept {
		return lastX;
	}
	int GetLastY() const noexcept {
		return lastY;
	}

	bool MouseIsInWindow() const noexcept {
		return IsInWindow;
	};
	bool IsLeftDown() const noexcept {
		return LeftIsDown;
	};
	bool IsRightDown() const noexcept {
		return RightIsDown;
	};

	Mouse::Event Read() noexcept;
public:
	//yummy
	void OnMouseMove(unsigned int x, unsigned int y) noexcept;
	void MouseEnter() noexcept;
	void MouseExit() noexcept;
	void OnWheelDelta(int x, int y, int delta) noexcept;
	void WheelUp(int x, int y) noexcept;
	void WheelDown(int x, int y) noexcept;
	void LeftDown() noexcept;
	void RightDown() noexcept;
	void LeftUp() noexcept;
	void RightUp() noexcept;
private:
	void Flush() noexcept {
		buffer = std::queue<Event>();
	};
	void TrimBuffer() noexcept {
		while (buffer.size() > bufferSize) {
			buffer.pop();
		}
	}

	bool RightIsDown = false;
	bool LeftIsDown = false;
	static constexpr unsigned int bufferSize = 16u;
	int X = 0;
	int Y = 0;
	int lastX = 0;
	int lastY = 0;
	int wheeldeltacarry = 0;
	std::queue<Event> buffer;
	bool IsInWindow = false;
};

#endif