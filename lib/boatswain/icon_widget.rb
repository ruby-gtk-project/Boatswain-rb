# frozen_string_literal: true

require "adwaita"
require_relative "icon"

module Boatswain
  # IconWidget draws an Icon.
  #
  # It stands in for the GdkPaintable half of lib/bs-icon.c. Upstream BsIcon is
  # itself a paintable, so bs-button-widget.ui and bs-button-editor.ui just put
  # it in a GtkPicture; the Ruby bindings cannot implement GdkPaintable, so the
  # snapshot code of bs_icon.c (snapshot_icon, snapshot_any_paintable,
  # snapshot_any_layout) is reproduced here as a widget: the background colour
  # is drawn by a DrawingArea, the image sits centred over it, and the caption
  # sits along the bottom edge, exactly as upstream lays them out.
  class IconWidget < Gtk::Overlay
    def initialize(icon = nil, size: 72)
      super()
      @size = size
      self.width_request = size
      self.height_request = size
      self.overflow = :hidden
      build
      self.icon = icon
    end

    attr_reader :icon

    def icon=(value)
      if @icon && @icon_callback
        @icon.disconnect(@icon_callback)
      end

      @icon = value
      @icon_callback = value&.on_changed { update }
      update
    end

    def update
      background.queue_draw
      update_image
      update_caption
    end

    private

      def build
        tap do |overlay|
          overlay.child = background
          overlay.add_overlay(image)
          overlay.add_overlay(caption)
        end
      end

      def update_image
        image.tap do |widget|
          widget.icon_name = nil
          widget.paintable = nil

          @icon&.effective_icon_name.then do |icon_name|
            if icon_name
              widget.icon_name = icon_name
            end
          end

          @icon&.effective_paintable.then do |paintable|
            if paintable
              widget.paintable = paintable
            end
          end

          widget.opacity = image_opacity
          apply_foreground(widget)
        end
      end

      def update_caption
        caption.tap do |label|
          label.label = @icon&.effective_text.to_s
          label.visible = !label.label.empty?
          apply_foreground(label)
        end
      end

      # Upstream's -1.0 opacity means "fully opaque"; see get_real_opacity and
      # the gtk_snapshot_push_opacity guard around it.
      def image_opacity
        @icon&.effective_opacity.then do |opacity|
          if opacity.nil? || opacity == Icon::INHERIT_OPACITY
            1.0
          else
            opacity
          end
        end
      end

      # Port of generate_foreground_color. A symbolic icon and the caption take
      # their colour from CSS, so the choice lands as one of the two classes
      # Style defines.
      def apply_foreground(widget)
        widget.remove_css_class("bs-icon-light")
        widget.remove_css_class("bs-icon-dark")
        @icon.then do |icon|
          if icon
            widget.add_css_class(foreground_class(icon))
          end
        end
      end

      def foreground_class(icon)
        if icon.foreground_color.red > 0.5
          "bs-icon-light"
        else
          "bs-icon-dark"
        end
      end

      def background
        @background ||= Gtk::DrawingArea.new.tap do |area|
          area.set_draw_func do |_area, context, width, height|
            draw_background(context, width, height)
          end
        end
      end

      def draw_background(context, width, height)
        (@icon&.background_color || Icon.transparent).then do |rgba|
          context.set_source_rgba(
            rgba.red,
            rgba.green,
            rgba.blue,
            rgba.alpha,
          )
          context.rectangle(
            0,
            0,
            width,
            height,
          )
          context.fill
        end
      end

      def image
        @image ||= Gtk::Image.new.tap do |widget|
          widget.halign = :center
          widget.valign = :center
          widget.pixel_size = Icon::ICON_SIZE
        end
      end

      def caption
        @caption ||= Gtk::Label.new.tap do |label|
          label.halign = :center
          label.valign = :end
          label.margin_bottom = 3
          label.ellipsize = :end
          label.add_css_class("caption")
        end
      end
  end
end
