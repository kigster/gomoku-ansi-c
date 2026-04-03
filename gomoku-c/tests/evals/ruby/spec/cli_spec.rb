# frozen_string_literal: true

require "spec_helper"
require "open3"

# Load CLI (script only runs CLI when executed as main)
require_relative "../lib/cli_parser"

RSpec.describe "depth_tournament_http CLI" do
  let(:script) { File.expand_path("../depth-tournament", __dir__) }

  it "lists tournament subcommand when run with --help" do
    out, err, status = Open3.capture3("ruby", script, "--help")
    expect(out + err).to include("tournament")
    expect([0, 1]).to include(status.exitstatus)
  end

  it "tournament subcommand accepts --help" do
    out, err, status = Open3.capture3("ruby", script, "tournament", "--help")
    expect(status).to be_success
    combined = out + err
    expect(combined).to include("games")
    expect(combined).to include("depths")
    expect(combined).to include("radiuses")
  end

  it "exits non-zero when client binary is missing" do
    script_dir = File.expand_path("..", __dir__)
    root_dir = File.expand_path("../..", script_dir)
    client_path = File.join(root_dir, "gomoku-http-client")
    allow(File).to receive(:executable?).and_call_original
    allow(File).to receive(:executable?).with(client_path).and_return(false)
    expect do
      Eval::CLI::Commands::Tournament.new.call(
        games: 1, depths: "2,3", radiuses: "3", board: 15, timeout: 300, verbose: false
      )
    end.to raise_error(SystemExit) do |e|
      expect(e.status).to eq 1
    end
  end

  it "runs tournament and prints final summary when client exists" do
    script_dir = File.expand_path("..", __dir__)
    root_dir = File.expand_path("../..", script_dir)
    client_path = File.join(root_dir, "gomoku-http-client")
    allow(File).to receive(:executable?).with(client_path).and_return(true)
    allow(File).to receive(:write)
    allow(File).to receive(:open).and_call_original
    dir = Dir.mktmpdir("eval_tournament")
    results_file = File.join(dir, "results.txt")
    result_struct = TournamentRunner::Result
    fake_results = [
      result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: "X", time_sec: 1.0, game_index: 1, client_exitstatus: 0),
      result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: "O", time_sec: 1.0, game_index: 2, client_exitstatus: 0)
    ]
    runner = instance_double(TournamentRunner,
      run: nil,
      results: fake_results,
      pids: [],
      pids_mutex: Mutex.new,
      interrupted: false)
    allow(TournamentRunner).to receive(:new).and_return(runner)

    Eval::CLI::Commands::Tournament.new.call(
      games: 1, depths: "2,3", radiuses: "3", board: 15, timeout: 300, verbose: false
    )

    expect(TournamentRunner).to have_received(:new)
    expect(runner).to have_received(:run)
  end

  describe "Tournament private helpers" do
    it "dump_partial_results writes to file when runner has results" do
      result_struct = TournamentRunner::Result
      results = [
        result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: "X", time_sec: 1, game_index: 1, client_exitstatus: 0)
      ]
      runner = instance_double(TournamentRunner, results: results)
      results_file = File.join(Dir.mktmpdir("eval_dump"), "out.txt")
      File.write(results_file, "existing\n")
      cmd = Eval::CLI::Commands::Tournament.new
      cmd.send(:dump_partial_results, runner, results_file)
      content = File.read(results_file)
      expect(content).to include("PARTIAL RESULTS")
      expect(content).to include("Total games completed: 1")
    end

    it "dump_partial_results does not write when results empty" do
      runner = instance_double(TournamentRunner, results: [])
      results_file = File.join(Dir.mktmpdir("eval_dump2"), "out.txt")
      File.write(results_file, "existing\n")
      cmd = Eval::CLI::Commands::Tournament.new
      cmd.send(:dump_partial_results, runner, results_file)
      expect(File.read(results_file)).to eq "existing\n"
    end

    it "print_final_summary handles lose rate and multiple radii" do
      result_struct = TournamentRunner::Result
      results = [
        result_struct.new(radius: 3, depth_x: 1, depth_o: 2, winner: "O", time_sec: 1, game_index: 1, client_exitstatus: 0),
        result_struct.new(radius: 3, depth_x: 1, depth_o: 2, winner: "O", time_sec: 1, game_index: 2, client_exitstatus: 0),
        result_struct.new(radius: 4, depth_x: 2, depth_o: 3, winner: "X", time_sec: 1, game_index: 1, client_exitstatus: 0)
      ]
      runner = instance_double(TournamentRunner, results: results)
      cmd = Eval::CLI::Commands::Tournament.new
      expect { cmd.send(:print_final_summary, runner, "/tmp/results.txt") }.not_to raise_error
    end

    it "print_final_summary skips matchups with zero total" do
      # per_radius can have a key with total 0 if we build summary manually
      runner = instance_double(TournamentRunner, results: [])
      allow(TournamentRunner).to receive(:aggregate_summary).with([]).and_return(
        total: 0, wins_higher: 0, wins_lower: 0,
        per_radius: { 3 => { [1, 2] => { wins: 0, total: 0 } } }
      )
      cmd = Eval::CLI::Commands::Tournament.new
      expect { cmd.send(:print_final_summary, runner, "/tmp/results.txt") }.not_to raise_error
    end
  end
end
