/********************************************************/
/*  adlib.cpp                                           */
/*  Team 20850W, Accelerated Dragon                     */
/*  Copyright (C) 2025                                  */
/********************************************************/
#include "adlib.h"

#include "pros/motors.hpp"
#include "pros/imu.hpp"
#include "pros/optical.hpp"
#include "pros/rotation.hpp"

/********************************************************/
/* Controller                                           */
/********************************************************/
namespace adlib {
	Controller::Controller(pros::controller_id_e_t id)
		: pros::Controller(id) {
	}

	// Start the controller task
	void Controller::start_task() {
		if (controller_task == nullptr) {
			controller_task = new pros::Task([this]() {
				int disp_cnt = 0;
				while (true) {
					button_process();
					if(disp_cnt == 0)
						print_process();

					disp_cnt = (disp_cnt + 1) % 2;  //disp_cnt becomes 0 every 50 msec
					pros::delay(25);
				}
			});
		}
	}

	// Check if a button is currently pressed
	bool Controller::is_button_pressed(int button) {
		return get_digital(static_cast<pros::controller_digital_e_t>(button));
	}

	// Register a callback for when a button is pressed
	void Controller::button_pressed(int button, std::function<void()> callback) {
		for (int i = 0; i < num_of_buttons; i++) {
			if (buttons[i].id == button) {
				buttons[i].on_press = callback;
				return;
			}
		}
	}

	// Register a callback for when a button is released
	void Controller::button_released(int button, std::function<void()> callback) {
		for (int i = 0; i < num_of_buttons; i++) {
			if (buttons[i].id == button) {
				buttons[i].on_release = callback;
				return;
			}
		}
	}

	// Process button states and trigger callbacks
	void Controller::button_process() {
		for (int i = 0; i < num_of_buttons; i++) {
			bool state = is_button_pressed(buttons[i].id);

			if (state && !buttons[i].last_state) { // Button was just pressed
				if (buttons[i].on_press != nullptr) {
					buttons[i].on_press();
				}
				buttons[i].last_state = state;
			}
			else if (!state && buttons[i].last_state) { // Button was just released
				if (buttons[i].on_release != nullptr) {
					buttons[i].on_release();
				}
				buttons[i].last_state = state;
			}
		}
	}

	bool Controller::is_buf_full() {
		return (wr_ptr + 1) % MAX_NUM_OF_MSG == rd_ptr;
	}

	bool Controller::is_buf_empty() {
		return rd_ptr == wr_ptr;
	}

	void Controller::update_wr_ptr() {
		wr_ptr = (wr_ptr + 1) % MAX_NUM_OF_MSG;
	}

	void Controller::update_rd_ptr() {
		rd_ptr = (rd_ptr + 1) % MAX_NUM_OF_MSG;
	}

	void Controller::clear(int row) {
		if(is_buf_full())
			return;

		if(row == -1) {	//clear all
			buf[wr_ptr][0] = MSG_CLEAR;
			update_wr_ptr();
		}
		else {	//clear a single row
			print(row, 0, "%28s", "");
		}
	}

	void Controller::print(int row, int col, const char* fmt, ...) {
		if(is_buf_full())
			return;

		buf[wr_ptr][0] = MSG_TEXT;
		buf[wr_ptr][1] = char(row);
		buf[wr_ptr][2] = char(col);
		va_list args;
		va_start(args, fmt);
		vsnprintf(&(buf[wr_ptr][3]), MAX_MSG_LEN-4, fmt, args);
		va_end(args);
		update_wr_ptr();
	}

	void Controller::rumble(const char* rumble_pattern) {
		if(is_buf_full())
			return;

		buf[wr_ptr][0] = MSG_RUMBLE;
		snprintf(&(buf[wr_ptr][1]), MAX_MSG_LEN-1, "%s", rumble_pattern);
		update_wr_ptr();
	}

	void Controller::print_process() {
		if(is_buf_empty())
			return;

		if (buf[rd_ptr][0] == MSG_CLEAR) {
			pros::Controller::clear();
		}
		else if (buf[rd_ptr][0] == MSG_RUMBLE) {
			char* pattern = &(buf[rd_ptr][1]);
			pros::Controller::rumble(pattern);
		}
		else if (buf[rd_ptr][0] == MSG_TEXT) {
			int row = (int)buf[rd_ptr][1];
			int col = (int)buf[rd_ptr][2];
			char* msg = &(buf[rd_ptr][3]);
			pros::Controller::print(row, col, msg);
		}
		update_rd_ptr();
	}
}

/********************************************************/
/* Brain                                                */
/********************************************************/
namespace adlib {
	Brain* Brain::instance = nullptr;

	Brain::Brain() {
		Brain::instance = this;
	}

	// Check if a device is connected
	bool Brain::check_device(int port, Device type) {
		switch (type) {
			case Device::Motor:
				return pros::Motor(port).is_installed();
			case Device::Imu:
				return pros::Imu(port).is_installed();
			case Device::Optical:
				return pros::Optical(port).is_installed();
			case Device::Rotation:
				return pros::Rotation(port).is_installed();
			case Device::Distance:
				return pros::Distance(port).is_installed();
			default:
				return false;
		}
	}

	// Perform self-check on a list of devices
	std::string Brain::self_check(const std::vector<DeviceInfo>& devices) {
		for(int i=0; i<devices.size(); i++) {
			int port = abs(devices[i].port);
			if(!check_device(port, devices[i].type))
				return "Err: [" + std::to_string(port) + "] " + devices[i].name;
		}
		return "";
	}

	// Initialize the brain screen
	void Brain::initialize() {
		pros::delay(50);
		clear_screen(0x000000);
		pros::screen::set_pen(0xffffff);
		pros::delay(50);

		pros::screen::touch_callback([]() {
			Brain* self = Brain::instance;
			self->touch_pressed_func();
		}, pros::E_TOUCH_PRESSED);

		pros::screen::touch_callback([]() {
			Brain* self = Brain::instance;
			self->touch_released_func();
		}, pros::E_TOUCH_RELEASED);

		for(int i=0; i<buttons.size(); i++) {
			buttons[i]->draw();
		}
		pros::delay(50);
	}

	// touch pressed callback, execute the whole screen or button callbacks
	void Brain::touch_pressed_func() {
		if(on_press != nullptr) {
			if(!on_press()) {	// If the callback returns false, do not check buttons
				return;
			}
		}

		for(int i=0; i<buttons.size(); i++) {
			if(buttons[i]->is_touched()) {
				if(buttons[i]->on_press != nullptr) {
					buttons[i]->on_press();
				}
				return;
			}
		}
	}

	// touch released callback, excute the whole screen or button callbacks
	void Brain::touch_released_func() {
		if(on_release != nullptr) {
			if(!on_release()) {	// If the callback returns false, do not check buttons
				return;
			}
		}

		for(int i=0; i<buttons.size(); i++) {
			if(buttons[i]->is_touched()) {
				if(buttons[i]->on_release != nullptr) {
					buttons[i]->on_release();
				}
				return;
			}
		}
	}

	// Clear the brain screen with a specific color
	void Brain::clear_screen(uint32_t color) {
		pros::screen::set_eraser(color);
		pros::screen::erase_rect(0, 0, SCREEN_W, SCREEN_H);
	}

	// Print formatted text to the brain screen at a specific row and column with a specific color
	void Brain::print(double row, double col, uint32_t color, const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		char buf[128];
		vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);

		int x = (int)(col * FONT_W + OFFSET_X);
		int y = (int)(row * FONT_H + OFFSET_Y);
		pros::screen::set_pen(color);
		pros::screen::print(pros::E_TEXT_MEDIUM, x, y, buf);
	}

	void Brain::print(pros::text_format_e_t font, double row, double col, uint32_t color, const char* fmt, ...) {
		va_list args;
		va_start(args, fmt);
		char buf[128];
		vsnprintf(buf, sizeof(buf), fmt, args);
		va_end(args);

		int x = (int)(col * FONT_W*2 + OFFSET_X);
		int y = (int)(row * FONT_H*1.6 + OFFSET_Y);
		pros::screen::set_pen(color);
		pros::screen::print(font, x, y, buf);
	}

	// Draw an image from a file to the brain screen at a specific position
	void Brain::draw_image(const char* filename, int x, int y, int32_t bgcolor) {
		if (!pros::usd::is_installed()) {
			print(11, 0, 0xff0000, "SD Card not found!");
			return;
		}

		FILE* file = fopen(filename, "rb");
		if (file == nullptr) {
			print(11, 0, 0xff0000, "File not found!");
			return;
		}

		int BUF_SIZE = 2048;
		std::vector<uint8_t> buf(BUF_SIZE);
		// Read the image size
		size_t r = fread(buf.data(), 1, BUF_SIZE, file);
		if(r < 4 + 1024) {	//size + palette
			fclose(file);
			print(11, 0, 0xff0000, "Invalid image file!");
			return;
		}
		int w = buf[0] << 8 | buf[1];
		int h = buf[2] << 8 | buf[3];

		// Calculate the position to draw the image
		int x0, y0;
		if(x == CENTER) {
			x0 = (SCREEN_W - w) / 2;
		}
		else if(x >= 0) {
			x0 = x;
		}
		else {
			x0 = SCREEN_W + x - w;
		}

		if(y == CENTER) {
			y0 = (SCREEN_H - h) / 2;
		}
		else if(y >= 0) {
			y0 = y;
		}
		else {
			y0 = SCREEN_H + y - h;
		}

		//Read the palette
		uint32_t old_eraser = pros::screen::get_eraser();
		if(bgcolor != 0xffffffff) {
			pros::screen::set_eraser(bgcolor);
			pros::screen::erase_rect(x0, y0, x0+w-1, y0+h-1);
			pros::screen::set_eraser(old_eraser);
		}
		else {
			bgcolor = old_eraser;
		}

		std::vector<uint32_t> palette(256);
		std::vector<uint8_t> alpha(256);
		int i;
		for (i = 0; i < 256; i++) {
			alpha[i] = buf[4+i*4+3];
			if(alpha[i] != 255) {
				buf[4+i*4]   = buf[4+i*4]   * alpha[i] /255 + (bgcolor >> 16 & 0xff) * (255 - alpha[i]) / 255;
				buf[4+i*4+1] = buf[4+i*4+1] * alpha[i] /255 + (bgcolor >> 8 & 0xff) * (255 - alpha[i]) / 255;
				buf[4+i*4+2] = buf[4+i*4+2] * alpha[i] /255 + (bgcolor & 0xff) * (255 - alpha[i]) / 255;
			}
			palette[i] = (buf[4+i*4] << 16) | (buf[4+i*4+1] << 8) | buf[4+i*4+2];
		}

		// Read the image data and draw the pixels
		int col = 0, row = 0, start;
		while(row < h) {
			if(col == 0 && row == 0) {
				start = 4 + 1024;
			}
			else {
				r = fread(buf.data(), 1, BUF_SIZE, file);
				if(r == 0)	// End of file
					break;
				start = 0;
			}

			for(i = start; i < r; i++) {
				uint8_t index = buf[i];
				if (alpha[index] != 0) {
					pros::screen::set_pen(palette[index]);
					pros::screen::draw_pixel(col + x0, row + y0);
				}

				// update column and row
				col++;
				if (col >= w) {
					col = 0;
					row++;
				}
			}
			pros::delay(1); // Need break between fread from the SD card
		}
		fclose(file);
		pros::delay(5);
	}

	// Draw a line
	void Brain::draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
		if(color != 0xffffffff)
			pros::screen::set_pen(color);
		pros::screen::draw_line(x1, y1, x2, y2);
	}

	// Register a callback for when the screen is pressed
	void Brain::pressed(std::function<bool()> callback) {
		on_press = callback;
	}

	// Register a callback for when the screen is released
	void Brain::released(std::function<bool()> callback) {
		on_release = callback;
	}

	// Get timestamp when the code is built
	const char* Brain::get_timestamp(const char* d, const char* t) {
		const char* months[] = {
			"Jan","Feb","Mar","Apr","May","Jun",
			"Jul","Aug","Sep","Oct","Nov","Dec"
		};

		std::istringstream iss(d);
		std::string mmm, dd, yyyy;
		iss >> mmm >> dd >> yyyy;

		// Find month number
		int m = 0;
		for (m = 0; m < 12; m++) {
			if (mmm == months[m]) {
				m++;
				break;
			}
		}

		// Format MM/DD/YYYY
		char buffer[24];
		sprintf(buffer, "%02d/%02d/%d %s", m, std::stoi(dd), std::stoi(yyyy), t);
		return std::string(buffer).c_str();
	}

	///////////////////////////////////////////////////////////////////////////
	// Button class implementation
	///////////////////////////////////////////////////////////////////////////
	Brain::Button::Button(const std::string& text, uint32_t color, uint32_t bgcolor, int x, int y, int w, int h, int radius, bool big) {
		this->text = text;
		this->color = color;
		this->bgcolor = bgcolor;
		this->x1 = x;
		this->y1 = y;
		this->x2 = x + w - 1;
		this->y2 = y + h - 1;
		this->on_press = nullptr;
		this->on_release = nullptr;
		if(radius < 0) {
			this->radius = 0;
		}
		else if(radius > w/2 || radius > h/2) {
			this->radius = std::min(w/2, h/2);
		}
		else {
			this->radius = radius;
		}
		this->big = big;

		Brain* parent = Brain::instance;
		if(parent != nullptr) {
			parent->buttons.push_back(this);
		}
	}

	// Draw the button on the screen
	void Brain::Button::draw() {
		uint32_t old_eraser = pros::screen::get_eraser();
		pros::screen::set_eraser(bgcolor);
		if(radius > 1) {
			pros::screen::erase_circle(x1+radius, y1+radius, radius-1);
			pros::screen::erase_circle(x2-radius, y1+radius, radius-1);
			pros::screen::erase_circle(x1+radius, y2-radius, radius-1);
			pros::screen::erase_circle(x2-radius, y2-radius, radius-1);
			pros::screen::erase_rect(x1+radius, y1, x2-radius, y2);
			pros::screen::erase_rect(x1, y1+radius, x2, y2-radius);
		}
		else{
			pros::screen::erase_rect(x1, y1, x2, y2);
		}

		if(text.length() == 0) {
			pros::screen::set_eraser(old_eraser);
			return;
		}

		pros::screen::set_pen(color);
		int num_of_lines = std::count(text.begin(), text.end(), '\n') + 1;
		std::istringstream iss(text);
		std::string line;
		for(int i=0; i<num_of_lines; i++) {
			std::getline(iss, line);
			size_t line_len = line.length();
			if(line_len > 0) {
				if(this->big) {
					int x0 = x1 + (x2 - x1 + 1 - line_len * FONT_W*2) / 2;
					int y0 = y1 + (y2 - y1 + 1 - num_of_lines * (FONT_H*1.6-2)) / 2 + i * (FONT_H*1.6-2) + 2;
					pros::screen::print(pros::E_TEXT_LARGE, x0, y0, line.c_str());
				}
				else {
					int x0 = x1 + (x2 - x1 + 1 - line_len * FONT_W) / 2;
					int y0 = y1 + (y2 - y1 + 1 - num_of_lines * (FONT_H-2)) / 2 + i * (FONT_H-2) + 2;
					pros::screen::print(pros::E_TEXT_MEDIUM, x0, y0, line.c_str());
				}
			}
		}
		pros::screen::set_eraser(old_eraser);
	}

	// Set the button text and redraw
	void Brain::Button::set_text(const char* t) {
		text = t;
		draw();
	}

	// Set the button text color and redraw
	void Brain::Button::set_color(uint32_t c) {
		color = c;
		draw();
	}

	// Set the button background color and redraw
	void Brain::Button::set_bgcolor(uint32_t bg) {
		bgcolor = bg;
		draw();
	}

	// Register a callback for when the button is pressed
	void Brain::Button::pressed(std::function<void()> callback) {
		on_press = callback;
	}

	// Register a callback for when the button is released
	void Brain::Button::released(std::function<void()> callback) {
		on_release = callback;
	}

	// Check if the button is currently touched
	bool Brain::Button::is_touched() {
		auto status = pros::screen::touch_status();
		return (status.x >= x1 && status.x <= x2 && status.y >= y1 && status.y <= y2);
	}
}

/********************************************************/
/* Others                                               */
/********************************************************/
namespace adlib {
	Distance::Distance(uint8_t port)
		: pros::Distance(port) {
	}

	double Distance::get_inches() {
		if(!is_installed()) {
			return 9999.0;
		}
		return get() / 25.4; // convert mm to inches
	}

	double Distance::distance_to_wall() {
		double d, sum = 0, min = 100, max = 0;
		for(int i=0; i<10; i++) {
			d = get_inches();
			sum += d;
			if (d < min) min = d;
			if (d > max) max = d;
			pros::delay(5);
		}
		return (sum - min - max) / 8;
	}

	/********************************************************/
	ADIDigitalOut::ADIDigitalOut(uint8_t port)
		: pros::ADIDigitalOut(port) {
	}

	void ADIDigitalOut::reverse(bool status) {
		is_reversed = status;
	}

	void ADIDigitalOut::press() {
		set_value(is_reversed? false : true);
		current_value = true;
	}

	void ADIDigitalOut::release() {
		set_value(is_reversed? true : false);
		current_value = false;
	}

	void ADIDigitalOut::toggle() {
		current_value = !current_value;
		set_value(is_reversed? !current_value : current_value);
	}
}
