# frozen_string_literal: true

require "spec_helper"
require "eval_output"

RSpec.describe EvalOutput do
  after { described_class.enabled = true }

  describe ".enabled=" do
    it "can be set to false to disable output" do
      described_class.enabled = false
      expect(described_class.enabled?).to eq false
    end

    describe "when output is enabled" do
      before do
        expect(described_class).to receive(:enabled?).and_return(true)
      end
      it "enabled? is true by default" do
        described_class.enabled = true
        expect(described_class.enabled?).to eq true
      end
    end
  end

  describe ".info" do
    it "does not raise when disabled" do
      described_class.enabled = false
      expect { described_class.info("hello") }.not_to raise_error
    end

    it "joins multiple lines with newline" do
      described_class.enabled = false
      expect { described_class.info("a", "b") }.not_to raise_error
    end
  end

  describe ".error" do
    it "does not raise when disabled" do
      described_class.enabled = false
      expect { described_class.error("err") }.not_to raise_error
    end
  end

  describe ".warn" do
    it "does not raise when disabled" do
      described_class.enabled = false
      expect { described_class.warn("warn") }.not_to raise_error
    end
  end

  context "when included in a module" do
    let(:host) { Class.new { include EvalOutput }.new }

    it "exposes info without class prefix" do
      described_class.enabled = false
      expect { host.info("x") }.not_to raise_error
    end

    it "exposes error without class prefix" do
      described_class.enabled = false
      expect { host.error("x") }.not_to raise_error
    end

    it "exposes warn without class prefix" do
      described_class.enabled = false
      expect { host.warn("x") }.not_to raise_error
    end
  end
end
