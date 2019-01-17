#ifndef SPOONY_RANDOMIZER_HH
#define SPOONY_RANDOMIZER_HH

#include <limits>
#include <vector>

class Randomizer
{
	public:
		Randomizer(bool random);

		void reset();

		int get_index();
		int get_int(int min_value, int max_value);

		int get_set_variable_count() const;

		int get_minimum_variables() const;
		int get_maximum_variables() const;

		void set_implicit_index(int index);
		bool is_implicit() const;

		std::vector<int> data;

	private:
		int _next();

		int _minimum_variables = std::numeric_limits<int>::max();
		int _maximum_variables = 0;

		std::vector<int>::size_type _index = 0;

		bool _random;

		std::vector<int>::size_type _implicit_index = 0;
};

#endif // SPOONY_RANDOMIZER_HH
