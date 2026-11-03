# frozen_string_literal: true

require_relative "../../action_factory"
require_relative "launch_action"
require_relative "open_file_action"
require_relative "open_url_action"

module Boatswain
  module Plugins
    module Launcher
      # ActionFactory is the port of
      # src/plugins/launcher/launcher-action-factory.c: the "Launcher"
      # plugin's catalogue of three actions.
      #
      # The module name, display name and plugin icon come from
      # src/plugins/launcher/launcher.plugin.desktop.in.
      class ActionFactory < Boatswain::ActionFactory
        class << self
          def module_name = "launcher"

          def display_name = "Launcher"

          def icon_name = "system-run-symbolic"

          def entries
            [
              {
                id:        "launch-action",
                icon_name: "app-launch-symbolic",
                name:      "Launch Application",
              },
              {
                id:        "launcher-open-file-action",
                icon_name: "folder-documents-symbolic",
                name:      "Open File",
              },
              {
                id:        "launcher-open-url-action",
                icon_name: "open-link-symbolic",
                name:      "Open URL",
              },
            ]
          end
        end

        private

          # Port of launcher_action_factory_create_action.
          def action_builders
            {
              "launch-action"             => -> { LaunchAction.new },
              "launcher-open-file-action" => -> { OpenFileAction.new },
              "launcher-open-url-action"  => -> { OpenUrlAction.new },
            }
          end
      end
    end
  end
end
