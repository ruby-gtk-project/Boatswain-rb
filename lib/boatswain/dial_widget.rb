# frozen_string_literal: true

require "gtk4"

module Boatswain
  # DialWidget is the port of src/bs-dial-widget.c and its .ui: one dial in
  # the dial-grid flow box.
  #
  # Upstream's template is an otherwise empty GtkWidget with its css-name set
  # to "streamdeckdialwidget" — the circular size and background come from
  # that selector in style.css and nothing is drawn in code. The Ruby
  # bindings cannot set a widget class's css-name (see Style), so the
  # equivalent selector there is ".bs-dial-widget" and this widget adds that
  # class itself, exactly as every other ported widget does.
  class DialWidget < Gtk::Widget
    type_register

    def initialize(dial)
      super()
      @dial = dial
      self.halign = :center
      self.valign = :center
      add_css_class("bs-dial-widget")
    end

    attr_reader :dial
  end
end
