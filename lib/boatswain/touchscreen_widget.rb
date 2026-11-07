# frozen_string_literal: true

require "adwaita"

require_relative "touchscreen_slot_widget"

module Boatswain
  # TouchscreenWidget is the port of src/bs-touchscreen-widget.c and its .ui:
  # the touchscreen region in the device editor, showing every slot in a flow
  # box sized to the touchscreen's own slot count.
  #
  # Upstream backs a GtkPicture with the touchscreen content's rendered
  # background and overlays the slots flow box on top of it; the background
  # paintable comes from BsTouchscreenContent/BsRenderer, which Touchscreen's
  # own comment documents as out of scope for this port, so only the slots
  # flow box is built here, on an Adwaita::Bin exactly as ButtonGridWidget
  # uses one in place of upstream's custom GtkWidget layout.
  class TouchscreenWidget < Adwaita::Bin
    def initialize(touchscreen, selection_controller)
      super()
      @touchscreen = touchscreen
      @selection_controller = selection_controller
      add_css_class("card")
      build
    end

    attr_reader :touchscreen

    private

      def build
        tap do |widget|
          widget.child = flowbox

          flowbox.tap do |box|
            box.min_children_per_line = @touchscreen.slots.length
            box.max_children_per_line = @touchscreen.slots.length

            @touchscreen.slots.each { |slot| box.append(TouchscreenSlotWidget.new(slot)) }

            box.signal_connect("selected-children-changed") { on_selected_children_changed }
          end

          @selection_controller.on_selection_changed { |controller| on_selection_changed(controller) }
        end
      end

      # Port of on_slots_flowbox_selected_children_changed_cb.
      def on_selected_children_changed
        flowbox.selected_children.first.then do |child|
          if child
            @selection_controller.select(@touchscreen, child.slot)
          end
        end
      end

      # Port of on_selection_controller_selection_changed_cb.
      def on_selection_changed(controller)
        controller.selection.then do |selection|
          unless selection && selection.first.equal?(@touchscreen)
            flowbox.unselect_all
          end
        end
      end

      def flowbox
        @flowbox ||= Gtk::FlowBox.new.tap do |box|
          box.hexpand = true
          box.vexpand = true
          box.selection_mode = :single
          box.homogeneous = true
        end
      end
  end
end
