# frozen_string_literal: true

require "json"
require "yaml"
require "parallel"
require "fileutils"
require_relative "eval_output"

# Runs the depth/radius tournament via gomoku-http-client (Envoy on port 10000).
# Tracks PIDs for SIGINT cleanup. Writes detailed results per radius to results file.
class TournamentRunner
  DEFAULT_GAMES = 20
  DEFAULT_DEPTHS = [2, 3, 4].freeze
  DEFAULT_RADIUSES = [3, 4, 5].freeze
  DEFAULT_BOARD = 15
  DEFAULT_TIMEOUT = 300
  HTTP_PORT = 10_000

  Result = Struct.new(:radius, :depth_x, :depth_o, :winner, :time_sec, :game_index, keyword_init: true) do
    def header
      format("%8s | %8s | %8s | %6s | %10s | %10s", "Radius", "Depth_X", "Depth_O", "Winner", "Time_sec", "Game_index")
    end

    def as_row
      format("%8d | %8d | %8d | %6s | %10.2f | %10d",
        radius, depth_x, depth_o, (winner || "-").to_s, time_sec, game_index)
    end
  end

  def initialize(
    client_path:,
    output_dir:,
    results_file:,
    games_per_matchup: DEFAULT_GAMES,
    depths: DEFAULT_DEPTHS,
    radiuses: DEFAULT_RADIUSES,
    board_size: DEFAULT_BOARD,
    timeout: DEFAULT_TIMEOUT,
    num_workers: nil,
    pids: nil,
    &on_progress
  )
    @client_path = client_path
    @output_dir = output_dir
    @results_file = results_file
    @total_games = 0
    @games_per_matchup = games_per_matchup.to_i
    @depths = depths.to_a.map(&:to_i)
    @radiuses = radiuses.to_a.map(&:to_i)
    @board_size = board_size.to_i
    @timeout = timeout.to_i
    @num_workers = num_workers.to_i || [Parallel.processor_count, 1].max
    @pids = pids.to_a || []
    @pids_mutex = Mutex.new
    @on_progress = on_progress
    @results = []
    @interrupted = false
  end

  attr_reader :results, :pids, :pids_mutex, :interrupted
  attr_accessor :interrupted

  def run
    FileUtils.mkdir_p(@output_dir)
    depth_pairs = @depths.repeated_permutation(2).reject { |a, b| a == b }

    @radiuses.each do |radius|
      radius_results = run_radius(radius, depth_pairs)
      write_radius_results(radius, radius_results)
      @on_progress&.call(:radius_done, radius) unless @interrupted
      break if @interrupted
    end

    self
  end

  def run_radius(radius, depth_pairs)
    jobs = []
    depth_pairs.each do |depth_x, depth_o|
      @games_per_matchup.times do |g|
        jobs << { radius: radius, depth_x: depth_x, depth_o: depth_o, game_index: g + 1 }
      end
    end

    collected = []
    mutex = Mutex.new

    # Use threads so spawn() PIDs are children of this process and can be killed on SIGINT
    Parallel.each(jobs, in_processes: @num_workers) do |job|
      break if @interrupted
      result = run_one_game(
        radius: job[:radius],
        depth_x: job[:depth_x],
        depth_o: job[:depth_o],
        game_index: job[:game_index]
      )
      mutex.synchronize do
        collected << result if result
        @on_progress&.call(:game_done, result)
      end
    end

    @results.concat(collected)
    collected
  end

  def run_one_game(radius:, depth_x:, depth_o:, game_index:)
    game_file = File.join(
      @output_dir,
      "game-r#{radius}-d#{depth_x}-d#{depth_o}-b#{@board_size}-#{game_index}.json"
    )
    argv = [
      @client_path,
      "-p", HTTP_PORT.to_s,
      "-d", "#{depth_x}:#{depth_o}",
      "-r", radius.to_s,
      "-b", @board_size.to_s,
      "-t", @timeout.to_s,
      "-q",
      "-j", game_file
    ]
    elapsed = nil
    start = Process.clock_gettime(Process::CLOCK_MONOTONIC)
    pid = spawn(*argv, out: File::NULL, err: File::NULL)
    @pids_mutex.synchronize { @pids << pid }
    Process.wait(pid)
    @pids_mutex.synchronize { @pids.delete(pid) }
    elapsed = Process.clock_gettime(Process::CLOCK_MONOTONIC) - start

    winner = nil
    if File.file?(game_file)
      data = JSON.parse(File.read(game_file))
      winner = data["winner"]
      File.delete(game_file)
    end

    Result.new(
      radius: radius,
      depth_x: depth_x,
      depth_o: depth_o,
      winner: winner,
      time_sec: elapsed.round(2),
      game_index: game_index
    ).tap do |result|
      method = case winner
      when "X"
        :success
      when "O"
        :info
      else # "draw" or nil
        :warn
      end
      EvalOutput.send(method, "Game #{'%3d' % game_index}/#{@total_games} completed in #{'%6.3f' % elapsed.round(3)} seconds", 
        "#{result.header}\n#{result.as_row}")
      puts
    end
  rescue StandardError => e
    Result.new(
      radius: radius,
      depth_x: depth_x,
      depth_o: depth_o,
      winner: nil,
      time_sec: 0,
      game_index: game_index
    ).tap do |result|
      EvalOutput.error("Game #{game_index} failed in #{(elapsed || 0).round(2)} seconds", 
        "Error: #{e.message}",
        "Result: #{result.to_h.to_json}")
    end
  end

  def write_radius_results(radius, radius_results)
    wins = Hash.new(0)
    losses = Hash.new(0)
    draws = Hash.new(0)
    radius_results.each do |r|
      next unless r.winner
      key = "d#{r.depth_x}_r#{radius}"
      case r.winner
      when "X"
        wins["d#{r.depth_x}_r#{radius}"] += 1
        losses["d#{r.depth_o}_r#{radius}"] += 1
      when "O"
        wins["d#{r.depth_o}_r#{radius}"] += 1
        losses["d#{r.depth_x}_r#{radius}"] += 1
      else
        draws["d#{r.depth_x}_r#{radius}"] += 1
        draws["d#{r.depth_o}_r#{radius}"] += 1
      end
    end

    # Aggregate per (depth, radius) from first-player perspective
    by_key = Hash.new { |h, k| h[k] = { wins: 0, losses: 0, draws: 0 } }
    @depths.each do |d|
      key = "d#{d}_r#{radius}"
      by_key[key] = { wins: wins[key], losses: losses[key], draws: draws[key] }
    end

    lines = []
    lines << ""
    lines << "TOURNAMENT RESULTS for Radius #{radius}"
    lines << format("%8s | %8s | %8s | %8s | %8s | %7s", "Depth", "Radius", "Wins", "Losses", "Draws", "Win %")
    @depths.each do |d|
      key = "d#{d}_r#{radius}"
      tot = by_key[key][:wins] + by_key[key][:losses] + by_key[key][:draws]
      pct = tot.positive? ? (100.0 * by_key[key][:wins] / tot) : 0
      lines << format("%8d | %8d | %8d | %8d | %8d | %6.2f",
        d, radius, by_key[key][:wins], by_key[key][:losses], by_key[key][:draws], pct)
    end
    lines << ""

    wins_higher = radius_results.count do |r|
      r.winner == "X" && r.depth_x > r.depth_o || r.winner == "O" && r.depth_o > r.depth_x
    end
    wins_lower = radius_results.count do |r|
      r.winner == "X" && r.depth_x < r.depth_o || r.winner == "O" && r.depth_o < r.depth_x
    end
    total_decided = wins_higher + wins_lower
    if total_decided.positive?
      pct_high = 100.0 * wins_higher / total_decided
      lines << "WIN BIAS @ Radius #{radius}: Higher depth wins: #{wins_higher}, Lower depth wins: #{wins_lower} (#{pct_high.round(2)}% higher)"
      lines << ""
    end

    File.open(@results_file, "a") { |f| f.puts lines }
  end

  def self.aggregate_summary(results)
    total = results.size
    wins_higher = results.count do |r|
      r.winner && (r.winner == "X" && r.depth_x > r.depth_o || r.winner == "O" && r.depth_o > r.depth_x)
    end
    wins_lower = results.count do |r|
      r.winner && (r.winner == "X" && r.depth_x < r.depth_o || r.winner == "O" && r.depth_o < r.depth_x)
    end

    by_radius = results.group_by(&:radius)
    per_radius = {}
    by_radius.each do |radius, rs|
      matchups = Hash.new { |h, k| h[k] = { wins: 0, total: 0 } }
      rs.each do |r|
        next unless r.winner && r.winner != "draw"
        key = [r.depth_x, r.depth_o]
        matchups[key][:total] += 1
        matchups[key][:wins] += 1 if r.winner == "X"  # first player (depth_x) wins
      end
      per_radius[radius] = matchups
    end

    { total: total, wins_higher: wins_higher, wins_lower: wins_lower, per_radius: per_radius, results: results }
  end
end
