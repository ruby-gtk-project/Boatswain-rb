# frozen_string_literal: true

module Boatswain
  # Dial is the port of lib/bs-dial.c/.h: one physical dial of a device — its
  # position in the dial grid and whether it is currently pressed down.
  #
  # Upstream is a plain GObject with a device, a position (both construct-only
  # and exposed only through C getters, not GObject properties) and a
  # "pressed" property that notifies on change. The same shape is kept here:
  # device/position are read-only, and #pressed= only fires its callbacks when
  # the value actually flips, exactly as bs_dial_set_pressed's early return on
  # an unchanged value does.
  class Dial
    def initialize(device:, position:)
      @device = device
      @position = position
      @pressed = false
      @callbacks = []
    end

    attr_reader :device, :position

    def pressed? = @pressed

    # Port of the "pressed" property's notify, which only fires on a change.
    def on_pressed_changed(&block)
      @callbacks << block
      block
    end

    # Port of bs_dial_set_pressed.
    def pressed=(value)
      unless @pressed == value
        @pressed = value
        @callbacks.dup.each { |callback| callback.call(self) }
      end
    end
  end
end
