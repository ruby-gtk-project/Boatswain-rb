# frozen_string_literal: true

require_relative "devices/mock/device_provider"
require_relative "devices/usb/hardware_detector"
require_relative "observable_list"

module Boatswain
  # DeviceManager is the port of lib/bs-device-manager.c: the one list of
  # devices the window shows, fed by every device provider.
  #
  # Upstream discovers providers as libpeas extensions — Elgato and Loupedeck
  # over USB HID, plus the mock one. Those two HID providers' libusb half (the
  # open, the key image upload, BsDevice itself) is the hardware half of the
  # device layer and is not part of this port, so the mock provider is the
  # only one in PROVIDERS; adding a provider is adding it there.
  #
  # Their GUdev enumeration half is genuinely portable, though, so `load` also
  # runs Devices::Usb::HardwareDetector and logs what real hardware it finds —
  # real USB discovery through the gudev gem, stopping short of the HID
  # upload the mock device stands in for.
  class DeviceManager
    PROVIDERS = [Devices::Mock::DeviceProvider].freeze

    def initialize(context)
      @context = context
      @devices = ObservableList.new
      @providers = PROVIDERS.map { |provider| provider.new(context) }
      @hardware_detector = Devices::Usb::HardwareDetector.new
    end

    attr_reader :devices

    # Port of bs_device_manager_load: every provider's devices land in the one
    # list, and each device loads its profiles.
    def load
      log_detected_hardware
      @providers.each do |provider|
        provider.load.each do |device|
          device.load
          @devices.append(device)
        end
      end
      @devices
    end

    private

      def log_detected_hardware
        @hardware_detector.detect.each do |found|
          warn "Found #{found.name} (#{found.vendor_id}:#{found.product_id} at " \
            "#{found.device_file}), but this port has no HID upload path for " \
            "real hardware; use BOATSWAIN_N_DEVICES for a device to edit."
        end
      rescue GLib::Error, Gio::Error => e
        warn "USB hardware detection unavailable: #{e.message}"
      end
  end
end
