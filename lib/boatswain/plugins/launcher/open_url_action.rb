# frozen_string_literal: true

require "adwaita"

require_relative "../../action"

module Boatswain
  module Plugins
    module Launcher
      # OpenUrlAction is the port of
      # src/plugins/launcher/launcher-open-url-action.c: opens a URL typed into
      # its one entry row.
      class OpenUrlAction < Boatswain::Action
        def initialize
          super
          icon.icon_name = "web-browser-symbolic"
        end

        attr_reader :url

        def url=(value)
          @url = value.to_s.strip
        end

        # Port of launcher_open_url_action_handle_event.
        def handle_event(event)
          if event.event_type == :button_press && !@url.to_s.empty?
            launch
          end
        end

        # Port of launcher_open_url_action_get_preferences.
        def preferences
          Adwaita::PreferencesGroup.new.tap do |group|
            group.add(url_row)
          end
        end

        # Port of launcher_open_url_action_serialize_settings.
        def serialize_settings = { "url" => @url }

        # Port of launcher_open_url_action_deserialize_settings.
        def deserialize_settings(settings)
          self.url = settings["url"]
        end

        private

          def launch
            Gtk::UriLauncher.new(@url).launch(active_window, nil) do |launcher, result|
              finish_launch(launcher, result)
            end
          end

          def finish_launch(launcher, result)
            launcher.launch_finish(result)
          rescue Gio::Error => e
            warn "Error showing URL: #{e.message}"
          end

          def active_window = Gio::Application.default&.active_window

          def url_row
            Adwaita::EntryRow.new.tap do |row|
              row.title = "URL"
              row.text = @url.to_s
              row.signal_connect("notify::text") { self.url = row.text }
            end
          end
      end
    end
  end
end
