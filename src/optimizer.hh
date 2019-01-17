#ifndef SPOONY_OPTIMIZER_HH
#define SPOONY_OPTIMIZER_HH

#include <memory>

#include <giomm/file.h>

class Engine;
class Options;
class Randomizer;

void optimize_bb(int start_index, double & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const Glib::RefPtr<Gio::File> & output_file);
void optimize_ils(int start_index, double & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const Glib::RefPtr<Gio::File> & output_file);
void optimize_local(int start_index, double & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const Glib::RefPtr<Gio::File> & output_file, bool final_newline);
void optimize_local_pair(int start_index, double & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const Glib::RefPtr<Gio::File> & output_file, bool final_newline);
void optimize_sequential(int start_index, double & best_frames, double & best_score, const Options & options, const std::shared_ptr<Randomizer> & randomizer, Engine & engine, const Engine & base_engine, const Glib::RefPtr<Gio::File> & output_file);

#endif // SPOONY_OPTIMIZER_HH
