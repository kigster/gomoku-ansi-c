# frozen_string_literal: true

require "tty-box"

# Output helpers that delegate to TTY::Box when enabled.
# Include this module to use info, error, warn without a class prefix.
# In tests set EvalOutput.enabled = false to suppress output.
module EvalOutput
  class << self
    attr_accessor :enabled

    def info(*lines)
      return unless enabled?
      puts TTY::Box.info(lines.join("\n"))
    end

    def error(*lines)
      return unless enabled?
      puts TTY::Box.error(lines.join("\n"))
    end

    def warn(*lines)
      return unless enabled?
      puts TTY::Box.warn(lines.join("\n"))
    end

    def success(*lines)
      return unless enabled?
      puts TTY::Box.success(lines.join("\n"))
    end

    def enabled?
      @enabled != false
    end
  end

  self.enabled = true

  def info(*lines)
    EvalOutput.info(*lines)
  end

  def error(*lines)
    EvalOutput.error(*lines)
  end

  def warn(*lines)
    EvalOutput.warn(*lines)
  end

  def success(*lines)
    EvalOutput.success(*lines)
  end
end
