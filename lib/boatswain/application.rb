# frozen_string_literal: true

require "gtk4"
require "adwaita"

require_relative "context"
require_relative "settings"
require_relative "shortcuts_dialog"
require_relative "style"
require_relative "window"

module Boatswain
  # Application is the port of src/bs-application.c and src/main.c: the
  # application object, its actions and accelerators, the dark colour scheme it
  # asks for at startup and the stylesheet it loads.
  #
  # Adwaita::Application is broken in the Ruby bindings, so the application
  # object comes from Gtk and only the window is Adwaita.
  #
  # Upstream also holds itself in the background through the XDG background
  # portal when its window is closed, and installs UNIX signal handlers and a
  # --debug option. The portal is libportal, which this port does not bind, so
  # closing the window quits; the --debug option is the GLib debug logging
  # switch and has no counterpart here.
  class Application
    APPLICATION_ID = "com.feaneron.Boatswain"

    def build
      app.tap do
        app.signal_connect("startup") do
          Adwaita::StyleManager.default.color_scheme = :prefer_dark
          Style.load
        end

        app.signal_connect("activate") do
          install_actions
          app.add_window(window)
          window.present
        end
      end
    end

    def run = app.run([])

    # The application-wide settings, the port of the one GSettings key in
    # data/com.feaneron.Boatswain.gschema.xml.
    def settings = @settings ||= Settings.new

    def context = @context ||= Context.default

    def app = @app ||= Gtk::Application.new(APPLICATION_ID, :default_flags)

    def window = @window ||= Window.new(app, context)

    private

      def install_actions
        app.add_action(quit_action)
        app.set_accels_for_action("app.quit", ["<primary>q"])

        app.add_action(shortcuts_action)
        app.set_accels_for_action("app.shortcuts", ["<primary>question"])
      end

      def quit_action
        Gio::SimpleAction.new("quit").tap do |action|
          action.signal_connect("activate") { app.quit }
        end
      end

      # Upstream references app.shortcuts from its menu and from
      # shortcuts-dialog.ui but never installs it; the action is installed here
      # so the menu item works.
      def shortcuts_action
        Gio::SimpleAction.new("shortcuts").tap do |action|
          action.signal_connect("activate") { ShortcutsDialog.new.present(window) }
        end
      end
  end
end
