# frozen_string_literal: true

module Boatswain
  # ObservableList is the port's stand-in for GListStore plus its
  # "items-changed" signal.
  #
  # Upstream (lib/bs-device.c, lib/bs-button-grid.c, lib/bs-device-manager.c)
  # keeps its collections in GListStore and lets widgets bind_model over them.
  # A GListStore can only hold GObjects, and the models in this port are plain
  # Ruby objects, so the collections are kept here instead and widgets rebuild
  # their rows from the "items-changed" callbacks. The method names match the
  # GListStore ones the upstream call sites use.
  class ObservableList
    include Enumerable

    def initialize(items = [])
      @items = items.dup
      @callbacks = []
    end

    def each(&block) = @items.each(&block)

    def n_items = @items.length

    # Enumerable does not define length/size on its own; callers that treat
    # this like any other Ruby collection (region.buttons.length, same as
    # they would write for an Array) need it alongside n_items, the name the
    # GListModel call sites use.
    def length = @items.length
    alias_method :size, :length

    def [](position) = @items[position]

    def to_a = @items.dup

    def empty? = @items.empty?

    # Registers a callback run as (position, removed, added) after every
    # change, the same arguments GListModel's "items-changed" carries.
    def on_items_changed(&block)
      @callbacks << block
      block
    end

    def disconnect(callback) = @callbacks.delete(callback)

    def append(item)
      @items << item
      items_changed(@items.length - 1, 0, 1)
      item
    end

    def insert(position, item)
      @items.insert(position, item)
      items_changed(position, 0, 1)
      item
    end

    def remove(position)
      @items.delete_at(position).tap do
        items_changed(position, 1, 0)
      end
    end

    def remove_all
      @items.length.then do |removed|
        @items = []
        items_changed(0, removed, 0)
      end
    end

    def find_item(item) = @items.index { |candidate| candidate.equal?(item) }

    private

      def items_changed(position, removed, added)
        @callbacks.dup.each { |callback| callback.call(position, removed, added) }
      end
  end
end
