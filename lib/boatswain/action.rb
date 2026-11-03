# frozen_string_literal: true

require_relative "icon"

module Boatswain
  # Action is the port of lib/bs-action.c: the abstract base every action in
  # every plugin derives from.
  #
  # Upstream is an abstract GObject with an "icon" property, a "changed" signal
  # and five vfuncs — get_icon, handle_event, get_preferences,
  # serialize_settings and deserialize_settings. The subclass hooks are plain
  # Ruby methods here with the same names and the same defaults: no
  # preferences, no settings, and an empty icon created in the constructor.
  class Action
    def initialize
      @icon = Icon.new_empty
      @callbacks = []
    end

    attr_reader :icon

    # Upstream's id, name and factory are write-once, set by the factory right
    # after it constructs the action.
    attr_reader :id, :name, :factory

    def id=(value)
      @id ||= value
    end

    def name=(value)
      @name ||= value
    end

    def factory=(value)
      @factory ||= value
    end

    # Port of bs_action_get_preferences: actions with no settings of their own
    # contribute no widget to the button editor.
    def preferences = nil

    # Port of bs_action_handle_event. Subclasses that respond to input
    # override it; the base action ignores every event.
    def handle_event(event) = event

    # Port of bs_action_serialize_settings. Subclasses return a Hash, which is
    # what upstream's "serialized settings must be JsonObjects" check enforces.
    def serialize_settings = nil

    # Port of bs_action_deserialize_settings.
    def deserialize_settings(settings) = settings

    # Port of the "changed" signal and bs_action_changed.
    def on_changed(&block)
      @callbacks << block
      block
    end

    def disconnect(callback) = @callbacks.delete(callback)

    def changed
      @callbacks.dup.each { |callback| callback.call(self) }
    end
  end
end
