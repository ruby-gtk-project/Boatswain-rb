# frozen_string_literal: true

require_relative "device_region"
require_relative "dial"

module Boatswain
  # DialGrid is the port of lib/bs-dial-grid.c/.h: the region holding a
  # device's dials, laid out in a fixed number of columns.
  #
  # Upstream builds every dial up front in bs_dial_grid_new, exactly as
  # ButtonGrid builds its buttons; dials carry no action of their own
  # (BsDial is not a BsActionable), so there is no change relay to wire here
  # the way ButtonGrid wires "action-changed"/"icon-changed" into
  # "button-changed".
  class DialGrid < DeviceRegion
    def initialize(id:, device:, n_dials:, grid_columns:, **geometry)
      super(id: id, device: device, **geometry)
      @grid_columns = grid_columns
      @dials = Array.new(n_dials) { |position| Dial.new(device: device, position: position) }
    end

    attr_reader :dials, :grid_columns
  end
end
