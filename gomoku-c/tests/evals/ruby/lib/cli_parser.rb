#!/usr/bin/env ruby
# frozen_string_literal: true

$LOAD_PATH.unshift(File.expand_path("lib", __dir__))
require "bundler/setup"
require "dry/cli"
require "eval_output"
require "tournament_runner"

module Eval
  module CLI
    module Commands
      class << self
        def test?
          ENV["RAILS_ENV"] == "test"
        end
      end

      extend Dry::CLI::Registry

      class Tournament < Dry::CLI::Command
        desc "Run depth/radius tournament via gomoku-http-client (Envoy port 10000)"

        option :games, aliases: ["g"], type: :integer, default: 20,
          desc: "Number of games per matchup"
        option :depths, aliases: ["d"], type: :string, default: "2,3,4",
          desc: "Depth levels (comma-separated)"
        option :radiuses, aliases: ["r"], type: :string, default: "3,4",
          desc: "Radius levels (comma-separated)"
        option :board, aliases: ["b"], type: :integer, default: 15,
          desc: "Board size"
        option :timeout, aliases: ["t"], type: :integer, default: 300,
          desc: "Move timeout (seconds)"
        option :verbose, aliases: ["v"], type: :boolean, default: false,
          desc: "Verbose output"

        def call(games:, depths:, radiuses:, board:, timeout:, verbose:, **)
          script_dir = File.expand_path(File.dirname(__FILE__))
          root_dir = File.expand_path("../../../", script_dir)
          client_path = File.join(root_dir, "gomoku-http-client")
          output_dir = File.join(root_dir, "tests/evals/results")
          timestamp = Time.now.strftime("%Y%m%d_%H%M%S")
          results_file = File.join(output_dir, "tournament-results-http-#{timestamp}.txt")

          EvalOutput.enabled = verbose

          unless File.executable?(client_path)
            EvalOutput.error("gomoku-http-client not found at #{client_path}", "Run 'make gomoku-http-client' first")
            exit 1
          end

          depth_arr = depths.split(",").map(&:strip).map(&:to_i)
          radius_arr = radiuses.split(",").map(&:strip).map(&:to_i)
          num_workers = [Parallel.processor_count, 1].max

          pids = []
          runner = TournamentRunner.new(
            client_path: client_path,
            output_dir: output_dir,
            results_file: results_file,
            games_per_matchup: games,
            depths: depth_arr,
            radiuses: radius_arr,
            board_size: board,
            timeout: timeout,
            num_workers: num_workers,
            pids: pids
          ) do |event, data|
            case event
            when :radius_done
              EvalOutput.info("RADIUS=#{data} complete, starting next radius.")
            end
          end

          # Save state on SIGINT and kill all gomoku-http-client children
          # :nocov: - trap block not exercised in specs
          trap("INT") do
            runner.interrupted = true
            EvalOutput.warn("Interrupted. Saving state and killing client processes...")
            pids_to_kill = runner.pids_mutex.synchronize { runner.pids.dup }
            pids_to_kill.each do |pid|
              Process.kill(9, pid) rescue nil
            end
            dump_partial_results(runner, results_file)
            EvalOutput.info("Partial results written to #{results_file}")
            exit 130
          end
          # :nocov:

          prior_info(games:, depths:, radiuses:, board:, timeout:, verbose:, num_workers:, pids:)

          File.write(results_file, "")  # truncate at start
          runner.run
          print_final_summary(runner, results_file)
        end

        private

        def dump_partial_results(runner, results_file)
          return if runner.results.empty?
          summary = TournamentRunner.aggregate_summary(runner.results)
          lines = ["\n--- PARTIAL RESULTS (interrupted) ---\n"]
          lines << "Total games completed: #{summary[:total]}"
          lines << "Wins by higher depth: #{summary[:wins_higher]}"
          lines << "Wins by lower depth: #{summary[:wins_lower]}"
          lines << ""
          File.open(results_file, "a") { |f| f.puts lines }
        end

        def print_final_summary(runner, results_file)
          summary = TournamentRunner.aggregate_summary(runner.results)
          total = summary[:total]
          wins_higher = summary[:wins_higher]
          wins_lower = summary[:wins_lower]

          EvalOutput.info(
            "You can find the detailed results of the tournament in #{results_file}.",
            "",
            "Quick summary:",
            "  • Total Games Played: #{total}",
            "  • Total Games Won by a player with higher Depth: #{wins_higher}",
            "  • Total Games Won by a player with lower Depth: #{wins_lower}",
            ""
          )
          EvalOutput.info("By depth (first player's depth shown first):")
          summary[:per_radius].sort.each do |radius, matchups|
            EvalOutput.info("RADIUS: #{radius}")
            matchups.sort.each do |(d1, d2), counts|
              next if counts[:total].zero?
              pct = (100.0 * counts[:wins] / counts[:total]).round
              rate_type = pct >= 50 ? "win" : "lose"
              pct_display = pct >= 50 ? pct : 100 - pct
              EvalOutput.info("  * Depth #{d1} vs #{d2}: #{pct_display}% #{rate_type} rate")
            end
          end
        end

        def prior_info(games:, depths:, radiuses:, board:, timeout:, verbose:, num_workers:, pids:)
          EvalOutput.enabled = true unless ::Eval::CLI::Commands.test?
          EvalOutput.warn(
            "Depth Tournament Runner:\n" +
            "Running tournament with the following parameters:                \n\n" +
            "  - Games per matchup: #{games}\n" +
            "  - Depths           : #{depths}\n" +
            "  - Radiuses         : #{radiuses}\n" +
            "  - Board size       : #{board}\n" +
            "  - Timeout          : #{timeout}\n" +
            "  - Verbose          : #{verbose}\n" +
            "  - Num workers      : #{num_workers}\n" +
            "  - Pids             : #{pids}\n"
          )

          puts unless ::Eval::CLI::Commands.test?

          EvalOutput.success(
            "Please note:\n" +
            "   • Since each game runs in a separate process, the results will\n" +
            "     arrive out of order. That's normal and is expected.\n" +
            "\n" +
            "Now, press Any Key to Continue...\n"
          )

          if !::Eval::CLI::Commands.test? && STDIN.respond_to?(:tty?) && STDIN.tty?
            require "io/console"
            STDIN.echo = false
            STDIN.getch
            STDIN.echo = true
          end

          STDOUT.puts "Continuing..." unless ::Eval::CLI::Commands.test?

          verbose = false if ::Eval::CLI::Commands.test?
          EvalOutput.enabled = verbose
        end
      end
      
      register "tournament", Tournament
    end
  end
end

if __FILE__ == $PROGRAM_NAME
  Dry::CLI.new(Eval::CLI::Commands).call(arguments: ARGV)
end
