# frozen_string_literal: true

require "adwaita"

module Boatswain
  # ProfileRow is the port of src/bs-profile-row.c and its .ui: one row of the
  # window's profile list, with a menu that renames, reorders or deletes the
  # profile, and a tick showing which profile is active.
  #
  # The three menu actions live in a "profile-row" action group on the row, as
  # upstream installs them, and move-up/move-down report the position they want
  # through the row's `move` callback because the list that holds the profiles
  # is the device's, not the row's.
  class ProfileRow < Adwaita::PreferencesRow
    def initialize(device, profile, &on_move)
      super()
      @device = device
      @profile = profile
      @on_move = on_move
      build
    end

    attr_reader :profile

    private

      def build
        tap do |row|
          row.child = center_box
          row.insert_action_group("profile-row", action_group)

          center_box.tap do |box|
            box.start_widget = title_box
            box.end_widget = menu_button
          end

          title_box.tap do |box|
            box.append(title_label)
            box.append(selected_icon)
          end

          install_actions
          connect_profile
          update_actions
          update_selected_icon_visibility
        end
      end

      def connect_profile
        self.title = @profile.name
        rename_entry.text = @profile.name

        @profile.on_changed do |profile|
          self.title = profile.name
          title_label.label = profile.name
        end

        @device.profiles.on_items_changed { |_p, _r, _a| update_actions }
        @device.on_changed { update_selected_icon_visibility }
      end

      def install_actions
        action_group.tap do |group|
          group.add_action(simple_action("delete") { on_delete })
          group.add_action(simple_action("move-up") { @on_move&.call(self, index - 1) })
          group.add_action(simple_action("move-down") { @on_move&.call(self, index + 1) })
        end
      end

      def simple_action(name, &block)
        Gio::SimpleAction.new(name).tap do |action|
          action.signal_connect("activate") { block.call }
        end
      end

      # Port of update_actions: delete needs a second profile to fall back to,
      # and the moves need somewhere to move to.
      def update_actions
        @device.profiles.then do |profiles|
          profiles.find_item(@profile).then do |position|
            lookup("delete").enabled = profiles.n_items > 1
            lookup("move-up").enabled = !position.nil? && position.positive?
            lookup("move-down").enabled = !position.nil? && position + 1 < profiles.n_items
          end
        end
      end

      def lookup(name) = action_group.lookup_action(name)

      # Port of update_selected_icon_visibility.
      def update_selected_icon_visibility
        selected_icon.visible = @device.active_profile.equal?(@profile)
      end

      # Port of on_delete_action_activated_cb: deleting the active profile
      # moves to the next one, or the previous one when it was the last.
      def on_delete
        @device.profiles.then do |profiles|
          profiles.find_item(@profile).then do |position|
            unless position.nil?
              load_replacement_profile(profiles, position)
              profiles.remove(position)
            end
          end
        end
      end

      def load_replacement_profile(profiles, position)
        if @device.active_profile.equal?(@profile)
          replacement_for(profiles, position).then do |replacement|
            if replacement
              @device.load_profile(replacement)
            end
          end
        end
      end

      def replacement_for(profiles, position)
        profiles[position + 1] || previous_profile(profiles, position)
      end

      def previous_profile(profiles, position)
        if position.positive?
          profiles[position - 1]
        end
      end

      # Port of rename_profile.
      def on_rename
        if rename_button.sensitive?
          @profile.name = rename_entry.text
          menu_button.popdown
        end
      end

      # Port of validate_rename_entry.
      def validate_rename_entry
        rename_entry.text.to_s.strip.then do |name|
          rename_button.sensitive = !name.empty? && name != @profile.name
        end
      end

      def action_group
        @action_group ||= Gio::SimpleActionGroup.new
      end

      def center_box
        @center_box ||= Gtk::CenterBox.new
      end

      def title_box
        @title_box ||= Gtk::Box.new(:horizontal, 6).tap { |box| box.hexpand = true }
      end

      def title_label
        @title_label ||= Gtk::Label.new.tap do |label|
          label.valign = :center
          label.ellipsize = :none
          label.lines = 0
          label.xalign = 0
          label.add_css_class("title")
        end
      end

      def selected_icon
        @selected_icon ||= Gtk::Image.new(icon_name: "object-select-symbolic")
      end

      def menu_button
        @menu_button ||= Gtk::MenuButton.new.tap do |button|
          button.valign = :center
          button.icon_name = "view-more-symbolic"
          button.add_css_class("flat")
          button.popover = popover_menu
        end
      end

      def popover_menu
        @popover_menu ||= Gtk::PopoverMenu.new(menu_model).tap do |popover|
          popover.add_child(rename_box, "rename")
        end
      end

      # Port of the `menu` object in bs-profile-row.ui.
      def menu_model
        Gio::Menu.new.tap do |menu|
          menu.append_section(nil, move_section)
          menu.append_section(nil, delete_section)
        end
      end

      def move_section
        Gio::Menu.new.tap do |section|
          section.append_submenu("Rename", rename_submenu)
          section.append("Move up", "profile-row.move-up")
          section.append("Move down", "profile-row.move-down")
        end
      end

      def rename_submenu
        Gio::Menu.new.tap do |submenu|
          submenu.append_item(rename_menu_item)
        end
      end

      def rename_menu_item
        Gio::MenuItem.new.tap do |item|
          item.set_attribute_value("custom", GLib::Variant.new("rename"))
        end
      end

      def delete_section
        Gio::Menu.new.tap do |section|
          section.append("Delete", "profile-row.delete")
        end
      end

      def rename_box
        @rename_box ||= Gtk::Box.new(:horizontal, 6).tap do |box|
          box.margin_top = 12
          box.append(rename_entry)
          box.append(rename_button)
        end
      end

      def rename_entry
        @rename_entry ||= Gtk::Entry.new.tap do |entry|
          entry.signal_connect("activate") { on_rename }
          entry.signal_connect("notify::text") { validate_rename_entry }
        end
      end

      def rename_button
        @rename_button ||= Gtk::Button.new.tap do |button|
          button.label = "Rename"
          button.add_css_class("suggested-action")
          button.signal_connect("clicked") { on_rename }
        end
      end
  end
end
