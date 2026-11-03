# frozen_string_literal: true

require_relative "action_info"

module Boatswain
  # ActionFactory is the port of lib/bs-action-factory.c: a plugin's catalogue
  # of ActionInfo entries, plus the constructor that turns one of those entries
  # into a live Action.
  #
  # Upstream derives from PeasExtensionBase and implements GListModel, so the
  # action selector can bind_model over a factory and read the plugin's name
  # and icon out of its PeasPluginInfo. There is no libpeas here: the plugin's
  # module name, display name and icon — the three fields the UI reads off
  # PeasPluginInfo — are declared by the subclass, and Context keeps the
  # registry that libpeas' extension set provided.
  class ActionFactory
    include Enumerable

    def initialize(context)
      @context = context
      @action_infos = []
      add_action_entries(self.class.entries)
    end

    attr_reader :context, :action_infos

    # The fields upstream reads from PeasPluginInfo, declared per plugin in
    # its <name>.plugin.desktop.in.
    class << self
      def module_name = ""

      def display_name = ""

      def icon_name = ""

      def entries = []
    end

    def module_name = self.class.module_name

    def display_name = self.class.display_name

    def icon_name = self.class.icon_name

    def each(&block) = @action_infos.each(&block)

    def n_items = @action_infos.length

    # Port of bs_action_factory_add_action.
    def add_action(info)
      @action_infos << info
      info
    end

    # Port of bs_action_factory_add_action_entries.
    def add_action_entries(entries)
      entries.each { |entry| add_action(ActionInfo.new(**entry)) }
      @action_infos
    end

    # Port of bs_action_factory_get_info.
    def info(id) = @action_infos.find { |info| info.id == id }

    # Port of bs_action_factory_create_action: builds the action, then stamps
    # the id, name and factory onto it the way bs_action_factory_create_action
    # does before handing it back.
    def create_action(action_info)
      build_action(action_info).then do |action|
        if action
          action.id = action_info.id
          action.name = action_info.name
          action.factory = self
        end
        action
      end
    end

    private

    # The create_action vfunc. Subclasses map an ActionInfo id to a block
    # that builds the action; a factory asked for an id it does not publish
    # answers with nothing, as upstream's trailing `return NULL` does.
      def build_action(action_info) = action_builders[action_info.id]&.call

      def action_builders = {}
  end
end
