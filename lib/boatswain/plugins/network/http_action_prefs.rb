# frozen_string_literal: true

require "adwaita"

module Boatswain
  module Plugins
    module Network
      # HttpActionPrefs is the port of
      # src/plugins/network/network-http-action-prefs.c and its .ui: the URI
      # entry, method and payload-source combo rows, the file chooser row and
      # the text-payload edit window.
      class HttpActionPrefs < Gtk::Box
        def initialize(http_action)
          super(:vertical, 24)
          @http_action = http_action
          build
          update_payload_src_visibility
        end

        # Port of network_http_action_prefs_deserialize_settings.
        def deserialize_settings(settings)
          uri = settings["uri"].to_s
          uri_row.text = uri
          @http_action.uri = uri

          method = HttpAction::METHODS[settings["method"] || 0] || :get
          method_row.selected = HttpAction::METHODS.index(method)
          @http_action.method = method

          source = HttpAction::SOURCES[settings["source"] || 0] || :text
          payload_src_row.selected = HttpAction::SOURCES.index(source)
          @http_action.source = source

          if settings["payload_file"]
            set_file(Gio::File.new_for_path(settings["payload_file"]))
          end

          payload_text_buffer.text = settings["payload_text"].to_s
          @http_action.text = settings["payload_text"].to_s

          update_payload_src_visibility
        end

        private

          def build
            tap do |box|
              box.append(request_group)

              request_group.tap do |group|
                group.add(uri_row)
                uri_row.signal_connect("notify::text") { @http_action.uri = uri_row.text }

                group.add(method_row)
                method_row.signal_connect("notify::selected") { on_method_selected }
              end

              box.append(post_group)

              post_group.tap do |group|
                group.add(payload_src_row)
                payload_src_row.signal_connect("notify::selected") { on_payload_src_selected }

                group.add(file_row)
                file_row.signal_connect("activated") { on_file_row_activated }

                group.add(edit_row)
                edit_row.signal_connect("activated") { on_edit_row_activated }
              end
            end
          end

          def on_method_selected
            @http_action.method = HttpAction::METHODS[method_row.selected]
            update_payload_src_visibility
          end

          def on_payload_src_selected
            @http_action.source = HttpAction::SOURCES[payload_src_row.selected]
            file_row.visible = @http_action.source == :file
          end

          def on_file_row_activated
            file_dialog.open(root, nil) { |dialog, result| finish_open_file(dialog, result) }
          end

          def finish_open_file(dialog, result)
            set_file(dialog.open_finish(result))
          rescue Gtk::DialogError, Gio::Error => e
            warn "Error opening file: #{e.message}"
          end

          def set_file(file)
            basename = file.basename
            filename_label.label = basename
            file_row.tooltip_text = basename
            @http_action.file = file
          end

          def on_edit_row_activated
            if @http_action.source == :file
              launch_payload_file
            else
              text_edit_window.present
            end
          end

          def launch_payload_file
            @http_action.payload_file.then do |file|
              if file
                Gtk::FileLauncher.new(file).launch(root, nil) { |launcher, result| finish_launch(launcher, result) }
              end
            end
          end

          def finish_launch(launcher, result)
            launcher.launch_finish(result)
          rescue Gio::Error => e
            warn "Error launching app to edit file: #{e.message}"
          end

          def update_payload_src_visibility
            post_group.visible = method_row.selected == HttpAction::METHODS.index(:post)
            file_row.visible = payload_src_row.selected == HttpAction::SOURCES.index(:file)
          end

          def request_group = @request_group ||= Adwaita::PreferencesGroup.new

          def uri_row
            @uri_row ||= Adwaita::EntryRow.new.tap { |row| row.title = "URI" }
          end

          def method_row
            @method_row ||= Adwaita::ComboRow.new.tap do |row|
              row.title = "Method"
              row.model = Gtk::StringList.new(%w[GET POST])
            end
          end

          def post_group = @post_group ||= Adwaita::PreferencesGroup.new

          def payload_src_row
            @payload_src_row ||= Adwaita::ComboRow.new.tap do |row|
              row.title = "Payload source"
              row.model = Gtk::StringList.new(%w[Text File])
            end
          end

          def file_row
            @file_row ||= Adwaita::ActionRow.new.tap do |row|
              row.title = "File"
              row.activatable = true
              row.add_suffix(filename_label)
              row.add_suffix(Gtk::Image.new(icon_name: "document-open-symbolic").tap { |image| image.valign = :center })
            end
          end

          def filename_label
            @filename_label ||= Gtk::Label.new.tap do |label|
              label.ellipsize = :middle
              label.max_width_chars = 35
            end
          end

          def edit_row
            @edit_row ||= Adwaita::ActionRow.new.tap do |row|
              row.title = "Edit payload"
              row.activatable = true
              row.add_suffix(Gtk::Image.new(icon_name: "text-editor-symbolic"))
            end
          end

          def file_dialog
            Gtk::FileDialog.new.tap do |dialog|
              dialog.modal = true
              dialog.title = "Select payload file"
              dialog.accept_label = "Open"
              dialog.filters = file_filters
            end
          end

          def file_filters
            Gio::ListStore.new(Gtk::FileFilter).tap do |filters|
              filters.append(text_file_filter)
              filters.append(json_file_filter)
            end
          end

          def text_file_filter
            Gtk::FileFilter.new.tap do |filter|
              filter.name = "Text Files"
              filter.add_mime_type("text/plain")
            end
          end

          def json_file_filter
            Gtk::FileFilter.new.tap do |filter|
              filter.name = "JSON Files"
              filter.add_mime_type("application/json")
            end
          end

          def payload_text_buffer
            @payload_text_buffer ||= Gtk::TextBuffer.new.tap do |buffer|
              buffer.signal_connect("notify::text") { @http_action.text = buffer.text }
            end
          end

          def text_edit_window
            @text_edit_window ||= Adwaita::Window.new.tap do |window|
              window.title = "HTTP Request Payload"
              window.hide_on_close = true
              window.default_width = 500
              window.default_height = 500
              window.content = text_edit_toolbar_view

              text_edit_toolbar_view.tap do |view|
                view.add_top_bar(Adwaita::HeaderBar.new)
                view.content = text_edit_scrolled_window
              end
            end
          end

          def text_edit_toolbar_view = @text_edit_toolbar_view ||= Adwaita::ToolbarView.new

          def text_edit_scrolled_window
            @text_edit_scrolled_window ||= Gtk::ScrolledWindow.new.tap do |scrolled|
              scrolled.child = text_view
            end
          end

          def text_view
            @text_view ||= Gtk::TextView.new.tap do |view|
              view.bottom_margin = 12
              view.left_margin = 12
              view.right_margin = 12
              view.top_margin = 12
              view.buffer = payload_text_buffer
            end
          end
      end
    end
  end
end
