# frozen_string_literal: true

require "adwaita"

require_relative "../../action"

module Boatswain
  module Plugins
    module Default
      # SwitchProfileAction is the port of
      # src/plugins/default/default-switch-profile-action.c: loads a named
      # profile of a named device, and shows that profile's name on the key.
      #
      # Upstream's preferences are two AdwComboRows bound straight to the
      # device and profile GListModels, with a property expression reading
      # "name" off each item. The models here are plain Ruby lists, so each row
      # gets a GtkStringList of names and maps the selected index back, the
      # same way the pilot port's combo rows do.
      class SwitchProfileAction < Boatswain::Action
        MISSING_OPACITY = 0.35

        def initialize(context)
          super()
          @context = context
          @devices_changed = context.devices.on_items_changed { |_p, _r, _a| update_active_profile }
        end

        attr_reader :serial_number, :profile_id

        # Port of default_switch_profile_action_handle_event.
        def handle_event(event)
          if event.event_type == :button_press
            find_device_and_profile.then do |device, profile|
              if device && profile
                device.load_profile(profile)
              end
            end
          end
        end

        # Port of default_switch_profile_action_get_preferences.
        def preferences
          Adwaita::PreferencesGroup.new.tap do |group|
            group.add(devices_row)
            group.add(profiles_row)
          end
        end

        # Port of default_switch_profile_action_serialize_settings.
        def serialize_settings
          {}.tap do |settings|
            if @serial_number
              settings["serial-number"] = @serial_number
            end
            if @profile_id
              settings["profile-id"] = @profile_id
            end
          end
        end

        # Port of default_switch_profile_action_deserialize_settings.
        def deserialize_settings(settings)
          @serial_number = settings["serial-number"]
          @profile_id = settings["profile-id"]
          update_active_profile
        end

        private

          # Port of find_device.
          def find_device
            @context.devices.find { |device| device.serial_number == @serial_number }
          end

          # Port of get_profile_from_device: an unknown profile id falls back
          # to the device's active profile.
          def profile_from(device)
            device.profiles.find { |profile| profile.id == @profile_id } ||
              device.active_profile
          end

          # Port of find_device_and_profile.
          def find_device_and_profile
            find_device.then do |device|
              if device
                [device, profile_from(device)]
              else
                [nil, nil]
              end
            end
          end

          # Port of update_active_profile: a key pointing at a device that is
          # not plugged in draws dimmed.
          def update_active_profile
            find_device_and_profile.then do |device, profile|
              if device && profile
                icon.opacity = Icon::INHERIT_OPACITY
                set_active_profile(profile)
              else
                icon.opacity = MISSING_OPACITY
              end
            end
          end

          # Port of set_active_profile: the key's caption follows the
          # profile's name.
          def set_active_profile(profile)
            if @profile_callback && @active_profile
              @active_profile.disconnect(@profile_callback)
            end

            @active_profile = profile
            @profile_callback = profile.on_changed { |changed| icon.text = changed.name }
            icon.text = profile.name
            changed
          end

          def devices_row
            @devices_row ||= Adwaita::ComboRow.new.tap do |row|
              row.title = "Stream Deck"
              row.model = Gtk::StringList.new(@context.devices.map(&:name))
              row.selected = device_position
              row.signal_connect("notify::selected") { on_device_selected(row.selected) }
            end
          end

          def profiles_row
            @profiles_row ||= Adwaita::ComboRow.new.tap do |row|
              row.title = "Profile"
              reload_profiles(row)
              row.signal_connect("notify::selected") { on_profile_selected(row.selected) }
            end
          end

          def device_position
            @context.devices.to_a.index { |device| device.serial_number == @serial_number } || 0
          end

          # Port of on_devices_combo_row_selected_item_changed_cb.
          def on_device_selected(position)
            @context.devices[position].then do |device|
              @serial_number = device&.serial_number
              reload_profiles(profiles_row)
              update_active_profile
            end
          end

          # Port of on_combo_row_selected_item_changed_cb.
          def on_profile_selected(position)
            selected_device_profiles[position].then do |profile|
              @profile_id = profile&.id
              if profile
                set_active_profile(profile)
              end
            end
          end

          def selected_device_profiles
            find_device.then do |device|
              if device
                device.profiles.to_a
              else
                []
              end
            end
          end

          def reload_profiles(row)
            selected_device_profiles.then do |profiles|
              row.model = Gtk::StringList.new(profiles.map(&:name))
              row.selected = profiles.index { |profile| profile.id == @profile_id } || 0
            end
          end
      end
    end
  end
end
