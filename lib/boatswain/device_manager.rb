# frozen_string_literal: true

require_relative "devices/mock/device_provider"
require_relative "observable_list"

module Boatswain
  # DeviceManager is the port of lib/bs-device-manager.c: the one list of
  # devices the window shows, fed by every device provider.
  #
  # Upstream discovers providers as libpeas extensions — Elgato and Loupedeck
  # over USB HID, plus the mock one. Those two HID providers are the hardware
  # half of the device layer and are not part of this port, so the mock
  # provider is the only one registered here; adding a provider is adding it to
  # PROVIDERS.
  class DeviceManager
    PROVIDERS = [Devices::Mock::DeviceProvider].freeze

    def initialize(context)
      @context = context
      @devices = ObservableList.new
      @providers = PROVIDERS.map { |provider| provider.new(context) }
    end

    attr_reader :devices

    # Port of bs_device_manager_load: every provider's devices land in the one
    # list, and each device loads its profiles.
    def load
      @providers.each do |provider|
        provider.load.each do |device|
          device.load
          @devices.append(device)
        end
      end
      @devices
    end
  end
end
