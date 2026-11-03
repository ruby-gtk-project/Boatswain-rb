# frozen_string_literal: true

require "adwaita"

require_relative "rb/version"

module Boatswain
  # AboutDialog is the port of the About dialog built in src/bs-window.c's
  # on_show_about_action_activated_cb, which calls adw_show_about_dialog with
  # the credits below. Upstream has no .ui file for it; the component is the
  # argument list of that one call.
  class AboutDialog < Adwaita::AboutDialog
    APPLICATION_NAME = "Boatswain"
    APPLICATION_ICON = "com.feaneron.Boatswain"
    COPYRIGHT = "© 2022 Georges Basile Stavracas Neto"
    DEVELOPERS = ["Georges Basile Stavracas Neto <georges.stavracas@gmail.com>"].freeze
    ARTISTS = ["Jakub Steiner", "Sam Hewitt"].freeze
    WEBSITE = "https://gitlab.gnome.org/World/boatswain"
    ISSUE_URL = "https://gitlab.gnome.org/World/boatswain/issues"

    def initialize
      super()
      self.application_name = APPLICATION_NAME
      self.application_icon = APPLICATION_ICON
      self.version = Rb::VERSION
      self.copyright = COPYRIGHT
      self.developers = DEVELOPERS
      self.artists = ARTISTS
      self.license_type = :gpl_3_0
      self.website = WEBSITE
      self.issue_url = ISSUE_URL
    end

    # Port of adw_show_about_dialog's own presentation: the dialog is built and
    # presented against the widget that asked for it.
    def self.show(parent)
      new.present(parent)
    end
  end
end
