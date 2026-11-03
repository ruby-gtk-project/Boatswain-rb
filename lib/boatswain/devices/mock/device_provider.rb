# frozen_string_literal: true

require_relative "../../observable_list"
require_relative "device"

module Boatswain
  module Devices
    module Mock
      # DeviceProvider is the port of
      # lib/devices/mock/mock-device-provider.c: it hands the device manager
      # as many fake devices as BOATSWAIN_N_DEVICES asks for, one of each
      # model in turn, defaulting to one.
      class DeviceProvider
        MODELS = %i[hangar_mild hangar_xl].freeze

        def initialize(context)
          @context = context
          @devices = ObservableList.new
        end

        attr_reader :devices

        def load
          n_devices.times do |index|
            @devices.append(Device.new(@context, model: MODELS[index % MODELS.length]))
          end
          @devices
        end

        private

          def n_devices
            [ENV.fetch("BOATSWAIN_N_DEVICES", "1").to_i, 0].max
          end
      end
    end
  end
end
