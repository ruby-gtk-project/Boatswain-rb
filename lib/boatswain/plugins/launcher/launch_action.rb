# frozen_string_literal: true

require "adwaita"

require_relative "../../action"
require_relative "launch_preferences"

module Boatswain
  module Plugins
    module Launcher
      # LaunchAction is the port of
      # src/plugins/launcher/launcher-launch-action.c: launches a chosen
      # application, optionally with a saved list of files.
      class LaunchAction < Boatswain::Action
        FALLBACK_ICON = "application-x-executable-symbolic"

        def initialize
          super
          @files = []
          icon.icon_name = FALLBACK_ICON
        end

        attr_reader :app, :files

        # Port of launcher_launch_action_set_app.
        def app=(value)
          @app = value
          update_icon
        end

        # Port of launcher_launch_action_handle_event.
        def handle_event(event)
          if event.event_type == :button_press && @app
            launch
          end
        end

        # Port of launcher_launch_action_get_preferences: the group is built
        # once and reused, as upstream's ref_sink does.
        def preferences
          @preferences ||= LaunchPreferences.new(self)
        end

        # Port of launcher_launch_action_serialize_settings.
        def serialize_settings
          { "app" => @app&.id, "files" => @files.map(&:path) }
        end

        # Port of launcher_launch_action_deserialize_settings: upstream looks
        # the saved desktop id up among the apps that should be shown.
        def deserialize_settings(settings)
          @app = find_app(settings["app"])
          @files = Array(settings["files"]).map { |path| Gio::File.new_for_path(path) }
          update_icon
          @preferences&.update_app_info
        end

        private

          def find_app(app_id)
            if app_id
              Gio::AppInfo.all.find { |app| app.should_show? && app.id == app_id }
            end
          end

          def launch
            @app.launch(@files, launch_context)
          rescue Gio::Error => e
            warn "Unable to launch application #{@app.name}: #{e.message}"
          end

          def launch_context
            Gdk::Display.default.app_launch_context
          end

          # Port of launcher_launch_action_update_icon.
          def update_icon
            icon.icon_name = themed_icon_name || FALLBACK_ICON
          end

          def themed_icon_name
            @app&.icon.then do |app_icon|
              if app_icon.is_a?(Gio::ThemedIcon)
                app_icon.names.first
              end
            end
          end
      end
    end
  end
end
