# frozen_string_literal: true

require "soup3"

require_relative "../../action"
require_relative "http_action_prefs"

module Boatswain
  module Plugins
    module Network
      # HttpAction is the port of src/plugins/network/network-http-action.c:
      # fires a GET or POST request at a URI on button press, with the POST
      # payload coming from either the text buffer or a chosen file.
      class HttpAction < Boatswain::Action
        METHODS = %i[get post].freeze
        SOURCES = %i[text file].freeze

        def initialize(session)
          super()
          @session = session
          @uri = ""
          @method = :get
          @source = :text
          @text = ""
          @file = nil
          icon.icon_name = "network-transmit-symbolic"
        end

        attr_reader :uri, :method, :source, :text, :file

        # Port of network_http_action_set_uri.
        def uri=(value)
          stripped = value.to_s.strip
          if @uri != stripped
            @uri = stripped
            changed
          end
        end

        # Port of network_http_action_set_method.
        def method=(value)
          if @method != value
            @method = value
            changed
          end
        end

        # Port of network_http_action_set_payload_src.
        def source=(value)
          @source = value
          changed
        end

        # Port of network_http_action_set_payload_text.
        def text=(value)
          if @text != value
            @text = value.to_s
            changed
          end
        end

        # Port of network_http_action_set_file.
        def file=(value)
          if @file != value
            @file = value
            changed
          end
        end

        # Port of network_http_action_get_file: only meaningful for a POST
        # whose payload comes from a file.
        def payload_file
          if @method == :post && @source == :file
            @file
          end
        end

        # Port of network_http_action_handle_event.
        def handle_event(event)
          if event.event_type == :button_press && !@uri.empty?
            send_request
          end
        end

        # Port of network_http_action_get_preferences.
        def preferences = @preferences ||= HttpActionPrefs.new(self)

        # Port of network_http_action_serialize_settings.
        def serialize_settings
          {
            "uri"          => @uri,
            "method"       => METHODS.index(@method),
            "source"       => SOURCES.index(@source),
            "payload_file" => @file&.path,
            "payload_text" => @text,
          }
        end

        # Port of network_http_action_deserialize_settings: upstream
        # forwards to the prefs widget, which sets both the fields and the
        # underlying action; this port keeps the same split.
        def deserialize_settings(settings)
          preferences.deserialize_settings(settings)
        end

        private

          def send_request
            Soup::Message.new(request_method, @uri).then do |message|
              if message
                message.request_headers.append("Connection", "close")
                if @method == :post
                  attach_payload(message)
                end
                @session.send_async(message, GLib::PRIORITY_DEFAULT, nil) do |source, result|
                  finish_request(source, result)
                end
              else
                warn "Error initializing message"
              end
            end
          end

          def request_method = @method == :post ? "POST" : "GET"

          def attach_payload(message)
            payload = read_payload
            if payload && !payload.empty?
              message.set_request_body_from_bytes("application/json", GLib::Bytes.new(payload))
            else
              message.request_headers.append("Content-Length", "0")
            end
          end

          def read_payload
            if @source == :file
              read_payload_file
            else
              @text
            end
          end

          def read_payload_file
            @file.then do |file|
              if file
                _success, bytes, = file.load_contents
                bytes.pack("C*")
              end
            end
          rescue Gio::Error => e
            warn "Error reading file contents: #{e.message}"
            nil
          end

          def finish_request(source, result)
            source.send_finish(result)
          rescue Gio::Error, GLib::Error => e
            warn "The request failed: #{e.message}"
          end
      end
    end
  end
end
