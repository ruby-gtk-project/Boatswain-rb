# frozen_string_literal: true

require_relative "../../action"
require_relative "multi_action_editor"

module Boatswain
  module Plugins
    module Default
      # MultiAction is the port of
      # src/plugins/default/default-multi-action.c: runs a list of other
      # actions, with optional delays between them, when the key is pressed.
      #
      # Upstream walks the list from an idle callback and re-arms itself with a
      # timeout for each delay entry, so a press that lands while a run is
      # still going cancels it first. The same sequencing is kept here with
      # GLib idle and timeout sources.
      class MultiAction < Boatswain::Action
        # One entry of the list: either a delay in milliseconds, or an action.
        Entry = Struct.new(:entry_type, :delay_ms, :action)

        def initialize(context)
          super()
          @context = context
          @entries = []
          @current_entry = 0
          icon.icon_name = "stacked-plates-symbolic"
        end

        attr_reader :entries, :context

        # The editor is built once and reused, as upstream's ref_sink does.
        def preferences
          @preferences ||= MultiActionEditor.new(self)
        end

        # Port of default_multi_action_add_action.
        def add_action(action)
          cancel_ongoing_run
          Entry.new(:action, nil, action).tap do |entry|
            action.on_changed { changed }
            @entries << entry
            changed
          end
        end

        # Port of default_multi_action_add_delay.
        def add_delay(delay_ms)
          cancel_ongoing_run
          Entry.new(:delay, delay_ms, nil).tap do |entry|
            @entries << entry
            changed
          end
        end

        # Port of on_entry_row_remove_cb's half of the work.
        def remove_entry(position)
          cancel_ongoing_run
          @entries.delete_at(position).tap { changed }
        end

        # Port of default_multi_action_handle_event.
        def handle_event(event)
          if event.event_type == :button_press
            cancel_ongoing_run
            @event = event
            @run_source_id = GLib::Idle.add { run_next_entry }
          end
        end

        # Port of default_multi_action_serialize_settings.
        def serialize_settings
          { "entries" => @entries.map { |entry| entry_json(entry) } }
        end

        # Port of default_multi_action_deserialize_settings.
        def deserialize_settings(settings)
          Array(settings["entries"]).each do |entry_data|
            entry_from_json(entry_data).then do |entry|
              if entry
                @entries << entry
              end
            end
          end
          @preferences&.update_entries
        end

        private

          # Port of run_actions_in_idle_cb.
          def run_next_entry
            @entries[@current_entry].then do |entry|
              if entry.nil?
                finish_run
              else
                @current_entry += 1
                run_entry(entry)
              end
            end
          end

          def run_entry(entry)
            if entry.entry_type == :delay
              @run_source_id = GLib::Timeout.add(entry.delay_ms) { run_next_entry }
              GLib::Source::REMOVE
            else
              entry.action.handle_event(@event)
              GLib::Source::CONTINUE
            end
          end

          def finish_run
            @event = nil
            @run_source_id = nil
            GLib::Source::REMOVE
          end

          # Port of cancel_ongoing_run.
          def cancel_ongoing_run
            if @run_source_id
              GLib::Source.remove(@run_source_id)
              @run_source_id = nil
            end
            @event = nil
            @current_entry = 0
          end

          def entry_json(entry)
            if entry.entry_type == :delay
              { "type" => "delay", "delay" => entry.delay_ms }
            else
              action_entry_json(entry.action)
            end
          end

          def action_entry_json(action)
            {
              "type"    => "action",
              "factory" => action.factory&.module_name,
              "action"  => action.id,
            }.tap do |data|
              action.serialize_settings.then do |settings|
                if settings
                  data["settings"] = settings
                end
              end
            end
          end

          def entry_from_json(data)
            case data["type"]
            when "delay" then Entry.new(:delay, data["delay"].to_i, nil)
            when "action" then action_entry_from_json(data)
            else
              warn "Invalid entry type"
              nil
            end
          end

          def action_entry_from_json(data)
            @context.action_factory(data["factory"]).then do |factory|
              factory&.info(data["action"]).then do |info|
                if info
                  Entry.new(:action, nil, build_child_action(factory, info, data["settings"]))
                end
              end
            end
          end

          def build_child_action(factory, info, settings)
            factory.create_action(info).tap do |action|
              if settings
                action.deserialize_settings(settings)
              end
              action.on_changed { changed }
            end
          end
      end
    end
  end
end
