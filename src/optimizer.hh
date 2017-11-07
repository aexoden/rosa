/*
 * Copyright (c) 2015 Jason Lynch <jason@calindora.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
