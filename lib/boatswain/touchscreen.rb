# frozen_string_literal: true

require_relative "device_region"
require_relative "touchscreen_slot"

module Boatswain
  # Touchscreen is the port of lib/bs-touchscreen.c/.h: the region holding a
  # device's touchscreen slots, built up front from an image size and a slot
  # count exactly as bs_touchscreen_new divides the screen width among them.
  #
  # Upstream also owns a BsRenderer and a BsTouchscreenContent that renders the
  # slots' icons into a single frame buffer for the HID upload path; neither
  # has a home in this port (see the gemspec note by the ashpd/glycin
  # dependencies — there is no renderer-equivalent class here), so this is the
  # model and layout half only: the slots themselves, and the
  # "touchscreen-slot-changed" relay bs_touchscreen_new wires up by connecting
  # each slot's "action-changed" to itself.
  class Touchscreen < DeviceRegion
    def initialize(id:, device:, image_info:, n_slots:, **geometry)
      super(id: id, device: device, **geometry)
      @width = image_info.fetch(:width)
      @height = image_info.fetch(:height)
      @slots = Array.new(n_slots) do
        TouchscreenSlot.new(touchscreen: self, width: @width / n_slots, height: @height)
      end
      @slot_changed_callbacks = []
      connect_slots
    end

    attr_reader :width, :height, :slots

    def on_slot_changed(&block)
      @slot_changed_callbacks << block
      block
    end

    # Port of bs_touchscreen_get_slot_position.
    def slot_position(slot) = @slots.index { |candidate| candidate.equal?(slot) }

    # Port of bs_touchscreen_pick_slot.
    def pick_slot(x)
      (x / @width.to_f * @slots.length).floor.clamp(0, @slots.length - 1).then { |position| @slots[position] }
    end

    private

      def connect_slots
        @slots.each do |slot|
          slot.on_action_changed { slot_changed(slot) }
        end
      end

      def slot_changed(slot)
        @slot_changed_callbacks.dup.each { |callback| callback.call(slot) }
      end
  end
end
