#include "sweeper/config.hpp"
#include "sweeper/sweeper_engine.hpp"

#include "smt-switch/bitwuzla_factory.h"
#include "frontend/btor2_encoder.h"
#include "framework/ts.h"
#include "framework/symsim.h"

#include <fstream>
#include <iostream>
#include <filesystem>

using namespace sweeper;
using namespace wasim;


int main(int argc, char* argv[]) {
    silence_cout();
    Config config;
    if (!parse_arguments(argc, argv, config)) return EXIT_FAILURE;

    if (config.debug) {
        std::cout << "==== DEBUG ====\n"
                  << "BTOR2 File           : " << config.btor2_file << "\n"
                  << "Simulation Iterations: " << config.simulation_iterations << "\n"
                  << "Solver Timeout (ms)  : " << config.solver_timeout_ms << "\n"
                  << "Property Timeout (ms): " << config.property_check_timeout_ms << "\n"
                  << "Bound                : " << config.bound << "\n"
                  << "Dump SMT Enabled     : " << (config.dump_smt ? "Yes" : "No") << "\n"
                  << "Debug Enabled        : " << (config.debug ? "Yes" : "No") << "\n";
    }

    auto program_start = std::chrono::high_resolution_clock::now();
    sweeper::last_time_point = program_start;

    smt::SmtSolver solver = smt::BitwuzlaSolverFactory::create(false);
    solver->set_logic("QF_UFBV");
    solver->set_opt("incremental", "true");
    solver->set_opt("produce-models", "true");
    solver->set_opt("produce-unsat-assumptions", "true");

    TransitionSystem sts(solver);
    BTOR2Encoder encoder(config.btor2_file, sts);
    SymbolicSimulator sim(sts, solver);

    restore_cout();

    const auto & propvec = sts.prop();
    if (propvec.empty()) {
        std::cerr << "[ERROR] No property found.\n";
        return EXIT_FAILURE;
    }

    auto prop = and_vec(propvec, solver);
    sim.init();
    sim.set_input({}, {});
    for (int i = 1; i <= config.bound; ++i) {
        sim.sim_one_step();
        sim.set_input({}, {});
    }

    auto root = sim.interpret_state_expr_on_curr_frame(prop, false);
    TermVec constraints = sim.all_assumptions();
    for (const auto & c : constraints) {
        solver->assert_formula(c);
    }

    int total_nodes = 0;
    count_total_nodes(root, total_nodes);
    auto pre_done = std::chrono::high_resolution_clock::now();
    auto pre_time = std::chrono::duration_cast<std::chrono::milliseconds>(pre_done - program_start).count();
    std::cout << "[Pre-processing] " << pre_time/1000.0 << " s, total nodes: " << total_nodes << "\n";

    sweeper::SweeperOptions sweeper_opts;
    sweeper_opts.find_unsat = config.find_unsat;
    sweeper_opts.find_sat = config.find_sat;
    sweeper_opts.systems = { &sts };

    auto sweeper_stats = sweeper::sweeper(
        root,
        solver,
        config,
        sweeper_opts);

    std::cout << "[Ordering Time] " << sweeper_stats.ordering_time.count()/1000.0 << " s\n";

    int solve_time_ms = 0;
    auto solving_start = std::chrono::high_resolution_clock::now();

    solver->push();
    solver->assert_formula(solver->make_term(smt::Not, root));
    auto res = solver->check_sat();
    solver->pop();

    auto solving_end = std::chrono::high_resolution_clock::now();
    solve_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(solving_end - solving_start).count();
    std::cout << "[Solving] " << solve_time_ms/1000.0 << " s\n";

    if (res.is_unsat()) {
        std::cout << "[RESULT] Bound " << config.bound << " passed.\n";
    } else if (res.is_sat()) {
        std::cout << "[RESULT] Failed at bound " << config.bound << "\n";
    } else {
        std::cerr << "[ERROR] Solver returned UNKNOWN.\n";
    }

    std::cout << "Sweeping: " << sweeper_stats.solver_queries
            << ", [UNSAT] " << sweeper_stats.unsat_count << " ("
            << sweeper_stats.total_unsat_time.count()/1000.0 << " s), "
            << "[SAT] " << sweeper_stats.sat_count << " ("
            << sweeper_stats.total_sat_time.count()/1000.0 << " s)\n";

    std::cout << "[Hash Map Size] " << sweeper_stats.hash_map_size << "\n";
    std::cout << "[Substitution Map Size] " << sweeper_stats.substitution_map_size << "\n";
    std::cout << "[Node Data Map Size] " << sweeper_stats.node_data_map_size << "\n";




    auto program_end = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(program_end - program_start).count();
    std::cout << "[Total Execution] " << total_time/1000.0 << " s\n";
    return EXIT_SUCCESS;
}
