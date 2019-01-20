#ifndef SPOONY_RANDOMIZER_HH
#define SPOONY_RANDOMIZER_HH

#include <limits>
#include <vector>

class Randomizer {
	public:
		void reset();

		std::size_t get_index();
		int get_int(int min_value, int max_value);

		int get_set_variable_count() const;

		std::size_t get_minimum_variables() const;
		std::size_t get_maximum_variables() const;

		void set_implicit_index(std::size_t index);
		bool is_implicit() const;

		std::vector<int> data;

	private:
		int _next();

		std::size_t _minimum_variables = std::numeric_limits<std::size_t>::max();
		std::size_t _maximum_variables = 0;

		std::size_t _index = 0;
		std::size_t _implicit_index = 0;
};

#endif // SPOONY_RANDOMIZER_HH
