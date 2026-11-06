# frozen_string_literal: true

require_relative "../../action_factory"
require_relative "score_action"

module Boatswain
  module Plugins
    module Gaming
      # ActionFactory is the port of src/plugins/gaming/gaming.js: the
      # "Gaming" plugin's one action, a score counter.
      #
      # Upstream is a GJS plugin loaded by libpeas' JavaScript loader; there
      # is no libpeas or GJS here, so this is a plain ActionFactory subclass
      # like every other plugin in this port.
      class ActionFactory < Boatswain::ActionFactory
        class << self
          def module_name = "gaming"

          def display_name = "Gaming"

          def icon_name = "input-gaming-symbolic"

          def entries
            [
              {
                id:        "gaming-score-action",
                icon_name: "score-symbolic",
                name:      "Score",
              },
            ]
          end
        end

        private

          # Port of vfunc_create_action.
          def action_builders
            {
              "gaming-score-action" => -> { ScoreAction.new },
            }
          end
      end
    end
  end
end
