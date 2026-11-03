# frozen_string_literal: true

require "fileutils"
require "json"

require_relative "button_grid"
require_relative "observable_list"
require_relative "profile"

module Boatswain
  # Device is the port of lib/bs-device.c: a connected device, its profiles,
  # the stack of pages it is showing and the brightness it runs at.
  #
  # Subclasses supply the three strings the sidebar shows (name, serial number
  # and firmware version) and the region layout, exactly as upstream's
  # create_layout, get_name, get_serial_number and get_firmware_version
  # vfuncs do. Upstream also owns the HID upload path through BsRenderer and
  # BsDeviceUpdate; that is the hardware half of the device layer and is not
  # part of this port, so an icon change here reaches the editor's widgets
  # rather than a frame buffer.
  class Device
    SAVE_DELAY_SECONDS = 5

    def initialize(context)
      @context = context
      @profiles = ObservableList.new
      @active_pages = []
      @brightness = Profile::DEFAULT_BRIGHTNESS
      @loading_profile = false
      @callbacks = []
      @regions = ObservableList.new(create_layout)
      connect_regions
    end

    attr_reader :context, :profiles, :regions, :brightness, :active_profile

    # The subclass hooks. Each device model answers these.
    def name = ""

    def serial_number = ""

    def firmware_version = ""

    def create_layout = []

    # Notified when the active profile or the active page changes, standing in
    # for upstream's "notify::active-profile" and "notify::active-page".
    def on_changed(&block)
      @callbacks << block
      block
    end

    def disconnect(callback) = @callbacks.delete(callback)

    def brightness=(value)
      value.clamp(0.0, 1.0).then do |clamped|
        unless clamped == @brightness
          @brightness = clamped
          changed
        end
      end
    end

    # Port of bs_device_get_region.
    def region(region_id) = @regions.find { |candidate| candidate.id == region_id }

    # Port of bs_device_get_active_page.
    def active_page = @active_pages.first

    # Port of load_profiles: reads the device's profile file, or creates the
    # "Default" profile the first time the device is seen.
    def load
      read_profiles.then do |saved|
        Array(saved["profiles"]).each do |profile_data|
          @profiles.append(Profile.from_json(@context, self, profile_data))
        end
        load_profile(active_profile_from(saved["active-profile"]))
      end
    end

    # Port of bs_device_load_profile.
    def load_profile(profile)
      unless @active_profile.equal?(profile)
        unwind_pages
        @active_profile = profile
        @loading_profile = true
        self.brightness = profile.brightness
        push_page(profile.root_page)
        @loading_profile = false
        changed
      end
    end

    # Port of bs_device_push_page.
    def push_page(page)
      @active_pages.unshift(page)
      page.load_items
      load_active_page
      changed
    end

    # Port of bs_device_pop_page.
    def pop_page
      @active_pages.shift.then do |page|
        if page
          update_page_region_data(page)
          page.unload_items
        end
        load_active_page
        changed
      end
    end

    # Port of save_profiles plus schedule_save: upstream coalesces saves into
    # one five-second timeout.
    def schedule_save
      if @save_timeout_id.nil?
        @save_timeout_id = GLib::Timeout.add_seconds(SAVE_DELAY_SECONDS) do
          @save_timeout_id = nil
          save_profiles
          GLib::Source::REMOVE
        end
      end
    end

    def save_profiles
      update_page_region_data(active_page)
      @active_pages.each(&:update_items)
      @active_profile.brightness = @brightness

      FileUtils.mkdir_p(File.dirname(profile_path))
      File.write(profile_path, JSON.pretty_generate(profiles_json))
    end

    private

      def changed
        @callbacks.dup.each { |callback| callback.call(self) }
      end

      def profiles_json
        {
          "active-profile" => @active_profile&.id,
          "profiles"       => @profiles.map(&:to_json_data),
        }
      end

      # Port of get_profile_path.
      def profile_path
        File.join(
          ENV.fetch("XDG_DATA_HOME", File.expand_path("~/.local/share")),
          "boatswain",
          "#{serial_number}.json",
        )
      end

      def read_profiles
        if File.exist?(profile_path)
          JSON.parse(File.read(profile_path))
        else
          {}
        end
      rescue JSON::ParserError => e
        warn "Error loading profile for device #{serial_number}: #{e.message}"
        {}
      end

      def active_profile_from(profile_id)
        @profiles.find { |profile| profile.id == profile_id } ||
          @profiles[0] ||
          default_profile
      end

      def default_profile
        Profile.new_empty(@context, self).tap do |profile|
          profile.name = "Default"
          @profiles.append(profile)
        end
      end

      def unwind_pages
        unless @active_pages.empty?
          update_page_region_data(@active_pages.first)
        end
        @active_pages.each(&:unload_items)
        @active_pages = []
      end

      # Port of update_page_region_data.
      def update_page_region_data(page)
        if page
          @regions.each { |region| page.set_region_data(region.id, region.serialize) }
        end
      end

      # Port of load_active_page: hands every button of every button grid the
      # action and custom icon the active page holds for its slot.
      def load_active_page
        active_page.then do |page|
          if page
            @regions.each { |region| load_region(page, region) }
          end
        end
      end

      def load_region(page, region)
        page.region_data(region.id).then { |data| region.deserialize(data) }

        if region.is_a?(ButtonGrid)
          region.buttons.each_with_index do |button, position|
            page.realize(region.id, position).then do |custom_icon, action|
              button.action = action
              button.custom_icon = custom_icon
            end
          end
        end
      end

      def connect_regions
        @regions.each do |region|
          if region.is_a?(ButtonGrid)
            region.on_button_changed { |button| button_changed(region, button) }
          end
        end
      end

      # Port of on_button_grid_button_changed_cb.
      def button_changed(region, button)
        unless @loading_profile || active_page.nil?
          active_page.update_item(
            region.id,
            button.position,
            button.action,
            button.custom_icon,
          )
          schedule_save
        end
      end
  end
end
