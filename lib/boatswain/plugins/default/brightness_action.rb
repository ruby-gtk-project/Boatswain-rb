# frozen_string_literal: true

require "adwaita"

require_relative "../../action"

module Boatswain
  module Plugins
    module Default
      # BrightnessAction is the port of
      # src/plugins/default/default-brightness-action.c: sets, raises or
      # lowers the device's brightness, with an icon per mode.
      class BrightnessAction < Boatswain::Action
        MODES = %i[set_value increase decrease].freeze

        MODE_ICONS = {
          set_value: "display-brightness-symbolic",
          increase:  "daytime-sunrise-symbolic",
          decrease:  "daytime-sunset-symbolic",
        }.freeze

        # Translators: "Constant" as in "Constant value of brightness"
        MODE_LABELS = ["Constant", "Increase", "Decrease"].freeze

        DEFAULT_VALUE = 0.01

        def initialize
          super
          @value = DEFAULT_VALUE
          @mode = nil
          self.mode = :set_value
        end

        attr_reader :mode, :value

        # Port of set_brightness_mode.
        def mode=(value)
          unless @mode == value
            @mode = value
            icon.icon_name = MODE_ICONS.fetch(value)
            changed
          end
        end

        def value=(new_value)
          @value = new_value
          changed
        end

        # Port of default_brightness_action_handle_event.
        def handle_event(event)
          if event.event_type == :button_press
            apply(event.device)
          end
        end

        # Port of default_brightness_action_get_preferences.
        def preferences
          Gtk::ListBox.new.tap do |listbox|
            listbox.selection_mode = :none
            listbox.add_css_class("boxed-list")
            listbox.append(mode_row)
            listbox.append(value_row)
          end
        end

        # Port of default_brightness_action_serialize_settings.
        def serialize_settings
          { "mode" => MODES.index(@mode), "value" => @value }
        end

        # Port of default_brightness_action_deserialize_settings.
        def deserialize_settings(settings)
          self.value = settings.fetch("value", DEFAULT_VALUE)
          self.mode = MODES[settings.fetch("mode", 0).to_i] || :set_value
        end

        private

          def apply(device)
            case @mode
            when :set_value then device.brightness = @value
            when :increase then device.brightness = (device.brightness + @value).clamp(0.0, 1.0)
            when :decrease then device.brightness = (device.brightness - @value).clamp(0.0, 1.0)
            end
          end

          def mode_row
            Adwaita::ComboRow.new.tap do |row|
              row.title = "Mode"
              row.model = Gtk::StringList.new(MODE_LABELS)
              row.selected = MODES.index(@mode)
              row.signal_connect("notify::selected") do
                self.mode = MODES[row.selected] || :set_value
              end
            end
          end

          def value_row
            Adwaita::ActionRow.new.tap do |row|
              row.title = "Value"
              row.add_suffix(value_spin_button)
            end
          end

          def value_spin_button
            Gtk::SpinButton.new(DEFAULT_VALUE, 1.0, DEFAULT_VALUE).tap do |spin|
              spin.valign = :center
              spin.value = @value
              spin.signal_connect("notify::value") { self.value = spin.value }
            end
          end
      end
    end
  end
end
