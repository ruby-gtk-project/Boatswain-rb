# frozen_string_literal: true

require "securerandom"

require_relative "page"

module Boatswain
  # Profile is the port of lib/bs-profile.c: a named set of pages for one
  # device, with the brightness that device runs at while the profile is
  # active.
  #
  # Upstream's id is construct-only and generated with g_uuid_string_random;
  # the name and brightness are read-write properties the sidebar binds to, so
  # both notify their observers here.
  class Profile
    DEFAULT_BRIGHTNESS = 0.5

    # Port of bs_profile_new_empty.
    def self.new_empty(context, device)
      new(context, device, id: SecureRandom.uuid, name: "Unnamed profile")
    end

    # Port of bs_profile_new_from_json. A node that is not an object falls back
    # to an empty profile, as upstream warns and does.
    def self.from_json(context, device, data)
      if data.is_a?(Hash)
        new(
          context,
          device,
          id:         data["id"],
          name:       data["name"],
          brightness: data["brightness"] || DEFAULT_BRIGHTNESS,
          page_data:  data["page"],
        )
      else
        warn "JSON node is not an object"
        new_empty(context, device)
      end
    end

    def initialize(context, device, id:, name:, brightness: DEFAULT_BRIGHTNESS, page_data: nil)
      @id = id
      @name = name
      @brightness = brightness
      @device = device
      @root_page = Page.new_root(context, page_data)
      @callbacks = []
    end

    attr_reader :id, :name, :brightness, :device, :root_page

    def on_changed(&block)
      @callbacks << block
      block
    end

    def disconnect(callback) = @callbacks.delete(callback)

    def name=(value)
      value.to_s.strip.then do |stripped|
        unless stripped.empty? || stripped == @name
          @name = stripped
          changed
        end
      end
    end

    def brightness=(value)
      value.clamp(0.0, 1.0).then do |clamped|
        unless clamped == @brightness
          @brightness = clamped
          changed
        end
      end
    end

    # Port of bs_profile_to_json.
    def to_json_data
      {
        "id"         => @id,
        "name"       => @name,
        "brightness" => @brightness,
        "page"       => @root_page.to_json_data,
      }
    end

    private

      def changed
        @callbacks.dup.each { |callback| callback.call(self) }
      end
  end
end
