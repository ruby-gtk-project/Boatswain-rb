# frozen_string_literal: true

require "adwaita"

require_relative "multi_action_row"

module Boatswain
  module Plugins
    module Default
      # MultiActionEditor is the port of
      # src/plugins/default/default-multi-action-editor.c and its .ui: the
      # preferences widget of the Multiple Actions action, plus the two dialogs
      # it opens — one listing the actions that can be added, one showing the
      # settings of an action already in the list.
      #
      # Upstream's two dialogs are GtkDialog with use-header-bar, which is
      # deprecated. They are reproduced here as modal Gtk::Window with a
      # Gtk::HeaderBar and hide-on-close, which is what GtkDialog with
      # use-header-bar is, rather than as Adwaita::Dialog, which would change
      # how they are presented.
      class MultiActionEditor < Gtk::Box
        DEFAULT_DELAY_MS = 250
        DIALOG_WIDTH = 450
        DIALOG_HEIGHT = 450

        # Port of can_add_action: an action list cannot nest itself, and a
        # folder inside a list has nowhere to go back to.
        DENYLIST = [
          ["default", "default-multi-action"],
          ["default", "default-switch-page-action"],
        ].freeze

        def initialize(multi_action)
          super(:vertical, 18)
          @multi_action = multi_action
          self.hexpand = true
          build
        end

        # Port of default_multi_action_editor_update_entries.
        def update_entries = recreate_entry_rows

        private

          def build
            tap do |box|
              box.append(entries_group)
              box.append(add_group)

              entries_group.tap { |group| group.add(entries_listbox) }
              add_group.tap { |group| group.add(add_action_row) }

              actions_page.tap do |page|
                page.add(actions_group)
                page.add(others_group)
              end

              @multi_action.context.action_factories.each { |factory| add_action_factory(factory) }
              others_group.add(delay_row)

              recreate_entry_rows
            end
          end

          # Port of recreate_entry_rows.
          def recreate_entry_rows
            while (child = entries_listbox.first_child)
              entries_listbox.remove(child)
            end

            @multi_action.entries.each do |entry|
              entries_listbox.append(entry_row(entry))
            end

            entries_group.visible = !@multi_action.entries.empty?
          end

          def entry_row(entry)
            MultiActionRow.new(
              entry,
              on_changed: method(:on_entry_row_changed),
              on_edit:    method(:on_entry_row_edit),
              on_remove:  method(:on_entry_row_remove),
            )
          end

          # Port of on_entry_row_changed_cb.
          def on_entry_row_changed(_row) = @multi_action.changed

          # Port of on_entry_row_edit_cb.
          def on_entry_row_edit(row)
            preferences_bin.child = row.entry.action.preferences
            present_dialog(preferences_dialog)
          end

          # Port of on_entry_row_remove_cb.
          def on_entry_row_remove(row)
            @multi_action.remove_entry(row.index)
            recreate_entry_rows
          end

          # Port of add_action_factory.
          def add_action_factory(factory)
            Adwaita::ExpanderRow.new.tap do |expander|
              expander.title = factory.display_name
              expander.add_prefix(Gtk::Image.new(icon_name: factory.icon_name))

              factory.reject { |info| denied?(factory, info) }.each do |info|
                expander.add_row(action_row(factory, info))
              end

              actions_group.add(expander)
            end
          end

          def denied?(factory, info)
            DENYLIST.include?([factory.module_name, info.id])
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

          # Port of on_action_row_activated_cb.
          def on_action_row_activated(factory, info)
            @multi_action.add_action(factory.create_action(info))
            recreate_entry_rows
            actions_dialog.close
          end

          # Port of on_delay_row_activated_cb.
          def on_delay_row_activated
            @multi_action.add_delay(DEFAULT_DELAY_MS)
            recreate_entry_rows
            actions_dialog.close
          end

          # Port of on_add_action_row_activated_cb.
          def present_dialog(dialog)
            dialog.tap do |window|
              window.transient_for = root
              window.present
            end
          end

          def entries_group
            @entries_group ||= Adwaita::PreferencesGroup.new.tap { |group| group.hexpand = true }
          end

          def entries_listbox
            @entries_listbox ||= Gtk::ListBox.new.tap do |listbox|
              listbox.selection_mode = :none
              listbox.add_css_class("boxed-list")
            end
          end

          def add_group
            @add_group ||= Adwaita::PreferencesGroup.new.tap { |group| group.hexpand = true }
          end

          def add_action_row
            @add_action_row ||= Adwaita::ActionRow.new.tap do |row|
              row.title = "Add action"
              row.activatable = true
              row.add_suffix(Gtk::Image.new(icon_name: "go-next-symbolic").tap { |i| i.valign = :center })
              row.signal_connect("activated") { present_dialog(actions_dialog) }
            end
          end

          def delay_row
            @delay_row ||= Adwaita::ActionRow.new.tap do |row|
              row.title = "Delay"
              row.activatable = true
              row.add_prefix(Gtk::Image.new(icon_name: "preferences-system-time-symbolic"))
              row.signal_connect("activated") { on_delay_row_activated }
            end
          end

          def actions_dialog
            @actions_dialog ||= dialog("Add Action", actions_page)
          end

          def actions_page
            @actions_page ||= Adwaita::PreferencesPage.new
          end

          def actions_group
            @actions_group ||= Adwaita::PreferencesGroup.new.tap { |group| group.title = "Actions" }
          end

          def others_group
            @others_group ||= Adwaita::PreferencesGroup.new.tap { |group| group.title = "Others" }
          end

          def preferences_dialog
            @preferences_dialog ||= dialog("Settings", preferences_page).tap do |window|
              window.signal_connect("close-request") do
                preferences_bin.child = nil
                false
              end
            end
          end

          def preferences_page
            @preferences_page ||= Adwaita::PreferencesPage.new.tap do |page|
              page.add(preferences_group)
            end
          end

          def preferences_group
            @preferences_group ||= Adwaita::PreferencesGroup.new.tap do |group|
              group.add(preferences_bin)
            end
          end

          def preferences_bin
            @preferences_bin ||= Adwaita::Bin.new
          end

          def dialog(title, content)
            Gtk::Window.new.tap do |window|
              window.title = title
              window.modal = true
              window.hide_on_close = true
              window.set_default_size(DIALOG_WIDTH, DIALOG_HEIGHT)
              window.titlebar = Gtk::HeaderBar.new
              window.child = content
            end
          end
      end
    end
  end
end
