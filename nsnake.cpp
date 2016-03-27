/*
 *  nsnake: ncurses snake
 *  Copyright (C) 2016 - Maarten de Vries <maarten@de-vri.es>

 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.

 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.

 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <vector>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <random>
#include <clocale>
#include <iostream>

#include <cursesw.h>

namespace snake {
	std::uint32_t upper_block = U'\u2580';
	std::uint32_t lower_block = U'\u2584';
	std::uint32_t full_block  = U'\u2588';
	std::uint32_t empty_block = U'\u0020';

	/// A point in a 2D space.
	struct Vector2 {
		int x;
		int y;
	};

	bool operator==(Vector2 const & a, Vector2 const & b) { return a.x == b.x && a.y == b.y; }
	bool operator!=(Vector2 const & a, Vector2 const & b) { return !(a == b); }

	constexpr Vector2 operator*(Vector2 const & a, int scalar) { return {a.x * scalar, a.y * scalar}; };
	constexpr Vector2 operator*(int scalar, Vector2 const & a) { return a * scalar; }
	Vector2 & operator*=(Vector2 & a, int scalar) { return a = a * scalar; }

	constexpr Vector2 operator-(Vector2 const & a) { return a * -1; }
	constexpr Vector2 operator+(Vector2 const & a) { return a; }

	constexpr Vector2 operator+(Vector2 const & a, Vector2 const & b) { return {a.x + b.x, a.y + b.y}; };
	constexpr Vector2 operator-(Vector2 const & a, Vector2 const & b) { return a + -b; };
	Vector2 & operator+=(Vector2 & a, Vector2 const & b) { return a = a + b; };
	Vector2 & operator-=(Vector2 & a, Vector2 const & b) { return a = a - b; };

	enum class Direction {
		up,
		down,
		left,
		right
	};

	Direction operator-(Direction direction) {
		switch (direction) {
			case Direction::up:    return Direction::down;
			case Direction::down:  return Direction::up;
			case Direction::left:  return Direction::right;
			case Direction::right: return Direction::left;
		}
		throw std::logic_error("Invalid direction.");
	}

	Vector2 directionVector(Direction direction) {
		switch (direction) {
			case Direction::up:    return { 0, -1};
			case Direction::down:  return { 0,  1};
			case Direction::left:  return {-1,  0};
			case Direction::right: return { 1,  0};
		}
		throw std::logic_error("Invalid direction.");
	}

	/// A segment of a snake.
	struct Segment {
		Direction direction;
		int length;
	};

	/// A line in 2D space.
	struct Line {
		Vector2 start;
		Direction direction;
		int length;

		Line(Vector2 const & start, Direction const & direction, int length) : start(start), direction(direction), length(length) {};
		Line(Vector2 const & start, Segment const & segment) : Line(start, segment.direction, segment.length) {};
	};

	/// A snake.
	struct Snake {
		Vector2 head;
		std::vector<Segment> segments;

		/// Move the snake head forward in a given direction.
		void moveHead(Direction const & direction) {
			// If the snake changed direction, insert a new segment at the front.
			if (segments.front().direction != direction) {
				segments.insert(segments.begin(), {direction, 0});
			}

			// Move the head and lengthen the first segment.
			head += directionVector(segments.front().direction);
			segments.front().length += 1;
		}

		/// Shrink the tail of the snake.
		void shrinkTail() {
			segments.back().length  -= 1;

			// If the final segment reaches length zero, delete it.
			if (segments.back().length <= 0) {
				segments.pop_back();
			}
		}
	};

	/// Check if a point is on a given line.
	bool pointOnLine(Vector2 const & point, Line const & line) {
		Vector2 diff = point - line.start;
		switch (line.direction) {
			case Direction::up:    return diff.x == 0 && -diff.y >= 0 && -diff.y < line.length;
			case Direction::down:  return diff.x == 0 &&  diff.y >= 0 &&  diff.y < line.length;
			case Direction::left:  return diff.y == 0 && -diff.x >= 0 && -diff.x < line.length;
			case Direction::right: return diff.y == 0 &&  diff.x >= 0 &&  diff.x < line.length;
		}
		throw std::logic_error("Invalid direction.");
	}

	/// Check if a point is inside a given area.
	bool pointInsideArea(Vector2 const & point, Vector2 const & area) {
		return point.x >= 0 && point.x < area.x && point.y >= 0 && point.y < area.y;
	}

	/// Check for a collision of a point with a snake.
	bool pointCollidesWithSnake(Vector2 const & point, Snake const & snake, bool check_head = true) {
		Vector2 start = snake.head;
		for (unsigned int i = 0; i < snake.segments.size(); ++i) {
			Segment const & segment = snake.segments[i];
			if ((check_head || i > 0) && pointOnLine(point, Line{start, -segment.direction, segment.length})) {
				return true;
			}
			start = start - (directionVector(segment.direction) * segment.length);
		}
		return false;
	}

	/// Check if the snake has in internal or external collision.
	bool snakeCollided(Snake const & snake, Vector2 const & field_size) {
		return pointCollidesWithSnake(snake.head, snake, false) || !pointInsideArea(snake.head, field_size);
	}

	/// A snake game.
	struct Game {
		Vector2 board_size;
		bool alive = true;
		int score = 0;
		Snake snake;
		Vector2 fruit;
		std::string message;

		/// Reset a game.
		void reset(std::mt19937 & generator) {
			alive   = true;
			score   = 0;
			message = "";

			snake.head.x = board_size.x / 2;
			snake.head.y = board_size.y / 2;
			snake.segments.clear();
			snake.segments.push_back(Segment{Direction::up, 3});

			spawnFruit(generator);
		}

		/// Spawn new fruit on the board.
		void spawnFruit(std::mt19937 & generator) {
			std::uniform_int_distribution<int> random(0, board_size.x * board_size.y - 1);
			while (true) {
				int i = random(generator);
				Vector2 fruit = {i % board_size.x, i / board_size.x};
				if (!pointCollidesWithSnake(fruit, snake)) {
					this->fruit = fruit;
					break;
				}
			}
		}

		/// Process a game tick.
		void doTick(int input, std::mt19937 & generator) {
			// If dead, only ENTER will reset the game.
			if (!alive) {
				if (input == KEY_ENTER || input == '\n' || input == '\r') reset(generator);
				return;
			}

			// Set the new direction of the snake based on input.
			Direction new_direction = snake.segments.front().direction;
			switch (input) {
				case KEY_UP:    new_direction = Direction::up;    break;
				case KEY_RIGHT: new_direction = Direction::right; break;
				case KEY_DOWN:  new_direction = Direction::down;  break;
				case KEY_LEFT:  new_direction = Direction::left;  break;
			}

			// Dissalow about-turning the snake.
			if (new_direction == -snake.segments.front().direction) {
				new_direction = snake.segments.front().direction;
			}

			// Move the snake head (effectively grows the snake by 1).
			Snake old_snake = snake;
			snake.moveHead(new_direction);

			// Check if we hit the fruit this turn, if not shrink the snake.
			if (snake.head == fruit) {
				score += 1;
				spawnFruit(generator);
			} else {
				snake.shrinkTail();
			}

			// Make sure the snake did not collide with anything.
			if (snakeCollided(snake, board_size)) {
				snake   = old_snake;
				alive   = false;
				message = "You are dead. Press [Enter] to reset.";
				return;
			}
		}
	};

	/// Color definitions.
	enum class Color : unsigned char {
		black,
		red,
		green,
		yellow,
		blue,
		magenta,
		cyan,
		white,
	};

	/// A playing field.
	class Field {
		Vector2 size_;
		std::vector<Color> data_;

	public:
		Field(Vector2 const & size) : size_(size), data_(size.x * size.y, Color::black) {}
		Field(int x, int y) : Field(Vector2{x, y}) {};

		/// Get the size of the field.
		Vector2 const & size() const { return size_; }

		/// Get the value of a pixel in a field.
		Color       & pixel(int x, int y)       { return data_[y * size_.x + x]; }
		Color const & pixel(int x, int y) const { return data_[y * size_.x + x]; }
		Color       & pixel(Vector2 const & location)       { return pixel(location.x, location.y); }
		Color const & pixel(Vector2 const & location) const { return pixel(location.x, location.y); }

		/// Clear the field with a single color.
		void clear(Color const & color = Color::black) {
			data_.assign(data_.size(), color);
		}
	};

	/// Draw a point on a field.
	void drawPoint(Field & field, Vector2 const & location, Color const & color) {
		field.pixel(location) = color;
	}

	/// Draw a line on a field. Returns the end point of the line.
	Vector2 drawLine(Field & field, Line const & line) {
		Vector2 point = line.start;
		for (int i = 0; i < line.length; ++i) {
			field.pixel(point) = Color::white;
			point = point + directionVector(line.direction);
		}
		return point;
	}

	/// Draw a snake on a field.
	void drawSnake(Field & field, Snake const & snake) {
		Vector2 start = snake.head;
		for (unsigned int i = 0; i < snake.segments.size(); ++i) {
			Segment const & segment = snake.segments[i];
			start = drawLine(field, Line{start, -segment.direction, segment.length});
		}
	}

	int colorIndex(Color fg, Color bg) {
		return int(fg) * 8 + int(bg) + 1;
	};

	/// Print a field to the screen.
	void printField(int start_y, int start_x, Field const & field) {
		cchar_t cchar_buffer  = {};
		cchar_buffer.attr     = A_NORMAL | COLOR_PAIR(13);
		cchar_buffer.chars[0] = upper_block;
		cchar_buffer.chars[1] = 0;

		for (int y = 0; y < field.size().y; y += 2) {
			move(start_y + y / 2, start_x);
			for (int x = 0; x < field.size().x; ++x) {
				Color top    = field.pixel(x, y);
				Color bottom = Color::black;
				if (y + 1 < field.size().y) bottom = field.pixel(x, y + 1);
				cchar_buffer.attr = COLOR_PAIR(colorIndex(top, bottom));
				add_wch(&cchar_buffer);
			}
		}
	}
}

/// Initialize ncurses.
bool initNcurses() {
	initscr();
	cbreak();
	noecho();
	nonl();
	nodelay(stdscr, true);
	keypad(stdscr, true);
	curs_set(0);

	start_color();

	if (COLOR_PAIRS - 2 < 8 * 8) {
		std::cerr << "Not enough color pairs available.\n";
		return false;
	}

	for (int i = 0; i < 8; ++i) {
		for (int j = 0; j < 8; ++j) {
			init_pair(colorIndex(snake::Color(i), snake::Color(j)), i, j);
			std::cerr << colorIndex(snake::Color(i), snake::Color(j)) << " " << i << " " << j << "\n";
		}
	}

	return true;
}

void cursesBox(int y, int x, int width, int height) {
	mvvline(y,          x,         ACS_VLINE, height);
	mvvline(y,          x + width, ACS_VLINE, height);
	mvhline(y,          x,         ACS_HLINE, width);
	mvhline(y + height, x,         ACS_HLINE, width);
	mvaddch(y,          x,         ACS_ULCORNER);
	mvaddch(y,          x + width, ACS_URCORNER);
	mvaddch(y + height, x,         ACS_LLCORNER);
	mvaddch(y + height, x + width, ACS_LRCORNER);
}

int main() {
	std::setlocale(LC_ALL, "");

	// Initialize random device and generator.
	std::random_device rand;
	std::mt19937 generator(rand());

	snake::Game game;
	game.board_size = {20, 20};
	game.reset(generator);

	snake::Field field(game.board_size);

	if (!initNcurses()) {
		endwin();
		return 1;
	}

	try {
		int input = 0;
		while (true) {
			// Draw the current game state.
			field.clear();
			drawPoint(field, game.fruit, snake::Color::yellow);
			drawSnake(field, game.snake);
			mvprintw(0, 0, "Score: %u", game.score);           clrtoeol();
			mvprintw(1, 0, "%s",        game.message.c_str()); clrtoeol();

			snake::printField(3, 1, field);
			cursesBox(2, 0, field.size().x + 1, field.size().y / 2 + 1);
			refresh();

			// Wait a bit, get input and update the game.
			std::this_thread::sleep_for(std::chrono::milliseconds(10000 / (40 + game.score)));
			input = getch();
			if (input == 27 || input == 'q') break;
			game.doTick(input, generator);
		}
	} catch (...) {
		endwin();
		throw;
	}

	endwin();
}
