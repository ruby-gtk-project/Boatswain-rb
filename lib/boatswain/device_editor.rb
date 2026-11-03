# frozen_string_literal: true

require "adwaita"

require_relative "button_editor"
require_relative "button_grid"
require_relative "button_grid_widget"
require_relative "selection_controller"

module Boatswain
  # DeviceEditor is the port of src/bs-device-editor.c and its .ui: one page
  # per device, with that device's regions laid out in a grid on the left and
  # the editor for whatever is selected on the right.
  #
  # The regions are attached at the row, column and spans each region declares,
  # which is how upstream's build_regions places them. Only button grids are
  # built here: the dial grid and touchscreen widgets are separate components
  # and are not part of this round, so a device with those regions shows its
  # button grids and nothing where they would be.
  class DeviceEditor < Adwaita::Bin
    SIDEBAR_WIDTH = 280
    CLAMP_MAXIMUM = 700
    CLAMP_TIGHTENING = 500
    SPACING = 24

    def initialize(context, device)
      super()
      @context = context
      @device = device
      @selection_controller = SelectionController.new
      @region_widgets = {}
      add_css_class("bs-device-editor")
      build
    end

    attr_reader :device

    private

      def build
        tap do |editor|
          editor.child = split_view

          split_view.tap do |view|
            view.sidebar_position = :end
            view.content = content_toolbar_view
            view.sidebar = editor_bin
          end

          content_toolbar_view.tap do |view|
            view.add_top_bar(content_header_bar)
            view.content = clamp
          end

          clamp.child = regions_grid

          empty_toolbar_view.tap do |view|
            view.add_top_bar(empty_header_bar)
            view.content = empty_status_page
          end

          @selection_controller.on_selection_changed { |controller| on_selection_changed(controller) }

          build_regions
        end
      end

      # Port of build_regions.
      def build_regions
        @device.regions.each do |region|
          if region.is_a?(ButtonGrid)
            add_region(region, ButtonGridWidget.new(region, @selection_controller))
          end
        end
      end

      def add_region(region, widget)
        regions_grid.attach(
          widget,
          region.column,
          region.row,
          region.column_span,
          region.row_span,
        )
        @region_widgets[region] = widget
      end

      # Port of on_selection_controller_selection_changed_cb and
      # set_selected_item.
      def on_selection_changed(controller)
        controller.selection.then do |selection|
          if selection
            set_selected_item(selection.last)
          end
        end
      end

      def set_selected_item(item)
        if item.is_a?(Button)
          editor_bin.child = ButtonEditor.new(@context, item)
        else
          editor_bin.child = empty_toolbar_view
        end
      end

      def split_view
        @split_view ||= Adwaita::OverlaySplitView.new
      end

      def content_toolbar_view
        @content_toolbar_view ||= Adwaita::ToolbarView.new.tap do |view|
          view.top_bar_style = :flat
        end
      end

      def content_header_bar
        @content_header_bar ||= Adwaita::HeaderBar.new.tap do |bar|
          bar.show_title = false
          bar.show_start_title_buttons = false
          bar.show_end_title_buttons = false
        end
      end

      def clamp
        @clamp ||= Adwaita::Clamp.new.tap do |clamp|
          clamp.hexpand = true
          clamp.margin_top = SPACING
          clamp.margin_bottom = SPACING
          clamp.margin_start = SPACING
          clamp.margin_end = SPACING
          clamp.maximum_size = CLAMP_MAXIMUM
          clamp.tightening_threshold = CLAMP_TIGHTENING
        end
      end

      def regions_grid
        @regions_grid ||= Gtk::Grid.new.tap do |grid|
          grid.halign = :center
          grid.valign = :center
          grid.column_spacing = SPACING
          grid.row_spacing = SPACING
        end
      end

      def editor_bin
        @editor_bin ||= Adwaita::Bin.new.tap do |bin|
          bin.hexpand = true
          bin.width_request = SIDEBAR_WIDTH
          bin.child = empty_toolbar_view
        end
      end

      def empty_toolbar_view
        @empty_toolbar_view ||= Adwaita::ToolbarView.new.tap do |view|
          view.top_bar_style = :flat
        end
      end

      def empty_header_bar
        @empty_header_bar ||= Adwaita::HeaderBar.new.tap do |bar|
          bar.show_title = false
          bar.show_start_title_buttons = false
        end
      end

      def empty_status_page
        @empty_status_page ||= Adwaita::StatusPage.new.tap do |page|
          page.title = "Select a button"
        end
      end
  end
end
