# frozen_string_literal: true

module Boatswain
  # Actionable is the port of lib/bs-actionable.c/.h: the interface every
  # "has one action, forwards events to it" thing implements — buttons,
  # dials and touchscreen slots upstream.
  #
  # Upstream is a GInterface with one abstract property (action, which the
  # implementer backs however it likes), a default handle_event that just
  # forwards to the action, and an "action-changed" signal implementers emit
  # through bs_actionable_action_changed. This module gives the same three
  # things to whatever Ruby class includes it: a default #handle_event, the
  # #on_action_changed callback list, and #notify_action_changed to fire it.
  # The includer still owns #action and #action= itself, exactly as upstream
  # leaves get_action/set_action to the implementing type.
  module Actionable
    # Port of bs_actionable_handle_event / bs_actionable_default_handle_event.
    def handle_event(event)
      action&.handle_event(event)
    end

    # Port of the "action-changed" signal.
    def on_action_changed(&block)
      action_callbacks << block
      block
    end

    # Port of bs_actionable_action_changed.
    def notify_action_changed
      action_callbacks.dup.each { |callback| callback.call(self) }
    end

    private

      def action_callbacks = @action_callbacks ||= []
  end
end
