# frozen_string_literal: true

require_relative "../../action"
require_relative "../../page"

module Boatswain
  module Plugins
    module Default
      # SwitchPageAction is the port of
      # src/plugins/default/default-switch-page-action.c: the "Folder" action,
      # which owns a page of its own and pushes it when pressed.
      class SwitchPageAction < Boatswain::Action
        def initialize(context)
          super()
          @context = context
          @page = Page.new_empty(context)
          icon.icon_name = "folder-symbolic"
        end

        attr_reader :page

        # Port of default_switch_page_action_handle_event.
        def handle_event(event)
          if %i[button_press cursor_double_click].include?(event.event_type)
            event.device.push_page(@page)
          end
        end

        # Port of default_switch_page_action_serialize_settings.
        def serialize_settings
          {}.tap do |settings|
            if @page
              settings["page"] = @page.to_json_data
            end
          end
        end

        # Port of default_switch_page_action_deserialize_settings.
        def deserialize_settings(settings)
          if settings.key?("page")
            @page = Page.from_json(@context, settings["page"])
          end
        end
      end
    end
  end
end
