# frozen_string_literal: true

require_relative "empty_action"
require_relative "icon"

module Boatswain
  # PageItem is the port of lib/bs-page-item.c: one saved slot of a page —
  # which factory and action fill it, that action's settings, and any custom
  # icon the user put on it.
  #
  # Upstream keeps the settings and the custom icon as raw JsonNodes and only
  # turns them into objects in bs_page_item_realize; the same split is kept
  # here, with Ruby Hashes where upstream has JsonObjects.
  class PageItem
    EMPTY = :empty
    ACTION = :action

    # Port of bs_page_item_new_from_json. Upstream's enum nick for
    # BS_PAGE_ITEM_ACTION is "action"; anything else is an empty slot.
    def self.from_json(data)
      new.tap do |item|
        item.item_type = type_from_json(data["type"])
        item.factory = data["factory"]
        item.action = data["action"]
        item.settings = data["settings"]
        item.custom_icon = data["custom-icon"]
      end
    end

    def self.type_from_json(value)
      if value.to_s == "action"
        ACTION
      else
        EMPTY
      end
    end

    def initialize
      @item_type = EMPTY
    end

    attr_accessor :item_type, :factory, :action, :settings, :custom_icon

    # Port of bs_page_item_to_json: an empty slot saves nothing but its custom
    # icon, an action slot saves its type, factory, action, icon and settings.
    def to_json_data
      {}.tap do |data|
        if @item_type == ACTION
          data["type"] = "action"
          data["factory"] = @factory
          data["action"] = @action
        end
        if @custom_icon
          data["custom-icon"] = @custom_icon
        end
        if @item_type == ACTION && @settings
          data["settings"] = @settings
        end
      end
    end

    # Port of bs_page_item_realize: builds the action and the custom icon this
    # slot describes. Answers [custom_icon, action]; a slot naming a factory
    # that is not loaded raises, as upstream reports through its GError.
    def realize(context)
      [realize_custom_icon, realize_action(context)]
    end

    # Port of bs_page_item_update: writes a live action and icon back into the
    # saved slot.
    def update(action, custom_icon)
      if action && !action.is_a?(EmptyAction)
        @item_type = ACTION
        @factory = action.factory&.module_name
        @action = action.id
        @settings = action.serialize_settings
      else
        @item_type = EMPTY
        @factory = nil
        @action = nil
        @settings = nil
      end

      @custom_icon = custom_icon&.to_json_data
    end

    # Raised when a saved slot names an action factory this build does not
    # have, matching the G_IO_ERROR_FAILED upstream sets.
    class UnknownFactoryError < StandardError
    end

    private

      def realize_custom_icon
        if @custom_icon
          Icon.from_json(@custom_icon)
        end
      end

      def realize_action(context)
        if @item_type == ACTION
          action_from_factory(context)
        else
          EmptyAction.new
        end
      end

      def action_from_factory(context)
        context.action_factory(@factory).then do |factory|
          unless factory
            raise UnknownFactoryError, "No action factory named \"#{@factory}\""
          end

          factory.info(@action).then do |info|
            unless info
              raise UnknownFactoryError, "Factory \"#{@factory}\" has no action \"#{@action}\""
            end

            factory.create_action(info).tap do |action|
              if @settings
                action.deserialize_settings(@settings)
              end
            end
          end
        end
      end
  end
end
