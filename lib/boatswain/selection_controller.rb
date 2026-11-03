# frozen_string_literal: true

module Boatswain
  # SelectionController is the port of src/bs-selection-controller.c: the one
  # object the device editor and every region widget share so that selecting a
  # button in one grid clears the selection in all the others.
  #
  # Upstream holds an owner and an item and emits "selection-changed"; the
  # owner is the region widget's model and the item is a BsButton, BsDial or
  # BsTouchscreenSlot.
  class SelectionController
    def initialize
      @selected = false
      @callbacks = []
    end

    attr_reader :owner, :item

    def on_selection_changed(&block)
      @callbacks << block
      block
    end

    def disconnect(callback) = @callbacks.delete(callback)

    # Port of bs_selection_controller_get_selection: answers the owner and the
    # item, or nothing at all while there is no selection.
    def selection
      if @selected
        [@owner, @item]
      end
    end

    def selected? = @selected

    # Port of bs_selection_controller_set_selection.
    def select(owner, item)
      unless @owner.equal?(owner) && @item.equal?(item)
        @selected = true
        @owner = owner
        @item = item
        selection_changed
      end
    end

    # Port of bs_selection_controller_unselect.
    def unselect
      if @selected
        @selected = false
        @owner = nil
        @item = nil
        selection_changed
      end
    end

    private

      def selection_changed
        @callbacks.dup.each { |callback| callback.call(self) }
      end
  end
end
