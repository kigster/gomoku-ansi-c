# frozen_string_literal: true

ENV["RAILS_ENV"] = "test"

$LOAD_PATH.unshift(File.expand_path("../lib", __dir__))

require "simplecov"
SimpleCov.start do
  add_filter "/spec/"
  add_filter "/vendor/"
  minimum_coverage 90
end

require "eval_output"
require "cli_parser"

# Disable TTY output in tests so we can assert without terminal noise
EvalOutput.enabled = false

RSpec.configure do |config|

  config.before(:each) do
    allow(::Eval::CLI::Commands).to receive(:test?).and_return(true)
    allow(::EvalOutput).to receive(:enabled?).and_return(false)
  end
  config.expect_with :rspec do |expectations|
    expectations.include_chain_clauses_in_custom_matcher_descriptions = true
  end
  config.mock_with :rspec do |mocks|
    mocks.verify_partial_doubles = true
  end
  config.shared_context_metadata_behavior = :apply_to_host_groups
end
