# frozen_string_literal: true

require "spec_helper"
require "tournament_runner"
require "fileutils"
require "tempfile"

RSpec.describe TournamentRunner do
  let(:client_path) { "/fake/gomoku-http-client" }
  let(:output_dir) { Dir.mktmpdir("tournament_spec") }
  let(:results_file) { File.join(output_dir, "results.txt") }
  let(:result_struct) { described_class::Result }

  after { FileUtils.rm_rf(output_dir) }

  describe "::aggregate_summary" do
    it "returns total count and wins by higher/lower depth" do
      results = [
        result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: "O", time_sec: 1.0, game_index: 1, client_exitstatus: 0),
        result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: "X", time_sec: 1.0, game_index: 2, client_exitstatus: 0),
        result_struct.new(radius: 3, depth_x: 3, depth_o: 2, winner: "X", time_sec: 1.0, game_index: 1, client_exitstatus: 0)
      ]
      summary = described_class.aggregate_summary(results)
      expect(summary[:total]).to eq 3
      expect(summary[:wins_higher]).to eq 2  # O (3>2), X (3>2)
      expect(summary[:wins_lower]).to eq 1   # X when depth_x 2 < depth_o 3
    end

    it "ignores draw and nil winner for higher/lower counts" do
      results = [
        result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: "draw", time_sec: 0, game_index: 1, client_exitstatus: nil),
        result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: nil, time_sec: 0, game_index: 2, client_exitstatus: nil)
      ]
      summary = described_class.aggregate_summary(results)
      expect(summary[:wins_higher]).to eq 0
      expect(summary[:wins_lower]).to eq 0
    end

    it "builds per_radius matchups from first player (X) perspective" do
      results = [
        result_struct.new(radius: 3, depth_x: 1, depth_o: 2, winner: "X", time_sec: 0, game_index: 1, client_exitstatus: nil),
        result_struct.new(radius: 3, depth_x: 1, depth_o: 2, winner: "O", time_sec: 0, game_index: 2, client_exitstatus: nil)
      ]
      summary = described_class.aggregate_summary(results)
      expect(summary[:per_radius][3][[1, 2]][:total]).to eq 2
      expect(summary[:per_radius][3][[1, 2]][:wins]).to eq 1
    end
  end

  describe "#run" do
    it "creates output_dir and returns self" do
      dir = File.join(output_dir, "nested")
      runner = described_class.new(
        client_path: client_path,
        output_dir: dir,
        results_file: results_file,
        games_per_matchup: 1,
        depths: [2, 3],
        radiuses: [3],
        num_workers: 1
      )
      allow(runner).to receive(:run_radius).and_return([])
      expect(File.directory?(dir)).to be false
      runner.run
      expect(File.directory?(dir)).to be true
      expect(runner.run).to eq runner
    end

    it "stops after first radius when interrupted" do
      runner = described_class.new(
        client_path: client_path,
        output_dir: output_dir,
        results_file: results_file,
        games_per_matchup: 0,
        depths: [2],
        radiuses: [3, 4],
        num_workers: 1
      )
      call_count = 0
      allow(runner).to receive(:run_radius) do
        call_count += 1
        runner.interrupted = true if call_count == 1
        []
      end
      runner.run
      expect(call_count).to eq 1
    end
  end

  describe "#run_one_game" do
    let(:runner) do
      described_class.new(
        client_path: client_path,
        output_dir: output_dir,
        results_file: results_file,
        games_per_matchup: 1,
        depths: [2, 3],
        radiuses: [3],
        num_workers: 1,
        pids: []
      )
    end

    it "parses winner from JSON and returns Result" do
      fake_pid = 99_999
      allow(runner).to receive(:spawn).and_return(fake_pid)
      allow(Process).to receive(:wait).with(fake_pid)
      allow(File).to receive(:file?).with(anything).and_return(true)
      allow(File).to receive(:read).with(anything) { '{"winner":"X"}' }
      allow(File).to receive(:delete)

      result = runner.run_one_game(radius: 3, depth_x: 2, depth_o: 3, game_index: 1)

      expect(result).to be_a(described_class::Result)
      expect(result.radius).to eq 3
      expect(result.depth_x).to eq 2
      expect(result.depth_o).to eq 3
      expect(result.winner).to eq "X"
    end

    it "returns result with nil winner when JSON file missing" do
      fake_pid = 88_888
      allow(runner).to receive(:spawn).and_return(fake_pid)
      allow(Process).to receive(:wait).with(fake_pid)
      allow(File).to receive(:file?).with(anything).and_return(false)

      result = runner.run_one_game(radius: 3, depth_x: 2, depth_o: 3, game_index: 1)

      expect(result.winner).to be_nil
    end
  end

  describe "#write_radius_results" do
    let(:runner) do
      described_class.new(
        client_path: client_path,
        output_dir: output_dir,
        results_file: results_file,
        games_per_matchup: 2,
        depths: [2, 3],
        radiuses: [3],
        num_workers: 1
      )
    end

    it "appends TOURNAMENT RESULTS and Win BIAS to results_file" do
      results = [
        result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: "X", time_sec: 1, game_index: 1, client_exitstatus: 0),
        result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: "O", time_sec: 1, game_index: 2, client_exitstatus: 0)
      ]
      runner.write_radius_results(3, results)
      content = File.read(results_file)
      expect(content).to include("TOURNAMENT RESULTS for Radius 3")
      expect(content).to include("Depth")
      expect(content).to include("Radius")
      expect(content).to include("WIN BIAS")
    end

    it "includes draw results in by_key aggregation" do
      results = [
        result_struct.new(radius: 3, depth_x: 2, depth_o: 3, winner: "draw", time_sec: 1, game_index: 1, client_exitstatus: 0)
      ]
      runner.write_radius_results(3, results)
      content = File.read(results_file)
      expect(content).to include("TOURNAMENT RESULTS for Radius 3")
    end
  end

  describe "#run_one_game rescue" do
    let(:runner) do
      described_class.new(
        client_path: "/fake/client",
        output_dir: output_dir,
        results_file: results_file,
        games_per_matchup: 1,
        depths: [2, 3],
        radiuses: [3],
        pids: []
      )
    end

    it "returns Result with nil winner on StandardError" do
      allow(runner).to receive(:spawn).and_raise(StandardError.new("oops"))
      result = runner.run_one_game(radius: 3, depth_x: 2, depth_o: 3, game_index: 1)
      expect(result).to be_a(described_class::Result)
      expect(result.winner).to be_nil
      expect(result.time_sec).to eq 0
    end
  end

  describe "constants" do
    it "defines HTTP_PORT 10000" do
      expect(described_class::HTTP_PORT).to eq 10_000
    end

    it "defines default depths and radiuses" do
      expect(described_class::DEFAULT_DEPTHS).to eq [2, 3, 4]
      expect(described_class::DEFAULT_RADIUSES).to eq [3, 4, 5]
    end
  end
end
