# frozen_string_literal: true

require "adwaita"

module Boatswain
  module Plugins
    module Launcher
      # LaunchPreferences is the port of
      # src/plugins/launcher/launcher-launch-preferences.c and its .ui: the
      # preferences group of the Launch Application action — one activatable
      # row that opens an app chooser and then shows the chosen app's name.
      class LaunchPreferences < Adwaita::PreferencesGroup
        def initialize(launch_action)
          super()
          @launch_action = launch_action
          build
          update_app_info
        end

        # Port of launcher_launch_preferences_update.
        def update_app_info
          @launch_action.app.then do |app|
            app_name.visible = !app.nil?
            next_image.visible = app.nil?
            app_name.label = app_display_name(app)
          end
        end

        private

          def build
            tap do |group|
              group.add(app_row)

              app_row.tap do |row|
                row.add_suffix(app_name)
                row.add_suffix(next_image)
                row.signal_connect("activated") { on_app_row_activated }
              end
            end
          end

          def app_display_name(app)
            if app
              app.display_name
            else
              "Unknown application"
            end
          end

          # Port of on_app_row_activated_cb and on_app_chooser_response.
          def on_app_row_activated
            app_chooser_dialog.tap do |dialog|
              dialog.signal_connect("response") do |chooser, response|
                if response == Gtk::ResponseType::OK.to_i
                  @launch_action.app = chooser.app_info
                  update_app_info
                end
                chooser.close
              end
              dialog.present
            end
          end

          def app_chooser_dialog
            Gtk::AppChooserDialog.new(root, :modal, Gio::File.new_for_path(Dir.home)).tap do |dialog|
              dialog.destroy_with_parent = true
            end
          end

          def app_row
            @app_row ||= Adwaita::ActionRow.new.tap do |row|
              row.title = "Choose Application"
              row.activatable = true
            end
          end

          def app_name
            @app_name ||= Gtk::Label.new.tap do |label|
              label.visible = false
              label.valign = :center
            end
          end

          def next_image
            @next_image ||= Gtk::Image.new(icon_name: "go-next-symbolic").tap do |image|
              image.valign = :center
            end
          end
      end
    end
  end
end
