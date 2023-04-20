/*
 * Copyright (c) 2019 Broadcom.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Broadcom, Inc. - initial API and implementation
 */

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "config/b4g_config.h"
#include "config/pgm_conf.h"
#include "diagnostic_counter.h"
#include "nlohmann/json.hpp"
#include "utils/path.h"
#include "utils/platform.h"
#include "utils/unicode_text.h"
#include "workspace_manager.h"

/*
 * The benchmark is used to evaluate multiple aspects about the performance and accuracy of the parse library.
 * The user specifies a standard HLASM workspace folder and the benchmark calls did_open_file for each program
 * file defined in the workspace's pgm_conf.json or .bridge.json. Performance metrics are written
 * to the console after each parsed file. When the whole benchmark is done, a json with the outputs is created.
 *
 * Accepted parameters:
 * -r start-end  - Range of files to be parsed in start-end format (zero based). Otherwise, all defined files are parsed
 * -c file_name  - Turns on infinite (unless -r is specified) parsing of a file specified by path
 * -p path       - Specifies a path to the folder with .hlasmplugin
 * -d            - Prints parsing details
 * -s            - Skips reparsing of each file
 * -m message    - Prepends message before every log entry related to parsed files
 * -g path       - Specifies a path to the folder with .bridge.json
 *
 * Collected metrics:
 * - File                     - File name
 * - Success                  - Parsing result
 * - Errors                   - Number of errors encountered during the parsing
 * - Warnings                 - Number of warnings encountered during the parsing
 * - Wall Time                - Parsing duration, wall time
 * - CPU Time                 - Parsing duration, CPU time
 * - Executed statements      - Number of executed statements
 * - ExecStatement/ms         - ExecStatements includes open code, macro, copy, lookahead and reparsed statements
 * - Line/ms                  - Processed lines per ms
 * - Top messages             - Top message code with highest occurrence
 * - Open Code Statements     - Number of statements parsed in open code
 * - Copy Statements          - Number of statements parsed in copy files (includes multiple uses of the same copy file)
 * - Macro Statements         - Number of statements parsed in macro files (includes multiple uses of the same macro)
 * - Copy Def Statements      - Number of statements defined in copy files (only the first occurrence of the copy file)
 * - Macro Def Statements     - Number of statements defined in macro files (only the first occurrence of the macro)
 * - Lookahead Statements     - Number of statements processed in lookahead mode
 * - Reparsed Statements      - Number of statements that were reparsed later (e.g. model statements)
 * - Continued Statements     - Number of statements that were continued (multiple continuations of one statement count
 *as one continued statement)
 * - Non-continued Statements - Number of statements that were not continued
 * - Lines                    - Total number of lines
 * - Files                    - Total number of parsed files
 */

using namespace hlasm_plugin;

using json = nlohmann::json;

namespace {
struct parsing_metadata_collector final : public parser_library::parsing_metadata_consumer
{
    void consume_parsing_metadata(
        parser_library::sequence<char>, double, const parser_library::parsing_metadata& metadata) override
    {
        data.emplace_back(metadata);
    }

    std::vector<parser_library::parsing_metadata> data;
};

class bench_configuration
{
public:
    std::string ws_folder = utils::path::current_path().string();
    std::string single_file = "";
    size_t start_range = 0, end_range = 0;
    bool write_details = true;
    bool do_reparse = true;
    std::string message;
    std::vector<std::string> pgm_names;
    std::optional<std::string> b4g_pgms_dir = std::nullopt;

    bool load(int argc, char** argv)
    {
        if (!load_options(argc, argv))
            return false;

        load_programs_to_parse();
        return true;
    }

    void log() const
    {
        if (write_details)
        {
            std::clog << "ws_folder: " << ws_folder << '\n';
            std::clog << "single_file: " << single_file << '\n';
            std::clog << "start_range-end_range: " << start_range << "-" << end_range - 1 << '\n';
            std::clog << "write_details: " << write_details << '\n';
            std::clog << "do_reparse: " << do_reparse << '\n';
            std::clog << "message: " << message << '\n';
            std::clog << "number of pgms: " << pgm_names.size() << std::endl;
        }
    }

private:
    bool load_options(int argc, char** argv)
    {
        const auto advance_and_retrieve = [argc, &argv](std::string_view option, auto& i, auto& s) {
            if (i + 1 >= argc)
            {
                std::clog << "Missing parameter for option " << option << '\n';
                return false;
            }

            s = static_cast<std::string>(argv[++i]);
            return true;
        };

        for (int i = 1; i < argc; i++)
        {
            if (std::string arg = argv[i]; arg == "-r") // range parameter, format start-end
            {
                std::string val;
                if (!advance_and_retrieve(arg, i, val))
                    return false;

                auto pos = val.find('-');
                if (pos == val.npos)
                {
                    std::clog << "Range parameter should be in format Start-End" << '\n';
                    return false;
                }

                try
                {
                    start_range = std::stoi(val.substr(0, pos));
                    end_range = std::stoi(val.substr(pos + 1));
                }
                catch (...)
                {
                    std::clog << "Range values must be integers" << '\n';
                    return false;
                }
            }
            else if (arg == "-p") // Path parameter, path to the folder containing .hlasmplugin
            {
                if (!advance_and_retrieve(arg, i, ws_folder))
                    return false;
            }
            else if (arg == "-c") // Cycle parameter, loop infinitely single file
            {
                if (!advance_and_retrieve(arg, i, single_file))
                    return false;
            }
            else if (arg == "-d") // Details switch, when specified, details are not outputted to stderr
                write_details = false;
            else if (arg == "-s") // When specified, skip reparsing each program to test out macro caching
                do_reparse = false;
            else if (arg == "-g") // Points to directory containing .bridge.json file
            {
                if (!advance_and_retrieve(arg, i, b4g_pgms_dir))
                    return false;
            }
            else if (arg == "-m") // Specifies annotation of each "Parsing <file>" message
            {
                if (!advance_and_retrieve(arg, i, message))
                    return false;
            }
            else
            {
                std::clog << "Unknown parameter " << arg << '\n';
                return false;
            }
        }

        return true;
    }

    void load_programs_to_parse()
    {
        bool some_config_exists = false;

        if (parser_library::config::pgm_conf pgm_conf; retrieve_config(pgm_conf, ".hlasmplugin/pgm_conf.json"))
        {
            some_config_exists = true;
            for (const auto& pgm : pgm_conf.pgms)
                pgm_names.emplace_back(pgm.program);
        }

        if (parser_library::config::b4g_map b4g_conf;
            b4g_pgms_dir.has_value() && retrieve_config(b4g_conf, b4g_pgms_dir.value() + "/.bridge.json"))
        {
            some_config_exists = true;
            for (const auto& [file, _] : b4g_conf.files)
                pgm_names.emplace_back(b4g_pgms_dir.value() + "/" + file);
        }

        if (!some_config_exists)
        {
            std::clog << "Non-existing configuration file: "
                      << ".hlasmplugin/pgm_conf.json" << '\n';

            if (b4g_pgms_dir.has_value())
                std::clog << "Non-existing configuration file: " << b4g_pgms_dir.value() + "/.bridge.json" << '\n';
        }
    }

    template<typename T>
    bool retrieve_config(T& configuration, std::string_view relative_cfg_file_path)
    {
        const auto cfg_json_path = ws_folder.append("/").append(relative_cfg_file_path);
        auto cfg_o = utils::platform::read_file(cfg_json_path);

        if (!cfg_o.has_value())
            return false;

        try
        {
            json::parse(cfg_o.value()).get_to(configuration);
        }
        catch (...)
        {
            return false;
        }

        return true;
    }
};

class bench
{
public:
    bool start(const bench_configuration& bc)
    {
        bc.log();

        all_file_stats s;
        if (!bc.single_file.empty())
        {
            auto end_range = bc.end_range != 0 ? bc.end_range : std::numeric_limits<long long int>::max();

            for (size_t i = 0; i < end_range; ++i)
                std::cout << parse_file(
                    parse_parameters {
                        bc.single_file,
                        i,
                        bc,
                    },
                    s,
                    bc.do_reparse,
                    bc.write_details)
                                 .dump(2)
                          << std::flush;
        }
        else if (!bc.pgm_names.empty())
        {
            if (bc.start_range >= bc.pgm_names.size() - 1)
            {
                std::clog << "Start range > program count (" << bc.start_range << " > " << s.program_count << ")\n";
                return false;
            }

            std::cout << "{\n\"pgms\" : [" << std::flush;

            bool first = true;
            for (size_t i = bc.start_range; i < bc.end_range && bc.start_range < bc.pgm_names.size(); ++i)
            {
                if (!std::exchange(first, false))
                    std::cout << ",\n";

                std::cout << parse_file(
                    parse_parameters {
                        bc.pgm_names[i],
                        i,
                        bc,
                    },
                    s,
                    bc.do_reparse,
                    bc.write_details)
                                 .dump()
                          << std::flush;
            }
            std::cout << "],\n\"total\" : ";

            std::clog << "Programs: " << s.program_count << '\n'
                      << "Benchmarked files: " << s.all_files << '\n'
                      << "Analyzer crashes: " << s.parsing_crashes << '\n'
                      << "Analyzer crashes (reparsing): " << s.reparsing_crashes << '\n'
                      << "Failed program opens: " << s.failed_file_opens << '\n'
                      << "Benchmark time: " << s.whole_time << " ms" << '\n'
                      << "Average statement/ms: " << s.average_stmt_ms / (double)bc.pgm_names.size() << '\n'
                      << "Average line/ms: " << s.average_line_ms / (double)bc.pgm_names.size() << "\n\n"
                      << std::endl;

            std::cout << json({ { "Programs", s.program_count },
                                  { "Benchmarked files", s.all_files },
                                  { "Benchmark time(ms)", s.whole_time },
                                  { "Analyzer crashes", s.parsing_crashes },
                                  { "Failed program opens", s.failed_file_opens },
                                  { "Average statement/ms", s.average_stmt_ms / bc.pgm_names.size() },
                                  { "Average line/ms", s.average_line_ms / bc.pgm_names.size() } })
                             .dump(2);
            std::cout << "}\n";
            std::clog << "Parse finished\n\n" << std::endl;
        }

        return true;
    }

private:
    struct all_file_stats
    {
        double average_line_ms = 0;
        double average_stmt_ms = 0;
        size_t all_files = 0;
        long long whole_time = 0;
        size_t program_count = 0;
        size_t parsing_crashes = 0;
        size_t reparsing_crashes = 0;
        size_t failed_file_opens = 0;
    };

    struct parse_time_stats
    {
        clock_t clock_time;
        long long time;
    };

    struct parse_results
    {
        bool success;
        json response;
        long long time;
    };

    struct parse_parameters
    {
        parser_library::workspace_manager ws;
        benchmark::diagnostic_counter diag_counter;
        parsing_metadata_collector collector;
        const std::string& source_file;
        std::string source_path;
        std::string annotation;

        parse_parameters(const std::string& source_file, size_t current_iteration, const bench_configuration& bc)
            : source_file(source_file)
            , source_path(bc.ws_folder + "/" + source_file)
        {
            annotation = get_file_message(current_iteration, bc);
            ws.register_diagnostics_consumer(&diag_counter);
            ws.register_parsing_metadata_consumer(&collector);
            ws.add_workspace(bc.ws_folder.c_str(), bc.ws_folder.c_str());
            if (ws.idle_handler())
                abort();
        }

    private:
        std::string get_file_message(size_t iteration, const bench_configuration& bc) const
        {
            if (bc.message.empty())
                return "";

            return "[" + bc.message + " " + std::to_string(iteration) + "/(" + std::to_string(bc.start_range) + "-"
                + std::to_string(bc.end_range ? bc.end_range - 1 : bc.end_range) + ")]";
        }
    };

    json parse_file(parse_parameters parse_params, all_file_stats& s, bool do_reparse, bool write_details)
    {
        auto content_o = utils::platform::read_file(parse_params.source_path);
        if (!content_o.has_value())
        {
            ++s.failed_file_opens;
            std::clog << parse_params.annotation << "File read error: " << parse_params.source_path << std::endl;
            return json({ { "File", parse_params.source_file }, { "Success", false }, { "Reason", "Read error" } });
        }

        s.program_count++;
        // program's contents
        auto content = utils::replace_non_utf8_chars(content_o.value());

        auto [init_parse_res, json_res, parse_time] = initial_parse(parse_params, s, content);
        if (!init_parse_res)
            return json_res;

        auto first_ws_info = parse_params.collector.data.front().ws_info;
        auto first_parse_top_messages = benchmark::get_top_messages(parse_params.diag_counter.message_counts);
        auto first_parse_metrics = parse_params.collector.data.front().metrics;
        auto first_diag_counter = parse_params.diag_counter;
        long long reparse_time = 0;

        // Reparse to benchmark macro caching
        if (do_reparse)
        {
            auto [reparse_res, json_reparse_res, time] = repeated_parse(parse_params, s, content);
            if (!reparse_res)
                return json_reparse_res;

            json_res.update(json_reparse_res, true);
            reparse_time = time;
        }

        auto exec_statements = first_parse_metrics.open_code_statements + first_parse_metrics.copy_statements
            + first_parse_metrics.macro_statements + first_parse_metrics.lookahead_statements
            + first_parse_metrics.reparsed_statements;

        if (write_details)
            std::clog << "Time: " << parse_time << " ms" << '\n'
                      << "Reparse time: " << reparse_time << " ms" << '\n'
                      << "Errors: " << first_diag_counter.error_count << '\n'
                      << "Reparse errors: " << parse_params.diag_counter.error_count << '\n'
                      << "Open Code Statements: " << first_parse_metrics.open_code_statements << '\n'
                      << "Copy Statements: " << first_parse_metrics.copy_statements << '\n'
                      << "Macro Statements: " << first_parse_metrics.macro_statements << '\n'
                      << "Copy Def Statements: " << first_parse_metrics.copy_def_statements << '\n'
                      << "Macro Def Statements: " << first_parse_metrics.macro_def_statements << '\n'
                      << "Lookahead Statements: " << first_parse_metrics.lookahead_statements << '\n'
                      << "Reparsed Statements: " << first_parse_metrics.reparsed_statements << '\n'
                      << "Continued Statements: " << first_parse_metrics.continued_statements << '\n'
                      << "Non-continued Statements: " << first_parse_metrics.non_continued_statements << '\n'
                      << "Lines: " << first_parse_metrics.lines << '\n'
                      << "Executed Statement/ms: " << (double)exec_statements / (double)parse_time << '\n'
                      << "Line/ms: " << (double)first_parse_metrics.lines / (double)parse_time << '\n'
                      << "Files: " << first_ws_info.files_processed << '\n'
                      << "Top messages: " << first_parse_top_messages.dump() << '\n'
                      << '\n'
                      << std::endl;

        return json_res;
    }

    parse_results initial_parse(parse_parameters& parse_params, all_file_stats& s, const std::string& content)
    {
        auto time_stats = parse(parse_params, content, false);
        if (!time_stats.has_value())
        {
            ++s.parsing_crashes;
            return parse_results {
                false,
                json({
                    { "File", parse_params.source_file },
                    { "Success", false },
                    { "Reason", "Crash" },
                }),
            };
        }
        else if (parse_params.collector.data.size() != 1)
        {
            ++s.parsing_crashes;
            std::clog << "Parsing error: Unexpected parsing metadata" << std::endl;
            return parse_results {
                false,
                json({
                    { "File", parse_params.source_file },
                    { "Success", false },
                    { "Reason", "Unexpected parsing metadata" },
                }),
            };
        }

        auto& [clock_time, time] = *time_stats;
        const auto& diag_counter = parse_params.diag_counter;
        const auto& metadata = parse_params.collector.data.front();

        const auto& files_processed = metadata.ws_info.files_processed;
        const auto& metrics = metadata.metrics;
        auto exec_statements = metrics.open_code_statements + metrics.copy_statements + metrics.macro_statements
            + metrics.lookahead_statements + metrics.reparsed_statements;
        s.average_stmt_ms += (exec_statements / (double)time);
        s.average_line_ms += metrics.lines / (double)time;
        s.all_files += files_processed;
        s.whole_time += time;

        return parse_results {
            true,
            json({
                { "File", parse_params.source_file },
                { "Success", true },
                { "Errors", diag_counter.error_count },
                { "Warnings", diag_counter.warning_count },
                { "Wall Time (ms)", time },
                { "CPU Time (ms/n)", 1000.0 * clock_time / CLOCKS_PER_SEC },
                { "Executed Statements", exec_statements },
                { "ExecStatement/ms", exec_statements / (double)time },
                { "Line/ms", metrics.lines / (double)time },
                { "Top messages", benchmark::get_top_messages(diag_counter.message_counts) },
                { "Open Code Statements", metrics.open_code_statements },
                { "Copy Statements", metrics.copy_statements },
                { "Macro Statements", metrics.macro_statements },
                { "Copy Def Statements", metrics.copy_def_statements },
                { "Macro Def Statements", metrics.macro_def_statements },
                { "Lookahead Statements", metrics.lookahead_statements },
                { "Reparsed Statements", metrics.reparsed_statements },
                { "Continued Statements", metrics.continued_statements },
                { "Non-continued Statements", metrics.non_continued_statements },
                { "Lines", metrics.lines },
                { "Files", files_processed },
            }),
            time,
        };
    }

    parse_results repeated_parse(parse_parameters& parse_params, all_file_stats& s, const std::string& content)
    {
        auto& diag_counter = parse_params.diag_counter;

        diag_counter.clear_counters();

        auto time_stats = parse(parse_params, content, true);
        if (!time_stats.has_value())
        {
            ++s.reparsing_crashes;
            return parse_results {
                false,
                json({
                    { "File", parse_params.source_file },
                    { "Success", false },
                    { "Reason", "Crash" },
                    { "Reparse", true },
                }),
            };
        }

        auto& [clock_time, time] = *time_stats;
        return parse_results {
            true,
            json({
                { "Reparse Wall Time (ms)", time },
                { "Reparse CPU Time (ms/n)", 1000.0 * clock_time / CLOCKS_PER_SEC },
                { "Reparse errors", diag_counter.error_count },
                { "Reparse warnings", diag_counter.warning_count },
            }),
            time,
        };
    }

    std::optional<parse_time_stats> parse(parse_parameters& parse_params, const std::string& content, bool reparse)
    {
        std::string annotation;
        annotation.append(parse_params.annotation).append(reparse ? "Reparsing " : "Parsing ");
        auto& ws = parse_params.ws;
        const auto& source_path_c_str = parse_params.source_path.c_str();
        static const parser_library::document_change dummy_change({}, "", 0);

        // Log a message before starting the clock
        std::clog << annotation << "file: " << parse_params.source_file << std::endl;

        // ******************    START THE CLOCK    ******************
        auto c_start = std::clock();
        auto start = std::chrono::high_resolution_clock::now();

        // open file/parse
        try
        {
            reparse ? ws.did_change_file(source_path_c_str, 1, &dummy_change, 1)
                    : ws.did_open_file(source_path_c_str, 1, content.c_str(), content.length());

            if (ws.idle_handler())
                abort();
        }
        catch (const std::exception& e)
        {
            std::clog << annotation << "error: " << e.what() << std::endl;
            return std::nullopt;
        }
        catch (...)
        {
            std::clog << annotation << "failed\n\n" << std::endl;
            return std::nullopt;
        }

        // ******************    STOP THE CLOCK    ******************

        return parse_time_stats {
            std::clock() - c_start,
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start)
                .count(),
        };
    }
};
} // namespace

int main(int argc, char** argv)
{
    bench_configuration bench_config;
    if (!bench_config.load(argc, argv))
        return false;

    if (bench_config.pgm_names.empty())
    {
        std::clog << "Didn't manage to load any programs to benchmark" << std::endl;
        return false;
    }

    bench benchmark;
    return !benchmark.start(bench_config);
}
