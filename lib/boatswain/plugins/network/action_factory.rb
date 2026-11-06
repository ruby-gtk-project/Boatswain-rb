# frozen_string_literal: true

require "soup3"

require_relative "../../action_factory"
require_relative "http_action"

module Boatswain
  module Plugins
    module Network
      # ActionFactory is the port of
      # src/plugins/network/network-action-factory.c: the "Network" plugin's
      # one action, backed by a single shared Soup session.
      class ActionFactory < Boatswain::ActionFactory
        class << self
          def module_name = "network"

          def display_name = "Network"

          def icon_name = "network-transmit-symbolic"

          def entries
            [
              {
                id:        "network-http-action",
                icon_name: "network-transmit-symbolic",
                name:      "HTTP Request",
              },
            ]
          end
        end

        def initialize(context)
          super
          @session = Soup::Session.new
        end

        private

          # Port of network_action_factory_create_action.
          def action_builders
            {
              "network-http-action" => -> { HttpAction.new(@session) },
            }
          end
      end
    end
  end
end
