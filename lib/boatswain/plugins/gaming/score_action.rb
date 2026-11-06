# frozen_string_literal: true

require "adwaita"

require_relative "../../action"

module Boatswain
  module Plugins
    module Gaming
      # ScoreAction is the port of src/plugins/gaming/score.js: a counter
      # that increments on a short press, decrements on a long press and
      # resets on a very long press, optionally persisted to a text file.
      #
      # Upstream draws the score itself through a custom GdkPaintable
      # (GamingScorePaintable) so the number can be given its own colour
      # independent of the icon's. The Ruby bindings cannot implement
      # GdkPaintable (see Icon's own comment), and Icon already carries a
      # caption and a colour that IconWidget draws exactly the same way, so
      # the score is shown through Icon#text and Icon#color instead of a
      # second paintable.
      class ScoreAction < Boatswain::Action
        LONG_PRESS_MS = 500

        def initialize
          super
          @score = 0
          @restore_score = false
          @save_to_file = false
          @file = nil
          @state = :disabled
          @timeout_id = nil
          icon.icon_name = "score-symbolic"
          update_icon_text
        end

        attr_reader :score, :restore_score, :save_to_file, :file

        def score=(value)
          @score = value
          update_icon_text
          save_to_file_if_enabled
          changed
        end

        def restore_score=(value)
          @restore_score = value
          changed
        end

        def save_to_file=(value)
          @save_to_file = value
          save_to_file_if_enabled
          changed
        end

        def file=(value)
          @file = value
          save_to_file_if_enabled
          changed
        end

        # Port of vfunc_handle_event.
        def handle_event(event)
          case event.event_type
          when :button_press
            on_button_press
          when :button_release
            on_button_release
          when :touchscreen_short_press
            apply_action(:increment)
          when :touchscreen_long_press
            apply_action(:decrement)
          when :dial_rotate
            apply_rotation(event.rotation)
          when :dial_press, :dial_release, :touchscreen_swipe, :cursor_double_click
            nil
          end
        end

        # Port of vfunc_get_preferences.
        def preferences
          Gtk::Box.new(:vertical, 18).tap do |box|
            box.append(main_group)
            main_group.tap do |group|
              group.add(restore_row)
              group.add(color_row)
            end

            box.append(file_group)
            file_group.tap do |group|
              group.add(save_to_file_row)
              group.add(file_row)
            end
          end
        end

        # Port of vfunc_serialize_settings.
        def serialize_settings
          {
            "restore-score" => @restore_score,
            "score"         => @score,
            "text-color"    => icon.color&.to_s || "#FFF",
            "save-to-file"  => @save_to_file,
            "file"          => @file&.path,
          }
        end

        # Port of vfunc_deserialize_settings.
        def deserialize_settings(settings)
          self.restore_score = settings.fetch("restore-score", false)
          if @restore_score
            self.score = settings.fetch("score", 0)
          end

          icon.color = Gdk::RGBA.parse(settings.fetch("text-color", "#FFF"))

          self.save_to_file = settings.fetch("save-to-file", false)
          if settings["file"]
            self.file = Gio::File.new_for_path(settings["file"])
          end
        end

        private

          def update_icon_text = icon.text = @score.to_s

          def save_to_file_if_enabled
            if @save_to_file && @file
              begin
                File.write(@file.path, @score.to_s)
              rescue SystemCallError => e
                warn e.message
              end
            end
          end

          def apply_action(action)
            case action
            when :increment then self.score = @score + 1
            when :decrement then self.score = @score - 1
            when :reset then self.score = 0
            end
          end

          def apply_rotation(rotation)
            if rotation > 0
              rotation.times { apply_action(:increment) }
            else
              (-rotation).times { apply_action(:decrement) }
            end
          end

          def on_button_press
            @state = :short
            queue_state_change(:long, LONG_PRESS_MS) do
              apply_action(:decrement)
              queue_state_change(:very_long, LONG_PRESS_MS * 5) do
                apply_action(:reset)
                @state = :disabled
              end
            end
          end

          def on_button_release
            if @state == :short
              apply_action(:increment)
            end
            @state = :disabled
            cancel_timeout
          end

          def queue_state_change(next_state, timeout_ms, &block)
            cancel_timeout
            @timeout_id = GLib::Timeout.add(timeout_ms) do
              @timeout_id = nil
              @state = next_state
              block.call
              GLib::Source::REMOVE
            end
          end

          def cancel_timeout
            if @timeout_id
              GLib::Source.remove(@timeout_id)
              @timeout_id = nil
            end
          end

          def main_group = @main_group ||= Adwaita::PreferencesGroup.new

          def restore_row
            @restore_row ||= Adwaita::SwitchRow.new.tap do |row|
              row.title = "Restore Score"
              row.active = @restore_score
              row.signal_connect("notify::active") { self.restore_score = row.active? }
            end
          end

          def color_row
            @color_row ||= Adwaita::ActionRow.new.tap do |row|
              row.title = "Text Color"
              row.activatable_widget = color_button
              row.add_suffix(color_button)
            end
          end

          def color_button
            @color_button ||= Gtk::ColorDialogButton.new.tap do |button|
              button.valign = :center
              button.dialog = color_dialog
              button.rgba = icon.color || Gdk::RGBA.parse("#FFF")
              button.signal_connect("notify::rgba") { icon.color = button.rgba }
            end
          end

          def color_dialog
            @color_dialog ||= Gtk::ColorDialog.new.tap do |dialog|
              dialog.modal = true
              dialog.with_alpha = false
            end
          end

          def file_group = @file_group ||= Adwaita::PreferencesGroup.new

          def save_to_file_row
            @save_to_file_row ||= Adwaita::SwitchRow.new.tap do |row|
              row.title = "Save to File"
              row.subtitle = "Save score to a text file. Other apps such as OBS Studio may read from it."
              row.active = @save_to_file
              row.signal_connect("notify::active") { self.save_to_file = row.active? }
            end
          end

          def file_row
            @file_row ||= Adwaita::ActionRow.new.tap do |row|
              row.title = "File"
              row.activatable = true
              update_file_row(row)
              row.signal_connect("activated") { on_file_row_activated }
            end
          end

          def update_file_row(row)
            row.subtitle = @file&.basename || ""
            row.tooltip_text = @file&.path || ""
          end

          def on_file_row_activated
            file_dialog.save(file_row.root, nil) { |dialog, result| finish_save(dialog, result) }
          end

          def finish_save(dialog, result)
            self.file = dialog.save_finish(result)
            update_file_row(file_row)
            save_to_file_if_enabled
          rescue Gtk::DialogError, Gio::Error => e
            warn "Error saving file: #{e.message}"
          end

          def file_dialog
            Gtk::FileDialog.new.tap do |dialog|
              dialog.modal = true
              dialog.initial_file = @file || default_file
              dialog.filters = file_filters
            end
          end

          def default_file
            path = GLib.build_filenamev(
              [
                GLib.get_user_special_dir(:documents),
                "score.txt",
              ],
            )
            Gio::File.new_for_path(path)
          end

          def file_filters
            Gio::ListStore.new(Gtk::FileFilter).tap do |filters|
              filters.append(score_file_filter)
            end
          end

          def score_file_filter
            Gtk::FileFilter.new.tap do |filter|
              filter.name = "Text File (.txt)"
              filter.add_suffix("txt")
            end
          end
      end
    end
  end
end
