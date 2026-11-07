# frozen_string_literal: true

require "gudev"

module Boatswain
  module Devices
    module Usb
      # HardwareDetector is the read-only half of
      # lib/devices/elgato/elgato-device-provider.c and
      # lib/devices/loupedeck/loupedeck-device-provider.c: the GUdev
      # enumeration that upstream uses to notice a Stream Deck, Loupedeck or
      # Razer Stream Controller plugged into USB.
      #
      # Upstream turns a match into a live BsDevice that it then talks HID to
      # over libusb. The libusb half (opening the device file, claiming the
      # interface, uploading key images) is the hardware half of the device
      # layer and is not part of this port — Device#create_layout has no
      # subclass that owns a HID handle, only Devices::Mock::Device. What
      # stays genuinely portable is the USB enumeration itself, so this class
      # does exactly that and nothing past it: ask GUdev which of the known
      # vendor/product pairs are plugged in right now, the same filter
      # is_gudev_device_suitable and the two providers' device_vtable tables
      # apply, and hand back what it found for DeviceManager to log.
      class HardwareDetector
        ELGATO_VENDOR_ID = 0x0fd9
        LOUPEDECK_VENDOR_ID = 0x2ec2
        RAZER_VENDOR_ID = 0x1532

        # The product IDs upstream's find_elgato_device_gtype, in the same
        # order, and the one entry find_razer_device_gtype carries (upstream's
        # own find_loupedeck_device_gtype vtable is empty).
        KNOWN_PRODUCTS = {
          ELGATO_VENDOR_ID    => {
            0x0060 => "Stream Deck Original",
            0x0063 => "Stream Deck Mini",
            0x006c => "Stream Deck XL",
            0x006d => "Stream Deck Original V2",
            0x0080 => "Stream Deck MK2",
            0x0084 => "Stream Deck Plus",
            0x0086 => "Stream Deck Pedal",
            0x008f => "Stream Deck XL",
            0x0090 => "Stream Deck Mini",
            0x009a => "Stream Deck Neo",
          },
          LOUPEDECK_VENDOR_ID => {},
          RAZER_VENDOR_ID     => { 0x0d09 => "Stream Controller X" },
        }.freeze

        Found = Struct.new(:name, :vendor_id, :product_id, :device_file, keyword_init: true)

        # Port of enumerate_devices + is_gudev_device_suitable + add_device's
        # vendor/product switch, with the libusb open and the BsDevice
        # construction cut off at the point this port has nothing to do them
        # with.
        def detect
          enumerator.execute.filter_map { |device| recognize(device) }
        end

        private

          def client = @client ||= GUdev::Client.new(["usb"])

          def enumerator
            GUdev::Enumerator.new(client).tap do |enumerator|
              enumerator.add_match_property("DEVTYPE", "usb_device")
            end
          end

          # Port of is_gudev_device_suitable.
          def suitable?(device)
            device.devtype == "usb_device" && !device.device_file.nil?
          end

          def recognize(device)
            if suitable?(device)
              model_for(device).then do |name|
                if name
                  Found.new(
                    name:        name,
                    vendor_id:   device.get_sysfs_attr("idVendor"),
                    product_id:  device.get_sysfs_attr("idProduct"),
                    device_file: device.device_file,
                  )
                end
              end
            end
          end

          def model_for(device)
            vendor_id = device.get_sysfs_attr("idVendor")
            product_id = device.get_sysfs_attr("idProduct")
            if vendor_id && product_id
              KNOWN_PRODUCTS.fetch(Integer(vendor_id, 16), {})[Integer(product_id, 16)]
            end
          end
      end
    end
  end
end
