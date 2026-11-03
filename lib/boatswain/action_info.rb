# frozen_string_literal: true

module Boatswain
  # ActionInfo is the port of lib/bs-action-info.c: the catalogue entry an
  # action factory publishes for one kind of action, which is what the action
  # selector lists.
  #
  # All five properties are construct-only upstream, so they are read-only
  # here.
  class ActionInfo
    def initialize(id:, name:, description: nil, icon_name: nil, hidden: false)
      @id = id
      @name = name
      @description = description
      @icon_name = icon_name
      @hidden = hidden
    end

    attr_reader :id, :name, :description, :icon_name

    def hidden? = @hidden
  end
end
