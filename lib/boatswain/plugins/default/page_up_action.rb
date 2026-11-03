# frozen_string_literal: true

require_relative "../../action"

module Boatswain
  module Plugins
    module Default
      # PageUpAction is the port of
      # src/plugins/default/default-page-up-action.c: the hidden action that
      # sits in the first slot of every subpage and goes back up.
      class PageUpAction < Boatswain::Action
        def initialize
          super
          icon.icon_name = "go-up-symbolic"
        end

        # Port of default_page_up_action_handle_event.
        def handle_event(event)
          if %i[button_press cursor_double_click].include?(event.event_type)
            event.device.pop_page
          end
        end
      end
    end
  end
end
