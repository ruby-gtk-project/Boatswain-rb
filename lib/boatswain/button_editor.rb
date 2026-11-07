# frozen_string_literal: true

require "adwaita"

require_relative "action_selector"
require_relative "empty_action"
require_relative "icon"
require_relative "icon_widget"

module Boatswain
  # ButtonEditor is the port of src/bs-button-editor.c and its .ui: the sidebar
  # of the device editor when a key is selected. It shows the key's icon with
  # the controls that override it — a built-in icon, a file, a caption and a
  # background colour — then the selected action's own preferences, and the
  # navigation page that picks a new action.
  #
  # Upstream shows the icon through a GtkPicture over the BsIcon paintable; the
  # icon is drawn by an IconWidget here for the reason IconWidget records. The
  # built-in icon grid is a GtkGridView over a sorted, filtered GtkStringList
  # of every symbolic icon in the theme, exactly as upstream builds it.
  class ButtonEditor < Adwaita::Bin
    ICON_SIZE = 128
    POPOVER_SIZE = 300

    class << self
      # Port of the static builtin_icons_list built once in
      # bs_button_editor_init.
      def builtin_icons
        @builtin_icons ||= Gtk::StringList.new(symbolic_icon_names)
      end

      def symbolic_icon_names
        Gtk::IconTheme.get_for_display(Gdk::Display.default)
          .icon_names
          .select { |name| name.end_with?("-symbolic") }
      end
    end

    def initialize(context, button)
      super()
      @context = context
      @button = button
      add_css_class("bs-button-editor")
      build
    end

    attr_reader :button

    private

      def build
        tap do |editor|
          editor.child = navigation_view

          navigation_view.tap do |view|
            view.add(button_page)
            view.add(actions_page)
          end

          button_toolbar_view.tap do |view|
            view.add_top_bar(button_header_bar)
            view.content = button_preferences_page
          end

          button_preferences_page.tap do |page|
            page.add(icon_group)
            page.add(action_preferences_group)
            page.add(select_action_group)
            page.add(remove_group)
          end

          icon_group.tap do |group|
            group.add(icon_overlay)
            group.add(icon_properties_listbox)
          end

          icon_overlay.tap do |overlay|
            overlay.child = icon_widget
            overlay.add_overlay(remove_custom_icon_bin)
            overlay.add_overlay(custom_icon_menu_button)
          end

          icon_properties_listbox.tap do |listbox|
            listbox.append(custom_icon_text_row)
            listbox.append(background_color_row)
          end

          select_action_group.add(select_action_row)
          remove_group.add(remove_action_row)

          connect_button
          update_action_preferences_group
          setup_button
          update_icon
        end
      end

      def connect_button
        @button.on_action_changed do
          update_action_preferences_group
          setup_button
          update_icon
        end

        @button.on_icon_changed do
          setup_button
          update_icon
        end
      end

      # Port of setup_button: the first key of a subpage is the Go Up key and
      # cannot be edited, and the controls show whatever the custom icon holds.
      def setup_button
        self.sensitive = @button.position.positive? || @button.device.active_page.root?
        remove_custom_icon_button.visible = !@button.custom_icon.nil?
        remove_custom_icon_bin.visible = !@button.custom_icon.nil?

        @updating = true
        apply_custom_icon_to_controls
        @updating = false
      end

      def apply_custom_icon_to_controls
        @button.custom_icon.then do |custom_icon|
          if custom_icon
            set_custom_icon_text_row_text(custom_icon.text.to_s)
            set_background_color_button_rgba(custom_icon.background_color)
          else
            set_custom_icon_text_row_text("")
            set_background_color_button_rgba(Icon.transparent)
          end
        end
      end

      # GTK still re-emits "notify::text"/"notify::rgba" synchronously for a
      # property set to its own current value, and that reentrant emission —
      # landing here while this very call is still inside the signal dispatch
      # that triggered it — is pathologically slow in this binding/runtime
      # combination (confirmed by isolating this single write: skipping it
      # entirely turns a multi-minute stall into microseconds). Upstream's C
      # setters have no such cost, so this port skips the write only when the
      # value already matches, which keeps the row showing exactly what it
      # would have shown while never re-entering the signal at all.
      def set_custom_icon_text_row_text(text)
        unless custom_icon_text_row.text == text
          custom_icon_text_row.text = text
        end
      end

      def set_background_color_button_rgba(rgba)
        unless background_color_button.rgba == rgba
          background_color_button.rgba = rgba
        end
      end

      def update_icon
        icon_widget.icon = @button.icon
      end

      # Port of update_action_preferences_group.
      def update_action_preferences_group
        @button.action.then do |action|
          remove_group.visible = !action.nil? && !action.is_a?(EmptyAction)
          swap_action_preferences(action&.preferences)
        end
      end

      def swap_action_preferences(preferences)
        unless @action_preferences.equal?(preferences)
          if @action_preferences
            action_preferences_group.remove(@action_preferences)
          end
          @action_preferences = preferences
          if preferences
            action_preferences_group.add(preferences)
          end
          action_preferences_group.visible = !preferences.nil?
        end
      end

      # Port of on_action_selector_action_selected_cb.
      def on_action_selected(factory, info)
        @button.action = factory.create_action(info)
        update_action_preferences_group
        navigation_view.pop
      end

      # Port of on_remove_row_activated_cb.
      def on_remove_action
        @button.action = EmptyAction.new
      end

      # Port of on_remove_custom_icon_button_clicked_cb.
      def on_remove_custom_icon
        @button.custom_icon = nil
        background_color_button.rgba = Icon.transparent
      end

      # Port of on_custom_icon_text_row_text_changed_cb.
      def on_custom_icon_text_changed
        unless @updating
          apply_text(custom_icon_text_row.text.to_s)
        end
      end

      def apply_text(text)
        if text.empty?
          clear_text
        else
          editable_custom_icon.tap do |icon|
            icon.text = text
            @button.custom_icon = icon
          end
        end
      end

      def clear_text
        @button.custom_icon.then do |icon|
          if icon
            icon.text = nil
            maybe_remove_custom_icon
          end
        end
      end

      # Port of maybe_remove_custom_icon.
      def maybe_remove_custom_icon
        @button.custom_icon.then do |icon|
          if icon&.empty?
            @button.custom_icon = nil
          end
        end
      end

      # Port of on_background_color_dialog_button_rgba_changed_cb.
      def on_background_color_changed
        unless @updating
          editable_custom_icon.tap do |icon|
            icon.background_color = background_color_button.rgba
            @button.custom_icon = icon
          end
        end
      end

      # Port of on_icons_gridview_activate_cb.
      def on_builtin_icon_activated(position)
        filtered_icons.get_item(position).then do |item|
          editable_custom_icon.tap do |icon|
            icon.file = nil
            icon.paintable = nil
            icon.icon_name = item.string
            @button.custom_icon = icon
          end
        end
      end

      # Port of on_custom_icon_button_clicked_cb and
      # on_file_dialog_file_opened_cb.
      def on_custom_icon_file_requested
        custom_icon_menu_button.popdown
        icon_file_dialog.open(root, nil) { |dialog, result| finish_open(dialog, result) }
      end

      def finish_open(dialog, result)
        editable_custom_icon.tap do |icon|
          icon.file = dialog.open_finish(result)
          @button.custom_icon = icon
        end
      rescue Gtk::DialogError, Gio::Error => e
        warn "Error opening file: #{e.message}"
      end

      # Upstream refs the existing custom icon or makes a fresh empty one; the
      # same icon object is then handed back to the button.
      def editable_custom_icon = @button.custom_icon || Icon.new_empty

      def navigation_view
        @navigation_view ||= Adwaita::NavigationView.new
      end

      def button_page
        @button_page ||= Adwaita::NavigationPage.new(button_toolbar_view, "Button Properties").tap do |page|
          page.tag = "button"
        end
      end

      def actions_page
        @actions_page ||= Adwaita::NavigationPage.new(action_selector, "Actions").tap do |page|
          page.tag = "actions"
        end
      end

      def action_selector
        @action_selector ||= ActionSelector.new(@context) do |factory, info|
          on_action_selected(factory, info)
        end
      end

      def button_toolbar_view
        @button_toolbar_view ||= Adwaita::ToolbarView.new.tap { |view| view.top_bar_style = :flat }
      end

      def button_header_bar
        @button_header_bar ||= Adwaita::HeaderBar.new.tap do |bar|
          bar.show_start_title_buttons = false
        end
      end

      def button_preferences_page
        @button_preferences_page ||= Adwaita::PreferencesPage.new
      end

      def icon_group
        @icon_group ||= Adwaita::PreferencesGroup.new
      end

      def icon_overlay
        @icon_overlay ||= Gtk::Overlay.new.tap do |overlay|
          overlay.halign = :center
          overlay.valign = :center
          overlay.margin_bottom = 24
        end
      end

      def icon_widget
        @icon_widget ||= IconWidget.new(nil, size: ICON_SIZE).tap do |widget|
          widget.margin_top = 12
          widget.margin_bottom = 12
          widget.margin_start = 12
          widget.margin_end = 12
          widget.add_css_class("card")
        end
      end

      def remove_custom_icon_bin
        @remove_custom_icon_bin ||= Adwaita::Bin.new.tap do |bin|
          bin.halign = :end
          bin.valign = :start
          bin.add_css_class("cutout")
          bin.child = remove_custom_icon_button
        end
      end

      def remove_custom_icon_button
        @remove_custom_icon_button ||= Gtk::Button.new.tap do |button|
          button.icon_name = "window-close-symbolic"
          button.add_css_class("circular")
          button.add_css_class("outline")
          button.signal_connect("clicked") { on_remove_custom_icon }
        end
      end

      def custom_icon_menu_button
        @custom_icon_menu_button ||= Gtk::MenuButton.new.tap do |button|
          button.halign = :end
          button.valign = :end
          button.icon_name = "document-edit-symbolic"
          button.add_css_class("circular")
          button.add_css_class("outline")
          button.add_css_class("cutout")
          button.popover = custom_icon_popover
        end
      end

      def custom_icon_popover
        @custom_icon_popover ||= Gtk::Popover.new.tap do |popover|
          popover.add_css_class("custom-icon-popover")
          popover.child = custom_icon_popover_box
        end
      end

      def custom_icon_popover_box
        @custom_icon_popover_box ||= Gtk::Box.new(:vertical, 0).tap do |box|
          box.append(custom_icon_search_box)
          box.append(Gtk::Separator.new(:horizontal))
          box.append(icons_scrolled_window)
        end
      end

      def custom_icon_search_box
        @custom_icon_search_box ||= Gtk::Box.new(:horizontal, 6).tap do |box|
          box.margin_top = 6
          box.margin_bottom = 6
          box.margin_start = 6
          box.margin_end = 6
          box.append(icons_search_entry)
          box.append(custom_icon_file_button)
        end
      end

      def icons_search_entry
        @icons_search_entry ||= Gtk::SearchEntry.new.tap do |entry|
          entry.hexpand = true
          entry.signal_connect("search-changed") { icons_filter.search = entry.text }
        end
      end

      def custom_icon_file_button
        @custom_icon_file_button ||= Gtk::Button.new.tap do |button|
          button.icon_name = "document-open-symbolic"
          button.signal_connect("clicked") { on_custom_icon_file_requested }
        end
      end

      def icons_scrolled_window
        @icons_scrolled_window ||= Gtk::ScrolledWindow.new.tap do |window|
          window.hscrollbar_policy = :never
          window.min_content_width = POPOVER_SIZE
          window.min_content_height = POPOVER_SIZE
          window.child = icons_grid_view
        end
      end

      def icons_grid_view
        @icons_grid_view ||= Gtk::GridView.new.tap do |view|
          view.single_click_activate = true
          view.model = Gtk::NoSelection.new(sorted_icons)
          view.factory = icons_factory
          view.signal_connect("activate") { |_view, position| on_builtin_icon_activated(position) }
        end
      end

      def icons_factory
        @icons_factory ||= Gtk::SignalListItemFactory.new.tap do |factory|
          factory.signal_connect("setup") { |_f, item| item.child = Gtk::Image.new }
          factory.signal_connect("bind") { |_f, item| bind_icon_item(item) }
        end
      end

      def bind_icon_item(item)
        item.item.string.then do |name|
          item.child.icon_name = name
          item.child.tooltip_text = name
        end
      end

      def sorted_icons
        @sorted_icons ||= Gtk::SortListModel.new(filtered_icons, icons_sorter)
      end

    # Both the sorter and the filter take their expression through the
    # property setter: the Ruby constructor does not accept a Gtk::Expression
    # as an argument.
      def icons_sorter
        @icons_sorter ||= Gtk::StringSorter.new.tap do |sorter|
          sorter.expression = icon_name_expression
        end
      end

      def filtered_icons
        @filtered_icons ||= Gtk::FilterListModel.new(self.class.builtin_icons, icons_filter)
      end

      def icons_filter
        @icons_filter ||= Gtk::StringFilter.new.tap do |filter|
          filter.expression = icon_name_expression
          filter.match_mode = :substring
        end
      end

      def icon_name_expression
        Gtk::PropertyExpression.new(Gtk::StringObject, nil, "string")
      end

      def icon_properties_listbox
        @icon_properties_listbox ||= Gtk::ListBox.new.tap do |listbox|
          listbox.selection_mode = :none
          listbox.add_css_class("boxed-list")
        end
      end

      def custom_icon_text_row
        @custom_icon_text_row ||= Adwaita::EntryRow.new.tap do |row|
          row.title = "Name"
          row.signal_connect("notify::text") { on_custom_icon_text_changed }
        end
      end

      def background_color_row
        @background_color_row ||= Adwaita::ActionRow.new.tap do |row|
          row.title = "Background Color"
          row.activatable_widget = background_color_button
          row.add_suffix(background_color_button)
        end
      end

      def background_color_button
        @background_color_button ||= Gtk::ColorDialogButton.new.tap do |button|
          button.valign = :center
          button.dialog = background_color_dialog
          button.signal_connect("notify::rgba") { on_background_color_changed }
        end
      end

      def background_color_dialog
        @background_color_dialog ||= Gtk::ColorDialog.new.tap do |dialog|
          dialog.modal = true
          dialog.with_alpha = false
        end
      end

      def action_preferences_group
        @action_preferences_group ||= Adwaita::PreferencesGroup.new.tap do |group|
          group.visible = false
        end
      end

      def select_action_group
        @select_action_group ||= Adwaita::PreferencesGroup.new
      end

      def select_action_row
        @select_action_row ||= Adwaita::ButtonRow.new.tap do |row|
          row.title = "Select Action"
          row.end_icon_name = "go-next-symbolic"
          row.add_css_class("suggested-action")
          row.signal_connect("activated") { navigation_view.push_by_tag("actions") }
        end
      end

      def remove_group
        @remove_group ||= Adwaita::PreferencesGroup.new
      end

      def remove_action_row
        @remove_action_row ||= Adwaita::ButtonRow.new.tap do |row|
          row.title = "Remove Action"
          row.add_css_class("destructive-action")
          row.signal_connect("activated") { on_remove_action }
        end
      end

      def icon_file_dialog
        Gtk::FileDialog.new.tap do |dialog|
          dialog.modal = true
          dialog.title = "Select icon"
          dialog.accept_label = "Open"
          dialog.filters = icon_file_filters
        end
      end

      def icon_file_filters
        Gio::ListStore.new(Gtk::FileFilter).tap do |filters|
          filters.append(icon_file_filter)
        end
      end

      def icon_file_filter
        Gtk::FileFilter.new.tap do |filter|
          filter.name = "All supported formats"
          filter.add_mime_type("image/*")
          filter.add_mime_type("video/*")
        end
      end
  end
end
