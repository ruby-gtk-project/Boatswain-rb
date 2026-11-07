# frozen_string_literal: true

require "ashpd"
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
  # portal when its window is closed (XdpPortal::request_background,
  # src/bs-application.c), and installs UNIX signal handlers and a --debug
  # option. The --debug option is the GLib debug logging switch and has no
  # counterpart here; the background hold is attempted for real below, in
  # request_background, and is this port's gem gap, not a design choice:
  # closing the window quits because it genuinely cannot do otherwise.
  class Application
    APPLICATION_ID = "com.feaneron.Boatswain"

    def build
      app.tap do
        app.signal_connect("startup") do
          Adwaita::StyleManager.default.color_scheme = :prefer_dark
          Style.load
          request_background
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

      # Port of the XdpPortal::request_background call in
      # bs_application_startup. ashpd (the only version rubygems has, 0.1.1)
      # implements only Account, Camera, OpenUri, Screenshot, Settings and
      # Trash — Ashpd.constants carries no Background — so this always takes
      # the rescue branch today; it is wired to the real gem, rather than
      # stubbed, so it starts working the day a release adds the portal.
      def request_background
        Ashpd::Background.request(reason: "Boatswain needs to keep running to upload button changes")
      rescue NameError => e
        warn "Cannot hold Boatswain in the background: #{e.message} (ashpd #{Ashpd::VERSION} has no Background portal)"
      end

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
