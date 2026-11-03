# frozen_string_literal: true

require_relative "../../action_factory"
require_relative "brightness_action"
require_relative "multi_action"
require_relative "page_up_action"
require_relative "switch_page_action"
require_relative "switch_profile_action"

module Boatswain
  module Plugins
    module Default
      # ActionFactory is the port of
      # src/plugins/default/default-action-factory.c: the "Stream Deck"
      # plugin's catalogue of five actions, the last of them hidden.
      #
      # The module name, display name and plugin icon come from
      # src/plugins/default/default.plugin.desktop.in.
      class ActionFactory < Boatswain::ActionFactory
        class << self
          def module_name = "default"

          def display_name = "Stream Deck"

          def icon_name = "dialpad-symbolic"

          def entries
            [
              { id: "default-switch-page-action", icon_name: "folder-symbolic", name: "Folder" },
              {
                id:        "default-switch-profile-action",
                icon_name: "view-list-bullet-symbolic",
                name:      "Switch Profile",
              },
              {
                id:        "default-brightness-action",
                icon_name: "display-brightness-symbolic",
                name:      "Brightness",
              },
              {
                id:        "default-multi-action",
                icon_name: "stacked-plates-symbolic",
                name:      "Multiple Actions",
              },
              {
                id:        "default-page-up-action",
                icon_name: "go-up-symbolic",
                name:      "Go Up",
                hidden:    true,
              },
            ]
          end
        end

        private

          # Port of default_action_factory_create_action.
          def action_builders
            {
              "default-switch-page-action"    => -> { SwitchPageAction.new(context) },
              "default-switch-profile-action" => -> { SwitchProfileAction.new(context) },
              "default-brightness-action"     => -> { BrightnessAction.new },
              "default-multi-action"          => -> { MultiAction.new(context) },
              "default-page-up-action"        => -> { PageUpAction.new },
            }
          end
      end
    end
  end
end
