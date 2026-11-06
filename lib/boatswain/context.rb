# frozen_string_literal: true

require_relative "device_manager"
require_relative "plugins/default/action_factory"
require_relative "plugins/launcher/action_factory"
require_relative "plugins/network/action_factory"

module Boatswain
  # Context is the port of lib/bs-context.c: the one object that owns the
  # device list and the action factories every part of the UI reads from.
  #
  # Upstream loads the factories as libpeas extensions, including a GJS loader
  # for the gaming plugin. There is no libpeas here, so FACTORIES is the plain
  # registry the action selector and the multi-action editor enumerate, in the
  # order the plugins are listed.
  class Context
    FACTORIES = [
      Plugins::Default::ActionFactory,
      Plugins::Launcher::ActionFactory,
      Plugins::Network::ActionFactory,
    ].freeze

    class << self
      # Port of bs_context_get_default and bs_context_init_default: the context
      # is built once and then loads the devices.
      def default
        @default ||= new.tap(&:load)
      end

      # Port of bs_context_shutdown.
      def shutdown
        @default = nil
      end
    end

    def initialize
      @action_factories = FACTORIES.map { |factory| factory.new(self) }
      @device_manager = DeviceManager.new(self)
    end

    # Port of bs_context_get_available_action_factories.
    attr_reader :action_factories

    # Port of bs_context_get_devices.
    def devices = @device_manager.devices

    # The second half of bs_context_init_default: the device manager loads
    # once the factories exist, because a saved page names the factory that
    # built each of its actions.
    def load = @device_manager.load

    # The lookup bs-page-item.c's get_action_factory and the multi-action's
    # get_action_factory both do, by the plugin's module name.
    def action_factory(module_name)
      @action_factories.find { |factory| factory.module_name == module_name }
    end
  end
end
