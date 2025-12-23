/********************************************************/
/*  adlib.h                                             */
/*  Team 20850W, Accelerated Dragon                     */
/*  Copyright (C) 2025                                  */
/********************************************************/
#pragma once

#include "pros/adi.hpp"
#include "pros/screen.hpp"
#include "pros/misc.hpp"
#include "pros/rtos.hpp"
#include "pros/distance.hpp"

namespace adlib {
	enum {
		BUTTON_A		= pros::E_CONTROLLER_DIGITAL_A,
		BUTTON_B		= pros::E_CONTROLLER_DIGITAL_B,
		BUTTON_X		= pros::E_CONTROLLER_DIGITAL_X,
		BUTTON_Y		= pros::E_CONTROLLER_DIGITAL_Y,
		BUTTON_UP		= pros::E_CONTROLLER_DIGITAL_UP,
		BUTTON_DOWN		= pros::E_CONTROLLER_DIGITAL_DOWN,
		BUTTON_LEFT		= pros::E_CONTROLLER_DIGITAL_LEFT,
		BUTTON_RIGHT	= pros::E_CONTROLLER_DIGITAL_RIGHT,
		BUTTON_L1		= pros::E_CONTROLLER_DIGITAL_L1,
		BUTTON_L2		= pros::E_CONTROLLER_DIGITAL_L2,
		BUTTON_R1		= pros::E_CONTROLLER_DIGITAL_R1,
		BUTTON_R2		= pros::E_CONTROLLER_DIGITAL_R2
	};

	class Controller : public pros::Controller {
	public:
		Controller(pros::controller_id_e_t id);
		void start_task();

		bool is_button_pressed(int button);
		void button_pressed(int button, std::function<void()> callback);
		void button_released(int button, std::function<void()> callback);
		void button_process();

		void clear(int row = -1);
		void print(int row, int col, const char* fmt, ...);
		void rumble(const char* rumble_pattern);
		void print_process();

	private:
		pros::Task* controller_task = nullptr;
		static void controller_task_func(void* param);

		struct ButtonStruct {
			int id; // button id
			bool last_state = false; // last state of the button
			// Callbacks for button press and release
			std::function<void()> on_press = nullptr;
			std::function<void()> on_release = nullptr;
		};

		//Button, last_state, on_press, on_release
		std::vector<ButtonStruct> buttons {
			{ BUTTON_A,		false, nullptr, nullptr },
			{ BUTTON_B,		false, nullptr, nullptr },
			{ BUTTON_X,		false, nullptr, nullptr },
			{ BUTTON_Y,		false, nullptr, nullptr },
			{ BUTTON_UP,	false, nullptr, nullptr },
			{ BUTTON_DOWN,	false, nullptr, nullptr },
			{ BUTTON_LEFT,	false, nullptr, nullptr },
			{ BUTTON_RIGHT,	false, nullptr, nullptr },
			{ BUTTON_L1,	false, nullptr, nullptr },
			{ BUTTON_L2,	false, nullptr, nullptr },
			{ BUTTON_R1,	false, nullptr, nullptr },
			{ BUTTON_R2,	false, nullptr, nullptr }
		};
		const size_t num_of_buttons = buttons.size();

		static constexpr int MAX_NUM_OF_MSG = 8;
		static constexpr int MAX_MSG_LEN = 36;
		enum {	//first byte in the msg
			MSG_CLEAR = 1,
			MSG_RUMBLE = 2,
			MSG_TEXT = 3
		};

		char buf[MAX_NUM_OF_MSG][MAX_MSG_LEN];
		int rd_ptr = 0;
		int wr_ptr = 0;
		bool is_buf_full();
		bool is_buf_empty();
		void update_wr_ptr();
		void update_rd_ptr();
	};
}

namespace adlib {
	enum class Device {
		Motor,
		Imu,
		Optical,
		Rotation,
		Distance,
		Unknown
	};

	struct DeviceInfo {
		int port;
		Device type;
		std::string name;
	};

	enum Alignment {
		LEFT	= 0,
		RIGHT	= -1,
		TOP		= 0,
		BOTTOM	= -1,
		CENTER	= 65535
	};

	class Brain {
	public:
		Brain();
		std::string self_check(const std::vector<DeviceInfo>& devices);

		void initialize();
		void clear_screen(uint32_t color);
		void print(double row, double col, uint32_t color, const char* fmt, ...);
		void print(pros::text_format_e_t font, double row, double col, uint32_t color, const char* fmt, ...);
		void draw_image(const char* filename, int x = 0, int y = 0, int32_t bgcolor = 0xffffffff);
		void draw_line(int x1, int y1, int x2, int y2, uint32_t color = 0xffffffff);
		void pressed(std::function<bool()> callback);
		void released(std::function<bool()> callback);
		static Brain* instance;
		void touch_pressed_func();
		void touch_released_func();
		const char* get_timestamp(const char* d, const char* t);

		class Button {
		public:
			Button(const std::string& text, uint32_t color, uint32_t bgcolor, int x, int y, int w, int h, int radius = 3, bool big = false);
			void draw();
			void set_text(const char* t);
			void set_color(uint32_t c);
			void set_bgcolor(uint32_t bg);
			void pressed(std::function<void()> callback);
			void released(std::function<void()> callback);
			std::function<void()> on_press = nullptr;
			std::function<void()> on_release = nullptr;
			bool is_touched();

		private:
			int x1, y1, x2, y2, radius;
			bool big;
			std::string text;
			uint32_t color;
			uint32_t bgcolor;
		};

	private:
		bool check_device(int port, Device type);

		static constexpr int SCREEN_W = 480;
		static constexpr int SCREEN_H = 239;
		static constexpr int FONT_W = 10;
		static constexpr int FONT_H = 20;
		static constexpr int OFFSET_X = 0;
		static constexpr int OFFSET_Y = 2;
		std::function<bool()> on_press = nullptr;
		std::function<bool()> on_release = nullptr;
		std::vector<Button*> buttons;
	};
}

namespace adlib {
	class Distance : public pros::Distance {
	public:
		Distance(uint8_t port);
		double get_inches();
		double distance_to_wall();
	};

	class ADIDigitalOut : public pros::ADIDigitalOut {
	public:
		ADIDigitalOut(uint8_t port);
		void reverse(bool status);
		void press();
		void release();
		void toggle();
	private:
		bool current_value = false;
		bool is_reversed = false;
	};
}
