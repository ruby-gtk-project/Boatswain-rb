# frozen_string_literal: true

require "fileutils"
require "json"

module Boatswain
  # Settings holds the same keys as upstream's com.feaneron.Boatswain GSettings
  # schema (data/com.feaneron.Boatswain.gschema.xml), which has exactly one:
  # the desktop controller's restore token.
  #
  # Upstream uses GSettings, which needs a compiled schema installed into the
  # system's glib-2.0/schemas directory. Requiring an install step to run the
  # app from a checkout is worse than the alternative, so the same key, default
  # and semantics are backed by a small JSON file under XDG_CONFIG_HOME
  # instead, the way the pilot port does it.
  class Settings
    DEFAULTS = {
      "desktop-controller-restore-token" => "",
    }.freeze

    def initialize(path: nil)
      @path = path || default_path
      @values = DEFAULTS.merge(read)
    end

    def location = @path

    def [](key) = @values.fetch(key.to_s, DEFAULTS[key.to_s])

    def []=(key, value)
      @values[key.to_s] = value
      write
      value
    end

    DEFAULTS.each_key do |key|
      method_name = key.tr("-", "_")
      define_method(method_name) { self[key] }
      define_method(:"#{method_name}=") { |value| self[key] = value }
    end

    private

      def default_path
        File.join(
          ENV.fetch("XDG_CONFIG_HOME", File.expand_path("~/.config")),
          "boatswain",
          "settings.json",
        )
      end

      def read
        if File.exist?(@path)
          JSON.parse(File.read(@path))
        else
          {}
        end
      rescue JSON::ParserError => e
        warn "Error parsing #{@path}: #{e.message}"
        {}
      end

      def write
        FileUtils.mkdir_p(File.dirname(@path))
        File.write(@path, JSON.pretty_generate(@values))
      end
  end
end
