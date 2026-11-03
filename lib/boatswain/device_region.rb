# frozen_string_literal: true

module Boatswain
  # DeviceRegion is the port of lib/bs-device-region.c: one addressable area of
  # a device — a button grid, a dial grid or a touchscreen — and where it sits
  # in the device's layout grid.
  #
  # Every geometry property is construct-only upstream, so they are read-only
  # here. serialize/deserialize are the vfuncs a region overrides to keep its
  # own data in the page; the base class round-trips an empty object, as
  # bs_device_region_real_serialize does.
  class DeviceRegion
    def initialize(id:, device:, column: 0, row: 0, column_span: 1, row_span: 1)
      @id = id
      @device = device
      @column = column
      @row = row
      @column_span = column_span
      @row_span = row_span
    end

    attr_reader :id, :device, :column, :row, :column_span, :row_span

    def serialize = {}

    def deserialize(data) = data
  end
end
