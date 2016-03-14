#include <vector>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <random>
#include <clocale>

#include <cursesw.h>

namespace snake {
	const int up    = 0;
	const int right = 1;
	const int down  = 2;
	const int left  = 3;

	std::uint32_t upper_block = L'\u2580';
	std::uint32_t lower_block = L'\u2584';
	std::uint32_t full_block  = L'\u2588';
	std::uint32_t empty_block = L'\u0020';

	/// A point in a 2D space.
	struct Point {
		int x;
		int y;
	};

	/// A 2D size.
	struct Size {
		int width;
		int height;
	};

	/// A segment of a snake.
	struct Segment {
		int direction;
		int length;
	};

	/// A snake.
	struct Snake {
		Point head;
		std::vector<Segment> segments;
	};

	/// A playing field.
	struct Field {
		Size size;
		std::vector<bool> data;
	};

	/// A snake game.
	struct Game {
		bool alive = true;
		int score = 0;
		Size board_size;
		Snake snake;
		Point fruit;
		std::string message;
		std::mt19937 * generator;
	};

	/// Get the opposite of a given direction.
	int opposite(int direction) {
		switch (direction) {
			case up:    return down;
			case right: return left;
			case down:  return up;
			case left:  return right;
		}
		throw std::logic_error("opposite: Invalid direction: " + std::to_string(direction));
	}

	/// Check if two points are equal.
	bool pointsEqual(Point const & a, Point const & b) {
		return a.x == b.x && a.y == b.y;
	}

	/// Get a point in a line from a given point.
	Point advance(Point const & point, int direction, int distance = 1) {
		Point result = point;
		switch(direction) {
			case up:    result.y -= distance; return result;
			case right: result.x += distance; return result;
			case down:  result.y += distance; return result;
			case left:  result.x -= distance; return result;
		}
		throw std::logic_error("advance: Invalid direction: " + std::to_string(direction));
	}

	/// Check if a point is on a given line.
	bool pointOnLine(Point const & point, Point const & line_start, int line_direction, int line_length) {
		int dx = point.x - line_start.x;
		int dy = point.y - line_start.y;

		switch (line_direction) {
			case up:     return dx == 0 && -dy >= 0 && -dy < line_length;
			case down:   return dx == 0 &&  dy >= 0 &&  dy < line_length;
			case left:   return dy == 0 && -dx >= 0 && -dx < line_length;
			case right:  return dy == 0 &&  dx >= 0 &&  dx < line_length;
		}

		throw std::logic_error("pointOnLine: Invalid direction: " + std::to_string(line_direction));
	}

	/// Check if a point is inside a given area.
	bool pointInsideArea(Point const & point, Size const & area) {
		return point.x >= 0 && point.x < area.width && point.y >= 0 && point.y < area.height;
	}

	/// Check for a collision of a point with a snake.
	bool pointCollidesWithSnake(Point const & point, Snake const & snake, bool check_head = true) {
		Point start = snake.head;
		for (unsigned int i = 0; i < snake.segments.size(); ++i) {
			Segment const & segment = snake.segments[i];
			if ((check_head || i > 0) && pointOnLine(point, start, opposite(segment.direction), segment.length)) {
				return true;
			}
			start = advance(start, opposite(segment.direction), segment.length);
		}
		return false;
	}

	/// Make a field with the given width and height.
	Field makeField(Size size) {
		Field result;
		result.size = size;
		result.data = std::vector<bool>(size.width * size.height);
		return result;
	}

	/// Clear a field from all drawings.
	void clearField(Field & field) {
		for (unsigned int i = 0; i < field.data.size(); ++i) {
			field.data[i] = false;
		}
	}

	/// Get the value of a pixel in a field.
	bool getPixel(Field const & field, Point const & location) {
		return field.data[location.y * field.size.width + location.x];
	}

	/// Set the value of a pixel in a field.
	void drawPixel(Field & field, Point const & location, bool value) {
		field.data[location.y * field.size.width + location.x] = value;
	}

	/// Draw a line on a field. Returns the end point of the line.
	Point drawLine(Field & field, Point const & start, int direction, int length) {
		Point point = start;
		for (int i = 0; i < length; ++i) {
			drawPixel(field, point, true);
			point = advance(point, direction);
		}
		return point;
	}

	/// Draw a snake on a field.
	void drawSnake(Field & field, Snake const & snake) {
		Point start = snake.head;
		for (unsigned int i = 0; i < snake.segments.size(); ++i) {
			Segment const & segment = snake.segments[i];
			start = drawLine(field, start, opposite(segment.direction), segment.length);
		}
	}

	/// Move the snake head forward in a given direction.
	void moveSnakeHead(Snake & snake, int direction) {
		// If the snake changed direction, insert a new segment at the front.
		if (snake.segments.front().direction != direction) {
			snake.segments.insert(snake.segments.begin(), {direction, 0});
		}

		// Move the head and lengthen the first segment.
		snake.head = advance(snake.head, snake.segments.front().direction);
		snake.segments.front().length += 1;
	}

	/// Shrink the tail of the snake.
	void shrinkSnakeTail(Snake & snake) {
		snake.segments.back().length  -= 1;

		// If the final segment reaches length zero, delete it.
		if (snake.segments.back().length <= 0) {
			snake.segments.pop_back();
		}
	}

	/// Check if the snake has in internal or external collision.
	bool snakeCollided(Snake const & snake, Size const & field_size) {
		return pointCollidesWithSnake(snake.head, snake, false) || !pointInsideArea(snake.head, field_size);
	}

	/// Reset a game.
	void resetGame(Game & game) {
		game.alive   = true;
		game.score   = 0;
		game.message = "";

		game.snake.head.x = game.board_size.width  / 2;
		game.snake.head.y = game.board_size.height / 2;
		game.snake.segments.clear();
		game.snake.segments.push_back(Segment{up, 3});
	}

	/// Spawn new fruit on the board.
	void spawnFruit(Game & game) {
		std::uniform_int_distribution<int> random(0, game.board_size.width * game.board_size.height - 1);
		Point fruit;
		while (true) {
			int i = random(*game.generator);
			fruit.x = i % game.board_size.width;
			fruit.y = i / game.board_size.width;
			if (!pointCollidesWithSnake(fruit, game.snake)) break;
		}
		game.fruit = fruit;
	}

	/// Process a game tick.
	void doTick(Game & game, int input) {
		// If dead, only ENTER will reset the game.
		if (!game.alive) {
			if (input == KEY_ENTER || input == '\n' || input == '\r') {
				resetGame(game);
			}
			return;
		}

		// Set the new direction of the snake based on input.
		int new_direction = game.snake.segments.front().direction;
		switch (input) {
			case KEY_UP:    new_direction = up;    break;
			case KEY_RIGHT: new_direction = right; break;
			case KEY_DOWN:  new_direction = down;  break;
			case KEY_LEFT:  new_direction = left;  break;
		}

		// Dissalow about-turning the snake.
		if (new_direction == opposite(game.snake.segments.front().direction)) {
			new_direction = game.snake.segments.front().direction;
		}

		// Move the snake head (effectively grows the snake by 1).
		Snake old_snake = game.snake;
		moveSnakeHead(game.snake, new_direction);

		// Check if we hit the fruit this turn, if not shrink the snake.
		if (pointsEqual(game.snake.head, game.fruit)) {
			game.score += 1;
			spawnFruit(game);
		} else {
			shrinkSnakeTail(game.snake);
		}

		// Make sure the snake did not collide with anything.
		if (snakeCollided(game.snake, game.board_size)) {
			game.snake   = old_snake;
			game.alive   = false;
			game.message = "You are dead. Press [Enter] to reset.";
			return;
		}
	}

	/// Print a field to the screen.
	void printField(int y, int x, Field const & field) {
		cchar_t buffer;
		buffer.attr     = A_NORMAL;
		buffer.chars[1] = 0;

		for (int i = 0; i < field.size.height; i += 2) {
			move(y + i / 2, x);
			for (int j = 0; j < field.size.width; ++j) {
				bool top    = getPixel(field, {j, i});
				bool bottom = i + 1 < field.size.height && getPixel(field, {j, i+1});
				buffer.chars[0] = top && bottom ? full_block : top ? upper_block : bottom ? lower_block : empty_block;
				add_wch(&buffer);
			}
		}
	}
}

/// Initialize ncurses.
void initNcurses() {
	initscr();
	cbreak();
	noecho();
	nonl();
	nodelay(stdscr, true);
	keypad(stdscr, true);
	curs_set(0);
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
	game.generator  = &generator;
	game.board_size = {20, 20};
	snake::resetGame(game);

	snake::Field field = makeField(game.board_size);

	initNcurses();

	try {
		int input = 0;
		while (true) {
			// Draw the current game state.
			clearField(field);
			drawPixel(field, game.fruit, true);
			drawSnake(field, game.snake);
			mvprintw(0, 0, "Score: %u", game.score);           clrtoeol();
			mvprintw(1, 0, "%s",        game.message.c_str()); clrtoeol();

			cursesBox(2, 0, field.size.width + 1, field.size.height / 2 + 1);
			printField(3, 1, field);
			refresh();

			// Wait a bit, get input and update the game.
			std::this_thread::sleep_for(std::chrono::milliseconds(10000 / (40 + game.score)));
			input = getch();
			if (input == 27 || input == 'q') break;
			doTick(game, input);
		}
	} catch (...) {
		endwin();
		throw;
	}

	endwin();
}
