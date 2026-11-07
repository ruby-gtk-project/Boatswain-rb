# frozen_string_literal: true

require "gtk4"

require_relative "icon_widget"

module Boatswain
  # TouchscreenSlotWidget is the port of src/bs-touchscreen-slot-widget.c and
  # its .ui: one tappable strip in the touchscreen's flow box.
  #
  # Upstream's GtkPicture binds straight to the slot, which is itself a
  # GdkPaintable; TouchscreenSlot documents why the Ruby bindings cannot do
  # that (see its own comment), so this draws the slot's action's icon with
  # an IconWidget instead, exactly as ButtonWidget does for a button, and
  # redraws it on the same "content-changed" signal TouchscreenSlot exposes
  # for that purpose.
  class TouchscreenSlotWidget < Gtk::FlowBoxChild
    def initialize(slot)
      super()
      @slot = slot
      self.hexpand = true
      self.vexpand = true
      self.width_request = 1
      add_css_class("bs-touchscreen-slot-widget")
      build
    end

    attr_reader :slot

    private

      def build
        tap do |widget|
          widget.child = icon_widget

          @slot.on_content_changed { update_icon }
        end
      end

      def icon_widget
        @icon_widget ||= IconWidget.new(@slot.action&.icon, size: @slot.height).tap do |widget|
          widget.width_request = @slot.width
        end
      end

      def update_icon
        icon_widget.icon = @slot.action&.icon
      end
  end
end
