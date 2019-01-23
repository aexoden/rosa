#include <iomanip>
#include <iostream>
#include <random>

#include "engine.hh"
#include "optimizer.hh"
#include "options.hh"
#include "route_output.hh"
#include "variable.hh"

/*
void optimize_ils(std::size_t start_index, Milliframes * best_frames, double * best_score, const Options & options, const std::shared_ptr<Variables> & variables, Engine * engine, const Engine & base_engine, const std::string & output_filename) {
	std::random_device rd;
	std::default_random_engine random_engine{rd()};

	optimize_local(start_index, best_frames, best_score, options, variables, engine, base_engine, output_filename, false);

	Milliframes search_best_frames = engine->get_frames();

	for (int i = 0; i < options.maximum_iterations; i++) {
		std::cout << "\rSeed: " << std::right << std::setw(4) << options.seed;
		std::cout << "   Algorithm: " << std::left << std::setw(15) << "ILS";
		std::cout << "   Index: " << std::right << std::setw(2) << i;
		std::cout << "   Best: " << std::setw(10) << Seconds{*best_frames}.count();
		std::cout << "   Current: " << std::setw(10) << Seconds{search_best_frames}.count();
		std::cout << "   Search Best: " << std::setw(10) << Seconds{search_best_frames}.count();
		std::cout << "   Iteration: " << std::setw(10) << (i + 1);
		std::cout << std::flush;

		std::uniform_int_distribution<std::size_t> index_dist{start_index, randomizer->data.size() - 1};

		int total = std::uniform_int_distribution<int>{-options.perturbation_wobble, options.perturbation_wobble}(random_engine);

		for (int j = 0; j < options.perturbation_strength; j++) {
			std::size_t index = index_dist(random_engine);
			total += randomizer->data[index];

			int value = std::uniform_int_distribution<int>{0, std::max(0, total * 2)}(random_engine);
			value = std::max(0, std::min(value, options.maximum_steps));

			if (j == options.perturbation_strength - 1) {
				value = std::max(0, std::min(total, options.maximum_steps));
			}

			randomizer->data[index] = value;
			total -= value;
		}

		optimize_local(start_index, best_frames, best_score, options, randomizer, engine, base_engine, output_filename, false);

		if (engine->get_frames() < search_best_frames) {
			search_best_frames = engine->get_frames();
		} else {
			randomizer->data = current_data;
		}
	}

	randomizer->reset();

	engine->reset();
	engine->run();

	std::cout << std::endl;
}

void optimize_local(std::size_t start_index, Milliframes * best_frames, double * best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine * engine, const Engine & base_engine, const std::string & output_filename, bool final_newline) {
	randomizer->reset();

	engine->reset();
	engine->run();

	Milliframes search_best_frames = engine->get_frames();

	while (true) {
		bool found_new_best = false;
		std::size_t best_index = 0;
		int best_value = 0;

		for (decltype(randomizer->data)::size_type i = start_index; i < randomizer->data.size(); i++) {
			std::cout << "\rSeed: " << std::right << std::setw(4) << options.seed;
			std::cout << "   Algorithm: " << std::left << std::setw(15) << "Local";
			std::cout << "   Index: " << std::right << std::setw(2) << i;
			std::cout << "   Variables: (" << std::setw(2) << randomizer->get_minimum_variables() << ", " << randomizer->get_maximum_variables() << ")";
			std::cout << "   Best: " << std::setw(10) << Seconds{*best_frames}.count();
			std::cout << "   Current: " << std::setw(10) << Seconds{search_best_frames}.count();
			std::cout << std::flush;

			int original_value = randomizer->data[i];

			for (int value = 0; value <= options.maximum_steps; value++) {
				randomizer->reset();
				randomizer->data[i] = value;

				engine->reset();
				engine->run();

				if ((engine->get_frames() < *best_frames) && RouteOutput::write_route(output_filename, randomizer, engine, base_engine, false)) {
					*best_frames = engine->get_frames();
					*best_score = engine->get_score();
				}

				if (engine->get_frames() < search_best_frames) {
					search_best_frames = engine->get_frames();
					best_index = i;
					best_value = value;
					found_new_best = true;
				}
			}

			randomizer->data[i] = original_value;
		}

		if (!found_new_best) {
			break;
		}

		randomizer->data[best_index] = best_value;
	}

	randomizer->reset();

	engine->reset();
	engine->run();

	if (final_newline) {
		std::cout << std::endl;
	}
}

void optimize_local_pair(std::size_t start_index, Milliframes * best_frames, double * best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine * engine, const Engine & base_engine, const std::string & output_filename, bool final_newline) {
	randomizer->reset();

	engine->reset();
	engine->run();

	Milliframes search_best_frames = engine->get_frames();
	Milliframes previous_search_best_frames = engine->get_frames();
	double search_best_score = engine->get_score();

	while (true) {
		bool found_new_best = false;

		std::size_t best_i = 0;
		std::size_t best_j = 0;

		int best_i_value = 0;
		int best_j_value = 0;

		for (std::size_t i = start_index; i < randomizer->data.size(); i++) {
			int original_i_value = randomizer->data[i];

			auto maximum = randomizer->data.size();

			if (options.maximum_comparisons > 0) {
				maximum = std::min(maximum, i + 1 + static_cast<std::size_t>(options.maximum_comparisons));
			}

			for (std::size_t j = i + 1; j < maximum; j++) {
				std::cout << "\rSeed: " << std::right << std::setw(4) << options.seed;
				std::cout << "   Algorithm: " << std::left << std::setw(15) << "Pairwise Local";
				std::cout << "   Current Indexes: (" << std::right << std::setw(3) << i << ", " << std::setw(3) << j << ")";
				std::cout << "   Variables: (" << std::setw(2) << randomizer->get_minimum_variables() << ", " << randomizer->get_maximum_variables() << ")";
				std::cout << "   Best: " << std::setw(10) << Seconds{*best_frames}.count();
				std::cout << "   Previous: " << std::setw(10) << Seconds{previous_search_best_frames}.count();
				std::cout << "   Current: " << std::setw(10) << Seconds{search_best_frames}.count();
				std::cout << std::flush;

				int original_j_value = randomizer->data[j];

				for (int i_value = 0; i_value <= options.maximum_steps; i_value++) {
					int j_minimum = 0;
					int j_maximum = options.maximum_steps;

					if (options.pairwise_shift) {
						j_minimum = original_i_value + original_j_value - i_value;
						j_maximum = j_minimum;
					}

					for (int j_value = j_minimum; j_value <= j_maximum; j_value++) {
						if (j_value >= 0) {
							randomizer->reset();
							randomizer->data[i] = i_value;
							randomizer->data[j] = j_value;

							engine->reset();
							engine->run();

							if ((engine->get_frames() < *best_frames || (engine->get_frames() == *best_frames && engine->get_score() > *best_score)) && RouteOutput::write_route(output_filename, randomizer, engine, base_engine, false)) {
								*best_frames = engine->get_frames();
								*best_score = engine->get_score();
							}

							if (engine->get_frames() < search_best_frames || (engine->get_frames() == search_best_frames && engine->get_score() > search_best_score)) {
								search_best_frames = engine->get_frames();
								search_best_score = engine->get_score();
								best_i = i;
								best_j = j;
								best_i_value = i_value;
								best_j_value = j_value;
								found_new_best = true;
							}
						}
					}
				}

				randomizer->data[j] = original_j_value;
			}

			randomizer->data[i] = original_i_value;
		}

		if (!found_new_best) {
			break;
		}

		if (final_newline) {
			std::cout << std::endl << "Updating (" << best_i << ", " << best_j << ") from (" << randomizer->data[best_i] << ", " << randomizer->data[best_j] << ") to (" << best_i_value << ", " << best_j_value << ") (" << Seconds{previous_search_best_frames}.count() << "s -> " << Seconds{search_best_frames}.count() << "s)" << std::endl;
		}

		previous_search_best_frames = search_best_frames;

		randomizer->data[best_i] = best_i_value;
		randomizer->data[best_j] = best_j_value;
	}

	randomizer->reset();

	engine->reset();
	engine->run();

	if (final_newline) {
		std::cout << std::endl;
	}
}

void optimize_sequential(std::size_t start_index, Milliframes * best_frames, double * best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine * engine, const Engine & base_engine, const std::string & output_filename) {
	randomizer->reset();

	engine->reset();
	engine->run();

	Milliframes search_best_frames = engine->get_frames();
	double search_best_score = engine->get_score();

	for (decltype(randomizer->data)::size_type i = start_index; i < randomizer->data.size(); i++) {
		std::cout << "\rSeed: " << std::right << std::setw(4) << options.seed;
		std::cout << "   Algorithm: " << std::left << std::setw(15) << "Sequential";
		std::cout << "   Index: " << std::right << std::setw(2) << i;
		std::cout << "   Variables: (" << std::setw(2) << randomizer->get_minimum_variables() << ", " << randomizer->get_maximum_variables() << ")";
		std::cout << "   Best: " << std::setw(10) << Seconds{*best_frames}.count();
		std::cout << "   Current: " << std::setw(10) << Seconds{search_best_frames}.count();
		std::cout << std::flush;

		int best_value = randomizer->data[i];

		for (int value = 0; value <= options.maximum_steps; value++) {
			randomizer->reset();
			randomizer->data[i] = value;

			engine->reset();
			engine->run();

			if ((engine->get_frames() < *best_frames || (engine->get_frames() == *best_frames && engine->get_score() > *best_score)) && RouteOutput::write_route(output_filename, randomizer, engine, base_engine, false)) {
				*best_frames = engine->get_frames();
				*best_score = engine->get_score();
			}

			if (engine->get_frames() < search_best_frames || (engine->get_frames() == search_best_frames && engine->get_score() > search_best_score)) {
				search_best_frames = engine->get_frames();
				search_best_score = engine->get_score();
				best_value = value;
			}
		}

		randomizer->data[i] = best_value;
	}

	randomizer->reset();

	engine->reset();
	engine->run();

	std::cout << std::endl;
}
*/
