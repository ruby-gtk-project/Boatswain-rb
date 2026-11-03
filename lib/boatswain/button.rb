# frozen_string_literal: true

require_relative "empty_action"
require_relative "icon"

module Boatswain
  # Button is the port of lib/bs-button.c: one physical key of a device, the
  # action bound to it and the icon it shows.
  #
  # Upstream implements BsActionable, whose default handle_event forwards the
  # event to the action (lib/bs-actionable.c), and emits "action-changed" and
  # "icon-changed" so the grid above it can save and redraw. The same two
  # notifications are kept here as callback lists.
  class Button
    def initialize(device:, region:, position:, icon_width:, icon_height:)
      @device = device
      @region = region
      @position = position
      @icon_width = icon_width
      @icon_height = icon_height
      @pressed = false
      @action_callbacks = []
      @icon_callbacks = []
    end

    attr_reader :device, :region, :position, :icon_width, :icon_height, :action, :custom_icon

    def pressed? = @pressed

    def on_action_changed(&block)
      @action_callbacks << block
      block
    end

    def on_icon_changed(&block)
      @icon_callbacks << block
      block
    end

    # Port of bs_button_get_icon: the custom icon wins, then the action's.
    def icon
      if @custom_icon
        @custom_icon
      else
        @action&.icon
      end
    end

    # Port of bs_button_actionable_set_action. Assigning an action drops the
    # custom icon, as upstream's `if (action) remove_custom_icon (self)` does.
    def action=(value)
      unless @action.equal?(value)
        if value
          remove_custom_icon
        end
        disconnect_action
        @action = value
        connect_action
        update_relative_icon
        notify_icon_changed
        notify_action_changed
      end
    end

    # Port of bs_button_set_custom_icon.
    def custom_icon=(value)
      unless @custom_icon.equal?(value)
        remove_custom_icon
        @custom_icon = value
        connect_custom_icon
        update_relative_icon
        notify_icon_changed
      end
    end

    # Port of bs_button_actionable_handle_event: the press state is tracked
    # here, and the event itself goes to the action.
    def handle_event(event)
      @action&.handle_event(event)

      case event.event_type
      when :button_press then self.pressed = true
      when :button_release then self.pressed = false
      end
    end

    attr_writer :pressed

    private

      # Port of update_relative_icon: a custom icon falls back to the action's
      # icon for anything it does not set itself.
      def update_relative_icon
        @custom_icon&.relative = @action&.icon
      end

      def connect_action
        @action_changed = @action&.on_changed { notify_action_changed }
        @action_icon_changed = @action&.icon&.on_changed { notify_icon_changed }
      end

      def disconnect_action
        if @action
          @action.disconnect(@action_changed)
          @action.icon&.disconnect(@action_icon_changed)
        end
      end

      def connect_custom_icon
        @custom_icon_changed = @custom_icon&.on_changed { notify_icon_changed }
      end

      def remove_custom_icon
        if @custom_icon
          @custom_icon.disconnect(@custom_icon_changed)
          @custom_icon = nil
        end
      end

      def notify_action_changed
        @action_callbacks.dup.each { |callback| callback.call(self) }
      end

      def notify_icon_changed
        @icon_callbacks.dup.each { |callback| callback.call(self, icon) }
      end
  end
end
