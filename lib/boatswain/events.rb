# frozen_string_literal: true

module Boatswain
  # Event is the port of lib/bs-events.h and lib/bs-events.c: the hardware (and
  # synthetic) events an action can be asked to handle.
  #
  # Upstream models these as a small GObject hierarchy with one type per source
  # of input. The event types are kept as symbols here, with the same names and
  # the same meanings, and BS_CURSOR_DOUBLE_CLICK stays the one synthetic event
  # the editor sends when a button is double-clicked in the UI.
  class Event
    TYPES = %i[
      button_press
      button_release
      dial_rotate
      dial_press
      dial_release
      touchscreen_short_press
      touchscreen_long_press
      touchscreen_swipe
      cursor_double_click
    ].freeze

    def initialize(event_type, device)
      @event_type = event_type
      @device = device
    end

    attr_reader :event_type, :device
  end

  # ButtonEvent carries the button that was pressed or released.
  class ButtonEvent < Event
    def initialize(event_type, device, button)
      super(event_type, device)
      @button = button
    end

    attr_reader :button
  end

  # CursorEvent is the synthetic event the editor raises on a double click.
  class CursorEvent < Event
    def self.double_click(device) = new(:cursor_double_click, device)
  end

  # DialEvent carries the dial and, for a rotation, how far it turned.
  class DialEvent < Event
    def initialize(event_type, device, dial, rotation = 0)
      super(event_type, device)
      @dial = dial
      @rotation = rotation
    end

    attr_reader :dial, :rotation
  end

  # TouchscreenEvent carries the slot and the start and end points of a swipe.
  class TouchscreenEvent < Event
    def initialize(event_type, device, slot, start_point = nil, end_point = nil)
      super(event_type, device)
      @slot = slot
      @start_point = start_point
      @end_point = end_point
    end

    attr_reader :slot, :start_point, :end_point
  end
end
