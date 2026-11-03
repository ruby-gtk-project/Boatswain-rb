# frozen_string_literal: true

require "adwaita"

require_relative "empty_action"
require_relative "icon_widget"

module Boatswain
  # ButtonWidget is the port of src/bs-button-widget.c and its .ui: one key of
  # the grid in the device editor, showing that key's icon and acting as both a
  # drag source and a drop target so two keys can swap what they hold.
  #
  # Upstream puts the BsIcon paintable straight into a GtkPicture; the icon is
  # not a paintable here, so an IconWidget draws it (see IconWidget). Upstream
  # also hands the BsButton itself across the drag as a typed GValue, which the
  # Ruby bindings cannot build, so the drag carries the button's region id and
  # position as text and the drop target looks the button back up — the keys
  # that end up swapped are the same.
  class ButtonWidget < Gtk::FlowBoxChild
    MIME_TYPE = "text/plain"

    def initialize(button)
      super()
      @button = button
      add_css_class("bs-button-widget")
      build
    end

    attr_reader :button

    private

      def build
        tap do |widget|
          widget.child = icon_widget
          widget.add_controller(drag_source)
          widget.add_controller(drop_target)

          @button.on_icon_changed { |_button, icon| icon_widget.icon = icon }
          @button.on_action_changed { update_drag_sensitivity }

          update_drag_sensitivity
        end
      end

      def icon_widget
        @icon_widget ||= IconWidget.new(@button.icon, size: @button.icon_width).tap do |widget|
          widget.halign = :center
          widget.height_request = @button.icon_height
          widget.add_css_class("card")
        end
      end

      # Port of on_drag_prepare_cb.
      def drag_source
        @drag_source ||= Gtk::DragSource.new.tap do |source|
          source.actions = :move
          source.signal_connect("prepare") { |_source, _x, _y| drag_content }
        end
      end

      def drag_content
        Gdk::ContentProvider.new(MIME_TYPE, GLib::Bytes.new(drag_payload))
      end

      def drag_payload = "#{@button.region.id}\t#{@button.position}"

      # Port of on_drop_target_drop_cb: the two keys swap actions and custom
      # icons, and the key that was dropped on becomes the selected one.
      def drop_target
        @drop_target ||= Gtk::DropTarget.new(String, :move).tap do |target|
          target.preload = true
          target.signal_connect("drop") { |_target, value, _x, _y| on_drop(value) }
        end
      end

      def on_drop(payload)
        dragged_button(payload).then do |dragged|
          if dragged.nil? || dragged.equal?(@button)
            false
          else
            swap_with(dragged)
            true
          end
        end
      end

      def dragged_button(payload)
        payload.to_s.split("\t").then do |region_id, position|
          @button.device.region(region_id).then do |region|
            region&.buttons&.[](position.to_i)
          end
        end
      end

      def swap_with(dragged)
        dragged.action.then do |dragged_action|
          dragged.custom_icon.then do |dragged_icon|
            @button.custom_icon.then do |dropped_icon|
              @button.action = dragged_action
              dragged.custom_icon = dropped_icon
              @button.custom_icon = dragged_icon
            end
          end
        end
        select_myself_in_parent_flowbox
      end

      def select_myself_in_parent_flowbox
        parent.then do |flowbox|
          if flowbox.is_a?(Gtk::FlowBox)
            flowbox.select_child(self)
          end
        end
      end

      # Port of is_move_page_up_action and on_button_action_changed_cb: the Go
      # Up key cannot be dragged away or replaced by a drop.
      def update_drag_sensitivity
        move_page_up?.then do |fixed|
          drag_source.propagation_phase = phase_for(fixed)
          drop_target.actions = actions_for(fixed)
        end
      end

      def phase_for(fixed)
        if fixed
          :none
        else
          :bubble
        end
      end

      def actions_for(fixed)
        if fixed
          0
        else
          Gdk::DragAction::MOVE
        end
      end

    # The id upstream tests for is "default-switch-page-action", not the
    # "default-page-up-action" that bs-page.c now puts in the first slot of a
    # subpage. That is upstream's own condition and it is reproduced as
    # written; changing it here would change which keys can be dragged.
      def move_page_up?
        @button.action.then do |action|
          if action.nil? || action.is_a?(EmptyAction)
            false
          else
            fixed_first_slot?(action)
          end
        end
      end

      def fixed_first_slot?(action)
        @button.position.zero? &&
          !@button.device.active_page.root? &&
          action.factory&.module_name == "default" &&
          action.id == "default-switch-page-action"
      end
  end
end
