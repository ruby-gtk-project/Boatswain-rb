# frozen_string_literal: true

require_relative "action"

module Boatswain
  # EmptyAction is the port of lib/bs-empty-action.c: the action a button with
  # nothing assigned to it carries, so that every button always has one.
  #
  # Upstream adds no behaviour at all over BsAction, and neither does this.
  class EmptyAction < Action
  end
end
