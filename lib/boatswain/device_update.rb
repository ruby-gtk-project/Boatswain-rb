# frozen_string_literal: true

module Boatswain
  # DeviceUpdate is the port of lib/bs-device-update.c/.h: the set of buttons
  # and touchscreen regions a device needs to redraw, batched up over one
  # pass before the device uploads them.
  #
  # Upstream builds this as two GPtrArrays the caller fills through
  # #add_button and #add_touchscreen_region, then seals before reading
  # either array back; every accessor asserts on the seal the way upstream's
  # g_assert(self->sealed) does. Button updates de-duplicate by identity, and
  # a second region on the same touchscreen merges into the first one's
  # rectangle instead of adding a second entry — both ported verbatim below.
  class DeviceUpdate
    SealedError = Class.new(StandardError)
    UnsealedError = Class.new(StandardError)

    TouchscreenUpdate = Struct.new(:touchscreen, :region)

    def initialize
      @sealed = false
      @button_updates = []
      @touchscreen_updates = []
    end

    def sealed? = @sealed

    # Port of bs_device_update_add_button.
    def add_button(button)
      assert_unsealed
      unless @button_updates.include?(button)
        @button_updates << button
      end
    end

    # Port of bs_device_update_add_touchscreen_region: a second region on an
    # already-tracked touchscreen is unioned into its existing rectangle.
    def add_touchscreen_region(touchscreen, region)
      assert_unsealed
      existing = @touchscreen_updates.find { |update| update.touchscreen.equal?(touchscreen) }
      if existing
        existing.region = union(existing.region, region)
      else
        @touchscreen_updates << TouchscreenUpdate.new(touchscreen, region)
      end
    end

    # Port of bs_device_update_seal.
    def seal
      assert_unsealed
      @sealed = true
    end

    # Port of bs_device_update_get_button_updates.
    def button_updates
      assert_sealed
      @button_updates
    end

    # Port of bs_device_update_get_touchscreen_updates.
    def touchscreen_updates
      assert_sealed
      @touchscreen_updates
    end

    private

      # Port of graphene_rect_union, in the one-axis-at-a-time terms a plain
      # Ruby Hash region ({x:, y:, width:, height:}) can express.
      def union(first, second)
        x = [first[:x], second[:x]].min
        y = [first[:y], second[:y]].min
        right = [first[:x] + first[:width], second[:x] + second[:width]].max
        bottom = [first[:y] + first[:height], second[:y] + second[:height]].max
        { x: x, y: y, width: right - x, height: bottom - y }
      end

      def assert_sealed
        unless @sealed
          raise UnsealedError, "DeviceUpdate has not been sealed"
        end
      end

      def assert_unsealed
        if @sealed
          raise SealedError, "DeviceUpdate is already sealed"
        end
      end
  end
end
