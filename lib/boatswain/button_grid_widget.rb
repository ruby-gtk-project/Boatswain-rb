# frozen_string_literal: true

require "adwaita"

require_relative "button_widget"
require_relative "events"

module Boatswain
  # ButtonGridWidget is the port of src/bs-button-grid-widget.c and its .ui:
  # the flow box of keys for one button grid region, with the grid's own column
  # count pinned as both the minimum and the maximum children per line.
  #
  # Upstream is a GtkWidget with a bin layout wrapping the flow box; the Ruby
  # bindings cannot set a layout manager type on a widget class, so this is an
  # Adwaita::Bin, which is the same single-child container.
  class ButtonGridWidget < Adwaita::Bin
    SPACING = 18

    def initialize(button_grid, selection_controller)
      super()
      @button_grid = button_grid
      @selection_controller = selection_controller
      add_css_class("bs-button-grid-widget")
      build
    end

    attr_reader :button_grid

    private

      def build
        tap do |widget|
          widget.child = flowbox

          flowbox.tap do |box|
            box.min_children_per_line = @button_grid.grid_columns
            box.max_children_per_line = @button_grid.grid_columns

            @button_grid.buttons.each { |button| box.append(ButtonWidget.new(button)) }

            box.signal_connect("child-activated") { |_box, child| on_child_activated(child) }
            box.signal_connect("selected-children-changed") { on_selected_children_changed }
          end

          @selection_controller.on_selection_changed { |controller| on_selection_changed(controller) }
        end
      end

      # Port of on_flowbox_child_activated_cb: a double click in the editor
      # runs the key's action through the synthetic cursor event.
      def on_child_activated(child)
        child.button.action.then do |action|
          if action
            action.handle_event(CursorEvent.double_click(@button_grid.device))
          end
        end
      end

      # Port of on_flowbox_selected_children_changed_cb.
      def on_selected_children_changed
        flowbox.selected_children.first.then do |child|
          if child
            @selection_controller.select(@button_grid, child.button)
          end
        end
      end

      # Port of on_selection_controller_selection_changed_cb: a selection made
      # in another region clears this one's.
      def on_selection_changed(controller)
        controller.selection.then do |selection|
          unless selection && selection.first.equal?(@button_grid)
            flowbox.unselect_all
          end
        end
      end

      def flowbox
        @flowbox ||= Gtk::FlowBox.new.tap do |box|
          box.halign = :center
          box.valign = :center
          box.homogeneous = true
          box.activate_on_single_click = false
          box.selection_mode = :single
          box.column_spacing = SPACING
          box.row_spacing = SPACING
        end
      end
  end
end
