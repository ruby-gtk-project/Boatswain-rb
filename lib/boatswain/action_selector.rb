# frozen_string_literal: true

require "adwaita"

module Boatswain
  # ActionSelector is the port of src/bs-action-selector.c and its .ui: the
  # "Actions" page of the button editor, one expander row per plugin with that
  # plugin's actions inside it.
  #
  # Upstream binds the list box to the context's action factory list; the
  # factories here are a plain Ruby list, so the rows are built once from it.
  # The hidden entries of a catalogue are skipped, as upstream skips them.
  class ActionSelector < Adwaita::Bin
    def initialize(context, &on_action_selected)
      super()
      @context = context
      @on_action_selected = on_action_selected
      build
    end

    private

      def build
        tap do |selector|
          selector.child = toolbar_view

          toolbar_view.tap do |view|
            view.add_top_bar(header_bar)
            view.content = preferences_page
          end

          preferences_page.add(preferences_group)
          preferences_group.add(actions_listbox)

          @context.action_factories.each do |factory|
            actions_listbox.append(factory_row(factory))
          end
        end
      end

      # Port of create_factory_row_cb.
      def factory_row(factory)
        Adwaita::ExpanderRow.new.tap do |expander|
          expander.title = factory.display_name
          expander.add_prefix(Gtk::Image.new(icon_name: factory.icon_name))

          factory.reject(&:hidden?).each do |info|
            expander.add_row(action_row(factory, info))
          end
        end
      end

      def action_row(factory, info)
        Adwaita::ActionRow.new.tap do |row|
          row.title = info.name
          row.subtitle = info.description.to_s
          row.activatable = true
          row.add_prefix(Gtk::Image.new(icon_name: info.icon_name))
          row.signal_connect("activated") { on_action_row_activated(factory, info) }
        end
      end

      # Port of on_action_row_activated_cb: every expander collapses, then the
      # choice is reported.
      def on_action_row_activated(factory, info)
        collapse_all
        @on_action_selected&.call(factory, info)
      end

      def collapse_all
        child = actions_listbox.first_child
        while child
          if child.is_a?(Adwaita::ExpanderRow)
            child.expanded = false
          end
          child = child.next_sibling
        end
      end

      def toolbar_view
        @toolbar_view ||= Adwaita::ToolbarView.new.tap { |view| view.top_bar_style = :flat }
      end

      def header_bar
        @header_bar ||= Adwaita::HeaderBar.new.tap do |bar|
          bar.show_start_title_buttons = false
        end
      end

      def preferences_page
        @preferences_page ||= Adwaita::PreferencesPage.new
      end

      def preferences_group
        @preferences_group ||= Adwaita::PreferencesGroup.new
      end

      def actions_listbox
        @actions_listbox ||= Gtk::ListBox.new.tap do |listbox|
          listbox.selection_mode = :none
          listbox.add_css_class("boxed-list")
        end
      end
  end
end
