# frozen_string_literal: true

require "adwaita"

module Boatswain
  # Style is the port of src/style.css, loaded onto the default display the way
  # the gresource-backed stylesheet is loaded upstream.
  #
  # Upstream addresses several widgets by the CSS name their class sets —
  # streamdeckbuttonwidget, streamdeckbuttoneditor, streamdeckdialwidget,
  # touchscreenslotwidget, touchscreensloteditor. The Ruby bindings cannot set
  # a widget class's CSS name, so each of those names becomes the style class
  # the corresponding Ruby widget adds to itself, and the selectors below are
  # rewritten to match. Every other rule is upstream's verbatim.
  #
  # The two bs-icon rules are the one addition: IconWidget draws a symbolic
  # icon and its caption in the foreground colour BsIcon computes in
  # generate_foreground_color, and a widget's colour has to come from CSS.
  class Style
    CSS = <<~CSS
      button windowtitle label.subtitle {
          font-weight: normal;
      }

      list.navigation-sidebar row menubutton button {
        min-width: 0;
        min-height: 0;
      }

      list.navigation-sidebar row:not(:selected) menubutton button image {
        color: alpha(currentColor, 0.33);
      }

      entry.new-profile-entry {
        background-color: transparent;
      }

      .bs-button-widget:selected {
        background-color: transparent;
      }

      .bs-button-widget:selected > overlay {
        outline: 2px solid @accent_color;
      }

      .bs-button-widget:selected:focus:focus-visible {
        outline: none;
      }

      .bs-button-widget:drop(active) {
        box-shadow: none;
      }

      .bs-button-widget:drop(active) overlay {
        background-color: alpha(@card_bg_color, 2.0);
        filter: brightness(125%);
        transition: 250ms;
      }

      .bs-button-editor .cutout {
        background-color: @window_bg_color;
        border-radius: 9999px;
        padding: 2px;
      }

      popover.custom-icon-popover > contents {
        padding: 0;
      }

      popover.custom-icon-popover gridview {
        background-color: @popover_bg_color;
      }

      popover.custom-icon-popover gridview,
      popover.custom-icon-popover gridview image {
        padding: 12px;
      }

      /* Dials */
      .bs-dial-widget {
        min-width: 72px;
        min-height: 72px;
        border-radius: 9999px;
        background-color: @card_bg_color;
      }

      /* Touchscreen */
      .bs-touchscreen-slot-widget {
        outline-color: transparent;
        outline-offset: -2px;
        transition: outline 250ms;
      }

      .bs-touchscreen-slot-widget:first-child,
      .bs-touchscreen-slot-widget:dir(rtl):last-child {
        border-top-left-radius: calc(var(--window-radius) - 3px);
        border-bottom-left-radius: calc(var(--window-radius) - 3px);
        border-top-right-radius: 0;
        border-bottom-right-radius: 0;
      }

      .bs-touchscreen-slot-widget:last-child,
      .bs-touchscreen-slot-widget:dir(rtl):first-child {
        border-top-left-radius: 0;
        border-bottom-left-radius: 0;
        border-top-right-radius: calc(var(--window-radius) - 3px);
        border-bottom-right-radius: calc(var(--window-radius) - 3px);
      }

      .bs-touchscreen-slot-widget:selected {
        outline: 2px solid @accent_color;
      }

      .bs-touchscreen-slot-editor box.header {
        margin-left: 12px;
        margin-right: 12px;
        border-spacing: 6px;
      }

      row.touchscreen-background {
        padding: 0;
        min-height: 50px;
      }

      row.touchscreen-default-background {
        font-weight: bold;
      }

      .bs-icon-light {
        color: #ffffff;
      }

      .bs-icon-dark {
        color: #000000;
      }

      @media (prefers-contrast: more) {
        splitbutton.omnibar {
          box-shadow: inset 0 0 0 1px alpha(currentColor, .75);
        }
      }
    CSS

    # Upstream loads style.css from the gresource at application startup; the
    # priority is GTK's own application priority.
    def self.load(display = Gdk::Display.default)
      new.load(display)
    end

    def load(display)
      Gtk::StyleContext.add_provider_for_display(
        display,
        provider,
        Gtk::StyleProvider::PRIORITY_APPLICATION,
      )
      provider
    end

    def provider
      @provider ||= Gtk::CssProvider.new.tap { |css| css.load(data: CSS) }
    end
  end
end
