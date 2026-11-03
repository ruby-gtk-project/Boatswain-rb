# frozen_string_literal: true

require "adwaita"

require_relative "about_dialog"
require_relative "device_editor"
require_relative "profile_row"

module Boatswain
  # Window is the port of src/bs-window.c and src/bs-window.ui: the main
  # window. It is a stack of two pages — the "No Stream Deck Found" status page
  # and the split view that shows the devices — with the device list in a
  # popover on the sidebar header bar, the profile list and the new-profile
  # entry below it, and the brightness slider and firmware version along the
  # bottom.
  #
  # Upstream names its device editor stack pages after each device's pointer;
  # the serial number serves the same purpose here and is stable across runs.
  class Window < Adwaita::ApplicationWindow
    DEFAULT_WIDTH = 1200
    DEFAULT_HEIGHT = 720
    POPOVER_WIDTH = 200
    BRIGHTNESS_STEP = 0.02
    BRIGHTNESS_PAGE = 0.05
    MARGIN = 12

    def initialize(application, context)
      super(application)
      @context = context
      @editors = {}
      build
    end

    attr_reader :current_device

    private

      def build
        tap do |window|
          window.title = "Boatswain"
          window.set_default_size(DEFAULT_WIDTH, DEFAULT_HEIGHT)
          window.content = main_stack
          window.insert_action_group("win", action_group)

          main_stack.tap do |stack|
            stack.add_named(empty_toolbar_view, "empty")
            stack.add_named(split_view, "devices")
          end

          empty_toolbar_view.tap do |view|
            view.add_top_bar(empty_header_bar)
            view.content = empty_page
          end

          empty_header_bar.pack_end(primary_menu_button)

          split_view.tap do |view|
            view.sidebar = sidebar_page
            view.content = content_page
          end

          sidebar_toolbar_view.tap do |view|
            view.add_top_bar(sidebar_header_bar)
            view.content = sidebar_scrolled_window
            view.add_bottom_bar(bottom_grid)
          end

          sidebar_header_bar.tap do |bar|
            bar.title_widget = devices_menu_button
            bar.pack_end(sidebar_primary_menu_button)
          end

          devices_menu_button.tap do |button|
            button.child = devices_menu_button_box
            button.popover = devices_popover
          end

          devices_menu_button_box.tap do |box|
            box.append(device_window_title)
            box.append(Gtk::Image.new(icon_name: "pan-down-symbolic"))
          end

          devices_popover.child = devices_listbox

          sidebar_scrolled_window.child = sidebar_box

          sidebar_box.tap do |box|
            box.append(profiles_listbox)
            box.append(new_profile_name_entry)
          end

          build_bottom_grid

          install_actions
          add_close_binding
          connect_devices
        end
      end

      def build_bottom_grid
        bottom_grid.tap do |grid|
          grid.attach(
            caption_label("Brightness"),
            0,
            1,
            1,
            1,
          )
          grid.attach(
            brightness_scale,
            1,
            1,
            1,
            1,
          )
          grid.attach(
            caption_label("Firmware"),
            0,
            4,
            1,
            1,
          )
          grid.attach(
            firmware_version_label,
            1,
            4,
            1,
            1,
          )
        end
      end

      def install_actions
        action_group.add_action(about_action)
      end

      def about_action
        Gio::SimpleAction.new("about").tap do |action|
          action.signal_connect("activate") { AboutDialog.show(self) }
        end
      end

      # Port of gtk_widget_class_add_binding_action for <primary>w.
      def add_close_binding
        Gtk::ShortcutController.new.tap do |controller|
          controller.add_shortcut(close_shortcut)
          add_controller(controller)
        end
      end

      def close_shortcut
        Gtk::Shortcut.new(
          Gtk::KeyvalTrigger.new(Gdk::Keyval::KEY_w, :control_mask),
          Gtk::NamedAction.new("window.close"),
        )
      end

      def connect_devices
        @context.devices.on_items_changed do |position, removed, added|
          on_devices_items_changed(position, removed, added)
        end
        on_devices_items_changed(0, 0, @context.devices.n_items)
      end

      # Port of on_device_manager_items_changed_cb.
      def on_devices_items_changed(position, removed, added)
        removed.times { remove_editor_at(position) }
        added.times { |offset| add_editor_at(position + offset) }

        rebuild_device_rows

        if @context.devices.n_items.positive?
          select_first_device_if_needed
          main_stack.visible_child_name = "devices"
        else
          main_stack.visible_child_name = "empty"
        end
      end

      def remove_editor_at(position)
        device_order.delete_at(position).then do |serial_number|
          @editors.delete(serial_number).then do |editor|
            if editor
              devices_stack.remove(editor)
            end
          end
        end
      end

      def add_editor_at(position)
        @context.devices[position].then do |device|
          DeviceEditor.new(@context, device).then do |editor|
            @editors[device.serial_number] = editor
            device_order.insert(position, device.serial_number)
            devices_stack.add_named(editor, device.serial_number)
          end
        end
      end

      def device_order
        @device_order ||= []
      end

      def select_first_device_if_needed
        if @current_device.nil?
          select_device(@context.devices[0])
        end
      end

      # Port of select_device.
      def select_device(device)
        unless @current_device.equal?(device)
          main_stack.visible_child_name = "devices"
          @current_device = device
          devices_stack.visible_child_name = device.serial_number
          firmware_version_label.label = device.firmware_version.to_s
          device_window_title.tap do |title|
            title.title = device.name.to_s
            title.subtitle = device.serial_number.to_s
          end
          sync_brightness(device)
          rebuild_profile_rows
        end
      end

      # Upstream binds the device's "brightness" property to the scale's
      # adjustment bidirectionally; the device is a plain Ruby object here, so
      # the two sides are kept in step by hand.
      def sync_brightness(device)
        @updating_brightness = true
        brightness_adjustment.value = device.brightness
        @updating_brightness = false
      end

      def on_brightness_changed
        unless @updating_brightness || @current_device.nil?
          @current_device.brightness = brightness_adjustment.value
          @current_device.schedule_save
        end
      end

      # Port of create_device_row_cb, over the device list.
      def rebuild_device_rows
        clear(devices_listbox)
        @context.devices.each { |device| devices_listbox.append(device_row(device)) }
      end

      def device_row(device)
        Gtk::ListBoxRow.new.tap do |row|
          row.child = device_row_box(device)
        end
      end

      def device_row_box(device)
        Gtk::Box.new(:vertical, 0).tap do |box|
          box.margin_top = 3
          box.margin_bottom = 3
          box.append(device_row_label(device.name))
          box.append(
            device_row_label(device.serial_number).tap do |label|
                        label.add_css_class("caption")
                        label.add_css_class("dim-label")
                      end,
          )
        end
      end

      def device_row_label(text)
        Gtk::Label.new(text.to_s).tap do |label|
          label.hexpand = true
          label.halign = :start
          label.xalign = 0.0
        end
      end

      # Port of on_devices_listbox_row_activated_cb.
      def on_device_row_activated(row)
        @context.devices[row.index].then do |device|
          if device
            select_device(device)
          end
        end
        devices_menu_button.popdown
      end

      # Port of create_profile_row_cb.
      def rebuild_profile_rows
        clear(profiles_listbox)
        @current_device.then do |device|
          if device
            device.profiles.each { |profile| profiles_listbox.append(profile_row(device, profile)) }
          end
        end
      end

      def profile_row(device, profile)
        ProfileRow.new(device, profile) { |row, new_position| on_profile_row_move(row, new_position) }
      end

      # Port of on_profile_row_move_cb.
      def on_profile_row_move(row, new_position)
        @current_device.profiles.then do |profiles|
          profiles.find_item(row.profile).then do |position|
            unless position.nil?
              profiles.remove(position)
              profiles.insert(new_position.clamp(0, profiles.n_items), row.profile)
              rebuild_profile_rows
            end
          end
        end
      end

      # Port of on_profiles_listbox_row_activated_cb.
      def on_profile_row_activated(row)
        @current_device.profiles[row.index].then do |profile|
          if profile
            @current_device.load_profile(profile)
          end
        end
      end

      # Port of append_new_profile.
      def append_new_profile
        new_profile_name_entry.text.to_s.strip.then do |name|
          unless name.empty? || @current_device.nil?
            add_profile(name)
            new_profile_name_entry.text = ""
          end
        end
      end

      def add_profile(name)
        Profile.new_empty(@context, @current_device).tap do |profile|
          profile.name = name
          @current_device.profiles.append(profile)
          @current_device.load_profile(profile)
          rebuild_profile_rows
        end
      end

      def clear(listbox)
        while (child = listbox.first_child)
          listbox.remove(child)
        end
      end

      def action_group
        @action_group ||= Gio::SimpleActionGroup.new
      end

      def main_stack
        @main_stack ||= Gtk::Stack.new
      end

      def empty_toolbar_view
        @empty_toolbar_view ||= Adwaita::ToolbarView.new
      end

      def empty_header_bar
        @empty_header_bar ||= Adwaita::HeaderBar.new
      end

      def empty_page
        @empty_page ||= Adwaita::StatusPage.new.tap do |page|
          page.vexpand = true
          page.icon_name = "com.feaneron.Boatswain"
          page.title = "No Stream Deck Found"
          page.description = "Plug in a Stream Deck device to use it"
        end
      end

      def primary_menu_button
        @primary_menu_button ||= menu_button
      end

      def sidebar_primary_menu_button
        @sidebar_primary_menu_button ||= menu_button
      end

      def menu_button
        Gtk::MenuButton.new.tap do |button|
          button.icon_name = "open-menu-symbolic"
          button.menu_model = primary_menu
          button.primary = true
        end
      end

      # Port of the `primary_menu` object in bs-window.ui.
      def primary_menu
        @primary_menu ||= Gio::Menu.new.tap do |menu|
          Gio::Menu.new.tap do |section|
            section.append("_Keyboard Shortcuts", "app.shortcuts")
            section.append("_About Boatswain", "win.about")
            section.append("_Quit", "app.quit")
            menu.append_section(nil, section)
          end
        end
      end

      def split_view
        @split_view ||= Adwaita::NavigationSplitView.new
      end

      def sidebar_page
        @sidebar_page ||= Adwaita::NavigationPage.new(sidebar_toolbar_view, "Devices").tap do |page|
          page.tag = "sidebar"
        end
      end

      def content_page
        @content_page ||= Adwaita::NavigationPage.new(devices_stack, "Device").tap do |page|
          page.tag = "content"
        end
      end

      def sidebar_toolbar_view
        @sidebar_toolbar_view ||= Adwaita::ToolbarView.new
      end

      def sidebar_header_bar
        @sidebar_header_bar ||= Adwaita::HeaderBar.new
      end

      def devices_menu_button
        @devices_menu_button ||= Gtk::MenuButton.new.tap do |button|
          button.add_css_class("flat")
        end
      end

      def devices_menu_button_box
        @devices_menu_button_box ||= Gtk::Box.new(:horizontal, 0)
      end

      def device_window_title
        @device_window_title ||= Adwaita::WindowTitle.new("", "")
      end

      def devices_popover
        @devices_popover ||= Gtk::Popover.new.tap do |popover|
          popover.width_request = POPOVER_WIDTH
        end
      end

      def devices_listbox
        @devices_listbox ||= Gtk::ListBox.new.tap do |listbox|
          listbox.selection_mode = :none
          listbox.add_css_class("navigation-sidebar")
          listbox.signal_connect("row-activated") { |_box, row| on_device_row_activated(row) }
        end
      end

      def sidebar_scrolled_window
        @sidebar_scrolled_window ||= Gtk::ScrolledWindow.new.tap do |window|
          window.vscrollbar_policy = :never
        end
      end

      def sidebar_box
        @sidebar_box ||= Gtk::Box.new(:vertical, 2)
      end

      def profiles_listbox
        @profiles_listbox ||= Gtk::ListBox.new.tap do |listbox|
          listbox.selection_mode = :browse
          listbox.add_css_class("navigation-sidebar")
          listbox.signal_connect("row-activated") { |_box, row| on_profile_row_activated(row) }
        end
      end

      def new_profile_name_entry
        @new_profile_name_entry ||= Gtk::Entry.new.tap do |entry|
          entry.margin_bottom = 6
          entry.margin_start = 6
          entry.margin_end = 6
          entry.placeholder_text = "New profile…"
          entry.add_css_class("new-profile-entry")
          entry.signal_connect("activate") { append_new_profile }
        end
      end

      def bottom_grid
        @bottom_grid ||= Gtk::Grid.new.tap do |grid|
          grid.margin_top = MARGIN
          grid.margin_bottom = MARGIN
          grid.margin_start = MARGIN
          grid.margin_end = MARGIN
          grid.row_spacing = 6
          grid.column_spacing = MARGIN
        end
      end

      def caption_label(text)
        Gtk::Label.new(text).tap do |label|
          label.xalign = 0.0
          label.add_css_class("dim-label")
        end
      end

      def brightness_scale
        @brightness_scale ||= Gtk::Scale.new(:horizontal, brightness_adjustment).tap do |scale|
          scale.hexpand = true
          scale.valign = :center
          scale.round_digits = 3
          scale.draw_value = false
        end
      end

      def brightness_adjustment
        @brightness_adjustment ||= Gtk::Adjustment.new(
          Profile::DEFAULT_BRIGHTNESS,
          0.0,
          1.0,
          BRIGHTNESS_STEP,
          BRIGHTNESS_PAGE,
          0.0,
        ).tap do |adjustment|
          adjustment.signal_connect("value-changed") { on_brightness_changed }
        end
      end

      def firmware_version_label
        @firmware_version_label ||= Gtk::Label.new.tap do |label|
          label.hexpand = true
          label.xalign = 1.0
          label.selectable = true
        end
      end

      def devices_stack
        @devices_stack ||= Gtk::Stack.new.tap do |stack|
          stack.vexpand = true
          stack.transition_type = :crossfade
        end
      end
  end
end
