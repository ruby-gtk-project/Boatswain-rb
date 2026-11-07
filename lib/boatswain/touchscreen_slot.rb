# frozen_string_literal: true

require_relative "actionable"

module Boatswain
  # TouchscreenSlot is the port of lib/bs-touchscreen-slot.c/.h: one tappable
  # strip of a touchscreen, the action bound to it and the size it draws at.
  #
  # Upstream implements both BsActionable and GdkPaintable: the action is the
  # only state (there is no custom icon, unlike BsButton), and the paintable
  # half just snapshots the action's icon. The Ruby bindings cannot implement
  # GdkPaintable (see Icon), so this is the model only; #on_content_changed
  # stands in for the "invalidate-contents" a widget would redraw on, fired
  # on the same two upstream triggers: the action itself changing, and the
  # current action's icon changing.
  class TouchscreenSlot
    include Actionable

    def initialize(touchscreen:, width:, height:)
      @touchscreen = touchscreen
      @width = width
      @height = height
      @content_callbacks = []
    end

    attr_reader :touchscreen, :width, :height, :action

    def on_content_changed(&block)
      @content_callbacks << block
      block
    end

    # Port of bs_touchscreen_slot_actionable_set_action.
    def action=(value)
      unless @action.equal?(value)
        disconnect_action
        @action = value
        connect_action
        notify_content_changed
        notify_action_changed
      end
    end

    private

      def connect_action
        @action_changed = @action&.on_changed { notify_action_changed }
        @action_icon_changed = @action&.icon&.on_changed { notify_content_changed }
      end

      def disconnect_action
        if @action
          @action.disconnect(@action_changed)
          @action.icon&.disconnect(@action_icon_changed)
        end
      end

      def notify_content_changed
        @content_callbacks.dup.each { |callback| callback.call(self) }
      end
  end
end
