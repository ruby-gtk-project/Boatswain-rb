# frozen_string_literal: true

require "adwaita"

module Boatswain
  module Plugins
    module Default
      # MultiActionRow is the port of
      # src/plugins/default/default-multi-action-row.c and its .ui: one row of
      # the multi-action editor's list, showing either a delay with its spin
      # button or an action with a button that opens its settings.
      class MultiActionRow < Adwaita::ActionRow
        DELAY_LOWER = 100
        DELAY_UPPER = 99_999_999
        DELAY_STEP = 1
        DELAY_PAGE = 100

        def initialize(entry, on_changed:, on_edit:, on_remove:)
          super()
          @entry = entry
          @on_changed = on_changed
          @on_edit = on_edit
          @on_remove = on_remove
          build
        end

        attr_reader :entry

        private

          def build
            tap do |row|
              row.add_prefix(icon)
              row.add_suffix(delay_spin_button)
              row.add_suffix(edit_button)
              row.add_suffix(remove_button)

              if @entry.entry_type == :delay
                configure_delay_entry
              else
                configure_action_entry
              end
            end
          end

          def configure_delay_entry
            self.title = "Delay"
            icon.icon_name = "preferences-system-time-symbolic"
            delay_spin_button.tap do |spin|
              spin.visible = true
              spin.adjustment.value = @entry.delay_ms
              spin.adjustment.signal_connect("notify::value") do
                @entry.delay_ms = spin.adjustment.value.to_i
                @on_changed.call(self)
              end
            end
          end

          def configure_action_entry
            self.title = @entry.action.name.to_s
            icon.icon_name = action_icon_name
            edit_button.visible = true
          end

          def action_icon_name
            @entry.action.factory&.info(@entry.action.id)&.icon_name
          end

          def icon
            @icon ||= Gtk::Image.new.tap { |image| image.valign = :center }
          end

          def delay_spin_button
            @delay_spin_button ||= Gtk::SpinButton.new(delay_adjustment, DELAY_STEP, 0).tap do |spin|
              spin.visible = false
              spin.valign = :center
            end
          end

          def delay_adjustment
            @delay_adjustment ||= Gtk::Adjustment.new(
              250,
              DELAY_LOWER,
              DELAY_UPPER,
              DELAY_STEP,
              DELAY_PAGE,
              0,
            )
          end

          def edit_button
            @edit_button ||= Gtk::Button.new.tap do |button|
              button.visible = false
              button.valign = :center
              button.icon_name = "preferences-system-symbolic"
              button.add_css_class("flat")
              button.signal_connect("clicked") { @on_edit.call(self) }
            end
          end

          def remove_button
            @remove_button ||= Gtk::Button.new.tap do |button|
              button.valign = :center
              button.icon_name = "edit-delete-symbolic"
              button.add_css_class("flat")
              button.signal_connect("clicked") { @on_remove.call(self) }
            end
          end
      end
    end
  end
end
