# frozen_string_literal: true

require "adwaita"

module Boatswain
  # Icon is the port of lib/bs-icon.c: what a button shows — a background
  # colour, an image (a themed icon name, a file or a paintable) and a caption
  # drawn over it.
  #
  # Upstream BsIcon implements GdkPaintable and does its own snapshot so the
  # same object can be handed both to a GtkPicture and to the HID renderer.
  # The Ruby bindings cannot implement GdkPaintable, so Icon here is the model
  # only and IconWidget draws it; the drawing follows bs_icon.c's snapshot_icon
  # (background, then the paintable, then the caption, with the relative icon
  # filling in whatever this icon does not set).
  class Icon
    # Upstream's own default: an icon with no opacity of its own takes its
    # relative icon's.
    INHERIT_OPACITY = -1.0

    # Upstream draws symbolic icons at a fixed 32px inside the button.
    ICON_SIZE = 32

    def self.transparent = Gdk::RGBA.new(
      0.0,
      0.0,
      0.0,
      0.0,
    )

    # Port of bs_icon_new_empty.
    def self.new_empty = new

    # Port of bs_icon_new.
    def self.with_background(background_color, paintable = nil)
      new.tap do |icon|
        icon.background_color = background_color
        icon.paintable = paintable
      end
    end

    # Port of bs_icon_new_from_json.
    def self.from_json(data)
      new.tap do |icon|
        icon.background_color = Gdk::RGBA.parse(data["background-color"] || "rgba(0,0,0,0)")
        if data["file"]
          icon.file = Gio::File.new_for_uri(data["file"])
        end
        icon.icon_name = data["icon-name"]
        icon.text = data["text"]
      end
    end

    def initialize
      @background_color = self.class.transparent
      @color = nil
      @opacity = INHERIT_OPACITY
      @callbacks = []
    end

    attr_reader :background_color, :color, :file, :icon_name, :text, :opacity, :paintable, :relative

    # Upstream emits GdkPaintable's "invalidate-contents" and
    # "invalidate-size"; both mean "redraw me", so there is one callback list.
    def on_changed(&block)
      @callbacks << block
      block
    end

    def disconnect(callback) = @callbacks.delete(callback)

    def background_color=(value)
      @background_color = value || self.class.transparent
      changed
    end

    def color=(value)
      @color = value
      changed
    end

    def file=(value)
      @file = value
      if value
        @icon_name = nil
        @paintable = Gdk::Texture.new(value)
      end
      changed
    end

    def icon_name=(value)
      @icon_name = value
      if value
        @file = nil
        @paintable = nil
      end
      changed
    end

    def text=(value)
      @text = value
      changed
    end

    def opacity=(value)
      @opacity = value
      changed
    end

    def paintable=(value)
      @paintable = value
      changed
    end

    # Port of bs_icon_set_relative: the action's own icon, which a custom icon
    # falls back to for anything it does not override.
    def relative=(value)
      @relative = value
      changed
    end

    # Port of get_real_opacity.
    def effective_opacity
      if @opacity == INHERIT_OPACITY && @relative
        @relative.opacity
      else
        @opacity
      end
    end

    # The image this icon draws, falling back to the relative icon the way
    # snapshot_any_paintable does.
    def effective_paintable
      if @paintable
        @paintable
      elsif @relative
        @relative.paintable
      end
    end

    def effective_icon_name
      if @icon_name
        @icon_name
      elsif @relative
        @relative.icon_name
      end
    end

    # The caption, falling back the way snapshot_any_layout does.
    def effective_text
      if presence(@text)
        @text
      elsif @relative
        @relative.text
      end
    end

    # Port of generate_foreground_color: black on light backgrounds, white on
    # dark ones, measured on the premultiplied background colour.
    def foreground_color
      if @color
        @color
      elsif intensity > 0.5
        Gdk::RGBA.new(
          0.0,
          0.0,
          0.0,
          1.0,
        )
      else
        Gdk::RGBA.new(
          1.0,
          1.0,
          1.0,
          1.0,
        )
      end
    end

    # True when nothing distinguishes this icon from an absent one. Port of
    # bs-button-editor.c's maybe_remove_custom_icon test.
    def empty?
      @background_color == self.class.transparent &&
        @file.nil? &&
        @icon_name.nil? &&
        @paintable.nil? &&
        presence(@text).nil?
    end

    # Port of bs_icon_to_json.
    def to_json_data
      { "background-color" => @background_color.to_s, "text" => @text }.tap do |data|
        if @file
          data["file"] = @file.uri
        end
        if @icon_name
          data["icon-name"] = @icon_name
        end
      end
    end

    def changed
      @callbacks.dup.each { |callback| callback.call(self) }
    end

    private

    # INTENSITY() in bs-icon.c, over the premultiplied colour.
      def intensity
        @background_color.then do |rgba|
          (rgba.red * rgba.alpha * 0.30) +
            (rgba.green * rgba.alpha * 0.59) +
            (rgba.blue * rgba.alpha * 0.11)
        end
      end

      def presence(value)
        if value.to_s.strip.empty?
          nil
        else
          value
        end
      end
  end
end
