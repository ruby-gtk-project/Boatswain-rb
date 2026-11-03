# frozen_string_literal: true

require "adwaita"

require_relative "../../action"

module Boatswain
  module Plugins
    module Launcher
      # OpenFileAction is the port of
      # src/plugins/launcher/launcher-open-file-action.c: opens a chosen file
      # with its default handler, and puts the file's name on the key.
      class OpenFileAction < Boatswain::Action
        def initialize
          super
          icon.icon_name = "folder-documents-symbolic"
        end

        attr_reader :file

        # Port of set_file.
        def file=(value)
          @file = value
          icon.text = value&.basename
          changed
          update_file_row
        end

        # Port of launcher_open_file_action_handle_event.
        def handle_event(event)
          if event.event_type == :button_press && @file
            launch
          end
        end

        # Port of launcher_open_file_action_get_preferences.
        def preferences
          Adwaita::PreferencesGroup.new.tap do |group|
            group.add(file_row)
            update_file_row
          end
        end

        # Port of launcher_open_file_action_serialize_settings.
        def serialize_settings = { "file" => @file&.uri }

        # Port of launcher_open_file_action_deserialize_settings.
        def deserialize_settings(settings)
          settings["file"].then do |uri|
            if uri
              self.file = Gio::File.new_for_uri(uri)
            end
          end
        end

        private

          def launch
            Gtk::FileLauncher.new(@file).launch(active_window, nil) do |launcher, result|
              finish_launch(launcher, result)
            end
          end

          def finish_launch(launcher, result)
            launcher.launch_finish(result)
          rescue Gio::Error => e
            warn "Error opening file: #{e.message}"
          end

          def active_window = Gio::Application.default&.active_window

          # Port of update_file_row: the clear button only shows once a file is
          # chosen, and the folder icon only while none is.
          def update_file_row
            @file_row.then do |row|
              if row
                clear_button.visible = !@file.nil?
                folder_icon.visible = @file.nil?
                row.subtitle = row_subtitle
                apply_property_class(row)
              end
            end
          end

          def row_subtitle
            if @file
              @file.basename
            else
              "No file selected"
            end
          end

          def apply_property_class(row)
            if @file
              row.add_css_class("property")
            else
              row.remove_css_class("property")
            end
          end

          def file_row
            @file_row ||= Adwaita::ActionRow.new.tap do |row|
              row.title = "File"
              row.activatable = true
              row.add_suffix(folder_icon)
              row.add_suffix(clear_button)
              row.signal_connect("activated") { on_file_row_activated }
            end
          end

          def folder_icon
            @folder_icon ||= Gtk::Image.new(icon_name: "document-open-symbolic").tap do |image|
              image.valign = :center
            end
          end

          def clear_button
            @clear_button ||= Gtk::Button.new.tap do |button|
              button.icon_name = "edit-clear-symbolic"
              button.valign = :center
              button.add_css_class("flat")
              button.signal_connect("clicked") { self.file = nil }
            end
          end

          # Port of on_file_row_activated_cb and on_file_dialog_opened_cb.
          def on_file_row_activated
            Gtk::FileDialog.new.tap do |dialog|
              dialog.title = "Select File"
              dialog.modal = true
              dialog.open(active_window, nil) { |source, result| finish_open(source, result) }
            end
          end

          def finish_open(dialog, result)
            self.file = dialog.open_finish(result)
          rescue Gtk::DialogError, Gio::Error => e
            warn "Error selecting file: #{e.message}"
          end
      end
    end
  end
end
