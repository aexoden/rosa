#ifndef SPOONY_OPTIMIZER_HH
#define SPOONY_OPTIMIZER_HH

#include <memory>

#include "duration.hh"
#include "encounter.hh"

class Engine;
class Options;
class Randomizer;

void optimize_bb(int start_index, Milliframes & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const std::string & output_filename);
void optimize_ils(int start_index, Milliframes & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const std::string & output_filename);
void optimize_local(int start_index, Milliframes & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const std::string & output_filename, bool final_newline);
void optimize_local_pair(int start_index, Milliframes & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const std::string & output_filename, bool final_newline);
void optimize_sequential(int start_index, Milliframes & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const std::string & output_filename);

#endif // SPOONY_OPTIMIZER_HH
