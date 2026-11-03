# frozen_string_literal: true

require_relative "../../button_grid"
require_relative "../../device"
require_relative "../../rb/version"

module Boatswain
  module Devices
    module Mock
      # Device is the port of lib/devices/mock/mock-device.c: the fake device
      # the app offers when no real hardware is plugged in, so the editor has
      # something to edit.
      #
      # Upstream publishes two models through its models_vtable, both
      # button-only; the numbers below are that table verbatim. The image
      # format and renderer flags are part of the HID upload path, which this
      # port does not have, so only the pixel size each model draws its keys at
      # is carried over.
      class Device < Boatswain::Device
        MODELS = {
          hangar_mild: {
            name:       "Feaneron Hangar Mild",
            prefix:     "mild",
            n_buttons:  15,
            columns:    5,
            image_info: { width: 72, height: 72 },
          },
          hangar_xl:   {
            name:       "Feaneron Hangar XL",
            prefix:     "xl",
            n_buttons:  32,
            columns:    8,
            image_info: { width: 96, height: 96 },
          },
        }.freeze

        # Port of get_serial_number_fake: one counter per model.
        @counters = Hash.new(0)

        class << self
          def next_serial_number(model)
            @counters[model] += 1
            "#{MODELS.fetch(model)[:prefix]}-#{@counters[model] - 1}"
          end
        end

        def initialize(context, model: :hangar_mild)
          @model = model
          @model_info = MODELS.fetch(model)
          @serial_number = self.class.next_serial_number(model)
          super(context)
        end

        attr_reader :model

        def name = @model_info[:name]

        def serial_number = @serial_number

        def firmware_version = Boatswain::Rb::VERSION

        # Port of mock_device_create_layout, which asks the layout builder for
        # one button grid in row 0.
        def create_layout
          [
            ButtonGrid.new(
              id:           Page::MAIN_BUTTON_GRID,
              device:       self,
              image_info:   @model_info[:image_info],
              n_buttons:    @model_info[:n_buttons],
              grid_columns: @model_info[:columns],
              column:       0,
              row:          0,
            ),
          ]
        end
      end
    end
  end
end
