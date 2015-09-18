#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <regex>
#include <streambuf>
#include <string>
#include <sstream>
#include <vector>

namespace
{
//	const int board_size = 15;
	int board_size = 15;
	const int killzone_size = 3;

//	const int initial_range_min = 50;
//	const int initial_range_max = 200;
}

namespace sys
{
	std::string read_file(const std::string& filename)
	{
		std::ifstream t(filename);
		return std::string((std::istreambuf_iterator<char>(t)), 
			std::istreambuf_iterator<char>());
	}
}

namespace util
{
	std::vector<std::string> split(const std::string& input, const std::string& regex) {
		// passing -1 as the submatch index parameter performs splitting
		 std::regex re(regex);
		std::sregex_token_iterator first(input.begin(), input.end(), re, -1);
		std::sregex_token_iterator last;
		return std::vector<std::string>(first, last);
	}
}

class Cell
{
public:
	Cell() : generation_(0), alive_(false) {}
	Cell(int generation) : generation_(generation), alive_(true) {}
	void setAlive(bool alive = true) { alive_ = alive; }
	bool isAlive() const { return alive_; }
	int getGeneration() const { return alive_ ? generation_ : 0; }
	void age() { ++generation_; }
private:
	int generation_;
	bool alive_;
	friend std::ostream& operator<<(std::ostream& os, const Cell& cell);
};

std::ostream& operator<<(std::ostream& os, const Cell& cell)
{
	if(cell.alive_) {
		os << "X";
	} else {
		os << " ";
	}
	return os;
}

void generate_board(std::vector<Cell>* board)
{
	const int initial_range_min = (board_size * board_size * 4) / 10;
	const int initial_range_max = (board_size * board_size * 55) / 100;

	std::mt19937 rng;
	rng.seed(std::random_device()());
	std::uniform_int_distribution<std::mt19937::result_type> dist(initial_range_min, initial_range_max);
	std::uniform_int_distribution<std::mt19937::result_type> location(0, board_size * board_size-1);

	board->clear();
	board->resize(board_size * board_size);

	const int count = dist(rng);
	for(int n = 0; n != count; ++n) {
		const int loc = location(rng);
		(*board)[loc].setAlive(true);
	}

	// remove the middle killzone
	for(int y = board_size/2 - killzone_size/2; y != board_size/2 + killzone_size/2; ++y) {
		for(int x = board_size/2 - killzone_size/2; x != board_size/2 + killzone_size/2; ++x) {
			(*board)[y * board_size + x].setAlive(false);
		}
	}
}

void print_board(const std::vector<Cell>& board)
{
	for(int n = -1; n != board_size + 1; ++n) {
		std::cout << "+";
	}
	std::cout << "\n";
	
	for(int y = 0; y != board_size; ++y) {
		std::cout << "+";
		for(int x = 0; x != board_size; ++x) {
			std::cout << board[y * board_size + x];
		}
		std::cout << "+\n";
	}
	for(int n = -1; n != board_size + 1; ++n) {
		std::cout << "+";
	}
	std::cout << std::endl;
}

void load_board(std::vector<Cell>* board, const std::string& filename)
{
	board->clear();
	board->resize(board_size * board_size);

	std::string s = sys::read_file(filename);
	auto vec = util::split(s, "\n");
	int y = 0;
	for(const auto& line : vec) {
		int x = 0;
		for(const auto& c : line) {
			auto& cell = (*board)[y * board_size + x];
			if(c == 'X' && x < board_size) {
				cell.setAlive(true);
			}
			++x;
		}
		++y;
		if(y >= board_size) {
			break;
		}
	}	
}

int get_neighbour_count(const std::vector<Cell>& board, int x, int y, int* max_generation = nullptr)
{
	const int start_x = x == 0 ? x : x - 1;
	const int end_x = x == board_size - 1 ? x : x + 1;
	const int start_y = y == 0 ? y : y - 1;
	const int end_y = y == board_size - 1 ? y : y + 1;
	int neighbours = 0;
	for(int dy = start_y; dy <= end_y; ++dy) {
		for(int dx = start_x; dx <= end_x; ++dx) {
			// ignore ourself.
			if(x == dx &&  y == dy) {
				continue;
			}
			const auto& cell = board[dy * board_size + dx];
			if(cell.isAlive()) {
				++neighbours;
				if(max_generation != nullptr && *max_generation < cell.getGeneration()) {
					*max_generation = cell.getGeneration();
				}
			}
		}
	}
	return neighbours;
}

int run_generation(std::vector<Cell>* board, std::map<int, int>* kill_histogram)
{
	std::vector<Cell> new_board(*board);
	//std::map<int, int> death_histogram;
	int cells_born = 0;
	int cells_killed_in_killzone = 0;
	// iterate over board.
	// 1) cells that have fewer than two neighbours die.
	// 2) cells that have more than three neighboards die.
	// 3) cells at a generation over 60 die.
	// 4) dead cells with three live neighbours live, at generation+1 of the oldest adjacent neighbour.
	// 5) cells surviving with 2 or 3 live neighbours have there generation count increased by one.
	for(int y = 0; y != board_size; ++y) {
		for(int x = 0; x != board_size; ++x) {
			const auto& cell = (*board)[y * board_size + x];
			auto& new_cell = new_board[y * board_size + x];

			int max_generation = 0;
			const int count = get_neighbour_count(*board, x, y, &max_generation);
			if(cell.isAlive()) {
				if(count < 2 || count > 3 || cell.getGeneration() >= 60) {
					new_cell.setAlive(false);
					//auto& h = *death_histogram[cell.getGeneration()];
					//++h;
				} else if(count == 2 || count == 3) {
					new_cell.age();
				}
				if(y >= board_size/2 - killzone_size/2 && y <= board_size/2 + killzone_size/2
					&& x >= board_size/2 - killzone_size/2 && x <= board_size/2 + killzone_size/2) {
					new_cell.setAlive(false);
					auto& h = (*kill_histogram)[cell.getGeneration()];
					++h;
					++cells_killed_in_killzone;
				}
			} else {
				if(count == 3) {
					new_cell = Cell(max_generation + 1);
					++cells_born;
				}
			}
		}
	}
	*board = new_board;

	//for(const auto& h : death_histogram) {
	//	std::cout << "Cells dead at age: " << h.first << " : " << h.second << "\n";
	//}
	//if(cells_born > 0) {
	//	std::cout << "New cells: " << cells_born << "\n";
	//}
	//if(cells_killed_in_killzone > 0) {
	//	std::cout << "Cells killed in killzone: " << cells_killed_in_killzone << "\n";
	//}
	return cells_killed_in_killzone;
}

int count_living_cells(const std::vector<Cell>& board)
{
	int living_cells = 0;
	for(int y = 0; y != board_size; ++y) {
		for(int x = 0; x != board_size; ++x) {
			auto& cell = board[y * board_size + x];
			if(cell.isAlive()) {
				++living_cells;
			}
		}
	}
	return living_cells;
}

int main(int argc, char* argv[])
{
	std::vector<std::string> args;
	for(int n = 1; n != argc; ++n) {
		std::string arg(argv[n]);
		if("--b=" == arg.substr(0, 4)) {
			std::istringstream ss(arg.substr(4, std::string::npos));
			ss >> board_size;
			if(board_size < 7 || board_size > 1024) {
				std::cout << "Board size parameter must be within bounds of 7-1024 " << board_size << "\n";
				return 1;
			}
		} else {
			args.emplace_back(arg);
		}
	}

	std::vector<Cell> board;
	if(!args.empty()) {
		load_board(&board, args[0]);
	} else {
		generate_board(&board);
	}

	const int start_cells = count_living_cells(board);
	print_board(board);

	int cells_in_killed_in_killzone = 0;
	int generations = 0;

	bool done = false;
	std::map<int, int> kill_histogram;
	while(!done) {
		cells_in_killed_in_killzone += run_generation(&board, &kill_histogram);
		//print_board(board);

		const int cbc = count_living_cells(board);
		if(cbc == 0) {
			done = true;
			std::cout << "Stopping as no cells are live.\n";
		}
		++generations;
	}
	std::cout << "Starting cells: " << start_cells << "\n";
	std::cout << "Total cells killed in killzone: " << cells_in_killed_in_killzone << ", generations: " << generations << std::endl;
	for(const auto& h : kill_histogram) {
		std::cout << "Cells dead at age: " << h.first << " : " << h.second << "\n";
	}
}
