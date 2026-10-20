
#ifndef KEYBOARD_CLASS
#define KEYBOARD_CLASS

#include <bitset>
#include <queue>

class Keyboard {
public:
	//EVENT CLASS
	class Event {
	public:
		enum class Type {
			Press,
			Release,
			Invalid
		};

		Event() : type(Type::Invalid), code(0) {}
		Event(Type typ, int cod) : type(typ), code(cod) {}
		bool IsPress() const noexcept {
			return type == Type::Press;
		};
		bool IsRelease() const noexcept {
			return type == Type::Release;
		};
		bool IsValid() const noexcept {
			return type != Type::Invalid;
		};

		int GetCode() const noexcept {
			return code;
		}

		explicit operator bool() const {
			return IsValid();
		}
	private:
		Type type;
		int code;
	};

public:
	Keyboard() = default;
	~Keyboard() = default;
	Keyboard(const Keyboard&) = delete;

	bool IsKeyDown(const int key) const noexcept {
		return key >= 0 && key < (int)numerodekeys && keys[key];
	}

	bool IsKeyEmpty() const noexcept {
		return keybuffer.empty();
	}
	bool IsCharEmpty() const noexcept {
		return charbuffer.empty();
	}
	unsigned char ReadChar() noexcept {
		if (charbuffer.size() > 0u) {
			unsigned char e = charbuffer.front();
			charbuffer.pop();
			return e;
		}
		else {
			return 0;
		}
	}


	Event ReadKey() noexcept {
		if (keybuffer.size() > 0u) {
			Event e = keybuffer.front();
			keybuffer.pop();
			return e;
		}
		else {
			return Event();
		}
	}

	void EnableAutorepeat() noexcept {
		AutorepeatEnabled = true;
	}
	void DisableAutorepeat() noexcept {
		AutorepeatEnabled = false;
	}
	bool IsAutorepeatEnabled() const noexcept {
		return AutorepeatEnabled;
	}
	void Char(unsigned char key) {
		charbuffer.push(key);
		TrimBuffer(charbuffer);
	}

	void KeyDown(const int key) noexcept {
		if (key < 0 || key >= (int)numerodekeys) return;  // GLFW_KEY_UNKNOWN etc.
		keybuffer.push(Event(Event::Type::Press, key));
		keys[key] = true;
		TrimBuffer(keybuffer);
	}
	void KeyUp(const int key) noexcept {
		if (key < 0 || key >= (int)numerodekeys) return;
		keybuffer.push(Event(Event::Type::Release, key));
		keys[key] = false;
		TrimBuffer(keybuffer);
	}

	void ClearKeyboard() noexcept {
		keys.reset();
	}

	void FlushKey() noexcept {
		keybuffer = std::queue<Event>();
	}
	void FlushChar() noexcept {
		charbuffer = std::queue<char>();
	}
	void Flush() {
		FlushKey();
		FlushChar();
	}

private:
	std::queue<Event> keybuffer;
	std::queue<char> charbuffer;

	template<typename T> void TrimBuffer(std::queue<T>& q) noexcept {
		while (q.size() > bufferSize) {
			q.pop();
		}
	}

	bool AutorepeatEnabled = false;
	static constexpr unsigned int numerodekeys = 512u;  // GLFW key codes go up to ~348
	static constexpr unsigned int bufferSize = 16u;
	std::bitset<numerodekeys> keys;

};

#endif // !KEYBOARD_CLASS
