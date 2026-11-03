# frozen_string_literal: true

require_relative "button"
require_relative "device_region"
require_relative "observable_list"

module Boatswain
  # ButtonGrid is the port of lib/bs-button-grid.c: the region holding a
  # device's keys, laid out in a fixed number of columns.
  #
  # Upstream creates every button up front and re-emits each button's
  # "action-changed" and "icon-changed" as its own "button-changed", which is
  # what makes the device save the page; the same relay is kept here.
  class ButtonGrid < DeviceRegion
    def initialize(id:, device:, image_info:, n_buttons:, grid_columns:, **geometry)
      super(id: id, device: device, **geometry)
      @image_info = image_info
      @grid_columns = grid_columns
      @callbacks = []
      @buttons = ObservableList.new(build_buttons(n_buttons))
    end

    attr_reader :buttons, :grid_columns, :image_info

    def on_button_changed(&block)
      @callbacks << block
      block
    end

    private

      def build_buttons(n_buttons)
        Array.new(n_buttons) { |position| build_button(position) }
      end

      def build_button(position)
        Button.new(
          device:      device,
          region:      self,
          position:    position,
          icon_width:  @image_info[:width],
          icon_height: @image_info[:height],
        ).tap do |button|
          button.on_action_changed { |changed| button_changed(changed) }
          button.on_icon_changed { |changed, _icon| button_changed(changed) }
        end
      end

      def button_changed(button)
        @callbacks.dup.each { |callback| callback.call(button) }
      end
  end
end
