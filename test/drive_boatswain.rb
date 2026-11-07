# frozen_string_literal: true

# Drives the whole app headlessly: builds the window with two mock devices,
# switches between them, adds/renames/reorders/deletes profiles, edits a
# button's icon and action through the real ActionSelector/ButtonEditor flow,
# exercises the plugin actions' own preferences widgets, opens the About and
# Shortcuts dialogs, and screenshots each state.
#
# Why this file exists: a GTK app that loads is not a GTK app that works.
# `ruby -c` passes on code that builds an empty window, and this run is the
# first time anything under lib/ has ever been loaded in a process with GTK
# actually running.
#
# Run it from the repository root:
#
#   nix develop --command xvfb-run -a ruby test/drive_boatswain.rb
#
# Most of Boatswain's widget methods are `private` (unlike the house style's
# "keep widget methods public" rule — see the ruby-gtk-testing skill). Rather
# than rewrite every class's visibility, which the gap this task is scoped to
# does not call for, this driver reaches through with `#send` wherever a
# memoized widget method is private. Everything that is public is called the
# way the UI would call it: signals are emitted, actions are activated by
# name, entries and adjustments are set and read back.

require "fileutils"
require "stringio"
require "tmpdir"

# Ruby's own IO buffering (separate from libc's, so stdbuf cannot reach it)
# otherwise holds every "ok"/"FAIL" line until the process exits, which
# makes a slow run look hung when it is only slow. Flush each line as it is
# printed so the run's actual progress is visible.
STDOUT.sync = true

# A scratch config and data directory, so a run never reads or writes the
# real one — this is also where the mock devices' profile JSON lands.
SCRATCH = Dir.mktmpdir("boatswain-drive")
ENV["XDG_CONFIG_HOME"] = File.join(SCRATCH, "config")
ENV["XDG_DATA_HOME"] = File.join(SCRATCH, "data")

# Two devices, one of each model, so the sidebar's device switcher, the two
# different button-grid sizes (15 keys / 5 columns vs 32 keys / 8 columns)
# and the per-device profile lists all have something real to exercise.
ENV["BOATSWAIN_N_DEVICES"] = "2"

$LOAD_PATH.unshift(File.expand_path("../lib", __dir__))

require "boatswain"
require_relative "gtk_driver"

SHOTS = File.expand_path("../tmp/shots", __dir__)
FileUtils.rm_rf(SHOTS)

# Turns the main loop over for `ms` milliseconds and comes back, the same
# polling idiom tally-rb's drive script uses: a step cannot sleep, because
# every step runs inside a GLib timeout dispatched by the very loop a sleep
# would block.
def pump(ms)
  (Process.clock_gettime(Process::CLOCK_MONOTONIC) + (ms / 1000.0)).then do |deadline|
    while Process.clock_gettime(Process::CLOCK_MONOTONIC) < deadline
      GLib::MainContext.default.iteration(false)
    end
  end
end

# Depth-first walk of a widget's tree. Used to find a row (an ExpanderRow's
# own rows, an AdwAboutDialog's link labels) that has no accessor of its own.
def find_widget(widget, &predicate)
  if predicate.call(widget)
    widget
  else
    found = nil
    child = widget.first_child
    while child && found.nil?
      found = find_widget(child, &predicate)
      child = child.next_sibling
    end
    found
  end
end

def profile_path_for(device)
  File.join(ENV.fetch("XDG_DATA_HOME"), "boatswain", "#{device.serial_number}.json")
end

def current_editor(app)
  app.window.send(:devices_stack).visible_child
end

GtkDriver.drive(Boatswain::Application.new, shots: SHOTS) do |d, app|
  d.window { app.window }

  d.step("the window builds with two mock devices already plugged in") do
    d.check("the window is an Adwaita::ApplicationWindow") do
      app.window.is_a?(Adwaita::ApplicationWindow)
    end
    d.check("it is the only toplevel") { Gtk::Window.list_toplevels.length == 1 }
    d.check("its title is Boatswain") { app.window.title == "Boatswain" }
    d.check("two devices are registered") { app.window.instance_variable_get(:@context).devices.n_items == 2 }
    d.check("the devices page is showing, not the empty one") do
      app.window.send(:main_stack).visible_child_name == "devices"
    end
    d.check("a device was auto-selected") { !app.window.current_device.nil? }
    d.check("the device rows match the device list") do
      app.window.send(:devices_listbox).then do |listbox|
        (0...2).all? { |i| !listbox.get_row_at_index(i).nil? }
      end
    end
    d.shot("01-two-devices")
  end

  d.step("the first device (Hangar Mild, 15 keys) is selected and shown") do
    d.check("the current device is the first mock device") do
      app.window.current_device.equal?(app.window.instance_variable_get(:@context).devices[0])
    end
    d.check("its name is Feaneron Hangar Mild") { app.window.current_device.name == "Feaneron Hangar Mild" }
    d.check("the window title names it") { app.window.send(:device_window_title).title == "Feaneron Hangar Mild" }
    d.check("the subtitle is its serial number") do
      app.window.send(:device_window_title).subtitle == app.window.current_device.serial_number
    end
    d.check("the firmware label shows the port's own version") do
      app.window.send(:firmware_version_label).label == Boatswain::Rb::VERSION
    end
    d.check("it has 15 buttons laid out 5 per row") do
      app.window.current_device.regions.first.then do |region|
        region.buttons.length == 15 && region.grid_columns == 5
      end
    end
    d.check("it starts with one profile, Default") do
      app.window.current_device.profiles.map(&:name) == ["Default"]
    end
    d.check("that profile is the active one") do
      app.window.current_device.active_profile.equal?(app.window.current_device.profiles[0])
    end
  end

  d.step("the brightness slider carries the profile's default and moves the device") do
    d.check("it starts at the profile default, 0.5") do
      app.window.send(:brightness_adjustment).value == Boatswain::Profile::DEFAULT_BRIGHTNESS
    end
    app.window.send(:brightness_adjustment).value = 0.8
    d.check("the device's brightness followed the slider") { app.window.current_device.brightness == 0.8 }
    d.check("a save is armed but has not run yet") { !File.exist?(profile_path_for(app.window.current_device)) }
  end

  d.step("the five-second debounce writes the brightness change to disk") do
    pump((Boatswain::Device::SAVE_DELAY_SECONDS * 1000) + 500)
    profile_path_for(app.window.current_device).then do |path|
      d.check("the profile file exists now") { File.exist?(path) }
      d.check("it carries the new brightness") do
        JSON.parse(File.read(path))["profiles"].first["brightness"] == 0.8
      end
    end
  end

  d.step("switching devices through the sidebar popover's row list") do
    app.window.send(:devices_listbox).get_row_at_index(1).then do |row|
      app.window.send(:devices_listbox).signal_emit("row-activated", row)
    end
  end

  d.step("the second device (Hangar XL, 32 keys) is now current") do
    d.check("the current device changed") do
      app.window.current_device.equal?(app.window.instance_variable_get(:@context).devices[1])
    end
    d.check("its name is Feaneron Hangar XL") { app.window.current_device.name == "Feaneron Hangar XL" }
    d.check("it has 32 buttons laid out 8 per row") do
      app.window.current_device.regions.first.then do |region|
        region.buttons.length == 32 && region.grid_columns == 8
      end
    end
    d.check("the brightness slider reset to this device's own value") do
      app.window.send(:brightness_adjustment).value == Boatswain::Profile::DEFAULT_BRIGHTNESS
    end
    d.check("the first device's brightness was not disturbed") do
      app.window.instance_variable_get(:@context).devices[0].brightness == 0.8
    end
    d.check("the popover closed itself") { !app.window.send(:devices_menu_button).active? }
    d.shot("02-second-device")
  end

  d.step("switching back to the first device") do
    app.window.send(:devices_listbox).get_row_at_index(0).then do |row|
      app.window.send(:devices_listbox).signal_emit("row-activated", row)
    end
  end

  d.step("the first device's own brightness came back") do
    d.check("the current device is the first one again") do
      app.window.current_device.equal?(app.window.instance_variable_get(:@context).devices[0])
    end
    d.check("its brightness followed it") { app.window.send(:brightness_adjustment).value == 0.8 }
  end

  d.step("a new profile is created from the sidebar entry") do
    app.window.send(:new_profile_name_entry).text = "Streaming"
    app.window.send(:new_profile_name_entry).signal_emit("activate")
  end

  d.step("Streaming exists, is active, and the entry cleared itself") do
    d.check("two profiles now") { app.window.current_device.profiles.map(&:name) == ["Default", "Streaming"] }
    d.check("the new one is active") { app.window.current_device.active_profile.name == "Streaming" }
    d.check("the entry was cleared") { app.window.send(:new_profile_name_entry).text.empty? }
    d.check("two rows are in the sidebar list") do
      (0...2).all? { |i| !app.window.send(:profiles_listbox).get_row_at_index(i).nil? } &&
        app.window.send(:profiles_listbox).get_row_at_index(2).nil?
    end
    d.shot("03-two-profiles")
  end

  d.step("the profile row's rename flow renames through the real entry and button") do
    app.window.send(:profiles_listbox).get_row_at_index(0).then do |row|
      d.check("the first row is Default") { row.profile.name == "Default" }
      d.check("rename starts insensitive (name unchanged)") { !row.send(:rename_button).sensitive? }
      row.send(:rename_entry).text = "Main"
      d.check("typing a new name makes it sensitive") { row.send(:rename_button).sensitive? }
      row.send(:rename_button).signal_emit("clicked")
      d.check("the profile's name changed") { row.profile.name == "Main" }
      d.check("the row's own title followed it") { row.title == "Main" }
    end
  end

  d.step("the selected-profile tick shows on the active row, not the renamed one") do
    app.window.send(:profiles_listbox).get_row_at_index(0).then do |main_row|
      d.check("Main is not selected") { !main_row.send(:selected_icon).visible? }
    end
    app.window.send(:profiles_listbox).get_row_at_index(1).then do |streaming_row|
      d.check("Streaming is selected") { streaming_row.send(:selected_icon).visible? }
    end
  end

  d.step("move-down is a real GAction on the row, resolved through win action groups") do
    app.window.send(:profiles_listbox).get_row_at_index(0).then do |row|
      d.check("move-down is enabled with a second profile to move past") do
        row.send(:action_group).lookup_action("move-down").enabled?
      end
      row.activate_action("profile-row.move-down", nil)
    end
    d.check("Main moved after Streaming in the device's own list") do
      app.window.current_device.profiles.map(&:name) == ["Streaming", "Main"]
    end
    d.check("the sidebar rebuilt in the new order") do
      app.window.send(:profiles_listbox).get_row_at_index(0).profile.name == "Streaming"
    end
  end

  d.step("delete removes the profile from the device and the sidebar both") do
    app.window.send(:profiles_listbox).get_row_at_index(1).then do |main_row|
      d.check("it is the Main row") { main_row.profile.name == "Main" }
      d.check("delete is enabled with two profiles") { main_row.send(:action_group).lookup_action("delete").enabled? }
      main_row.activate_action("profile-row.delete", nil)
    end
    d.check("one profile left on the device") { app.window.current_device.profiles.map(&:name) == ["Streaming"] }
    d.check("the sidebar list shrank to match — not left stale") do
      !app.window.send(:profiles_listbox).get_row_at_index(0).nil? &&
        app.window.send(:profiles_listbox).get_row_at_index(1).nil?
    end
    d.check("Streaming is still active (it was never the deleted one)") do
      app.window.current_device.active_profile.name == "Streaming"
    end
  end

  d.step("deleting the last profile falls back to a replacement, never to none") do
    app.window.send(:profiles_listbox).get_row_at_index(0).then do |row|
      d.check("delete is disabled with only one profile left") { !row.send(:action_group).lookup_action("delete").enabled? }
    end
  end

  d.step("a key is selected in the grid, which swaps the sidebar to its editor") do
    app.window.send(:devices_stack).visible_child.then do |editor|
      editor.send(:regions_grid).first_child.then do |grid_widget|
        grid_widget.send(:flowbox).first_child.then do |first_key|
          d.check("the empty editor shows before anything is picked") do
            editor.send(:editor_bin).child.equal?(editor.send(:empty_toolbar_view))
          end
          grid_widget.send(:flowbox).select_child(first_key)
          d.check("the selection controller picked it up") do
            editor.instance_variable_get(:@selection_controller).selected?
          end
          d.check("the sidebar swapped to a ButtonEditor") do
            editor.send(:editor_bin).child.is_a?(Boatswain::ButtonEditor)
          end
          d.check("it is editing the key that was clicked") do
            editor.send(:editor_bin).child.button.equal?(first_key.button)
          end
          d.shot("04-button-selected")
        end
      end
    end
  end

  d.step("the icon widget mirrors the button's icon and updates live") do
    current_editor(app).then do |editor|
      button_editor = editor.send(:editor_bin).child
      d.check("the icon widget shows the button's own icon") do
        button_editor.send(:icon_widget).icon.equal?(button_editor.button.icon)
      end
      button_editor.send(:custom_icon_text_row).text = "Hi"
      d.check("typing a caption gave the button a custom icon") do
        button_editor.button.custom_icon && button_editor.button.custom_icon.text == "Hi"
      end
      d.check("the remove-custom-icon button appeared") { button_editor.send(:remove_custom_icon_button).visible? }
      d.check("the icon widget followed the change") { button_editor.send(:icon_widget).icon.text == "Hi" }
      button_editor.send(:background_color_button).rgba = Gdk::RGBA.parse("#ff0000")
      d.check("the colour landed on the custom icon") do
        button_editor.button.custom_icon.background_color.red > 0.9
      end
      button_editor.send(:remove_custom_icon_button).signal_emit("clicked")
      d.check("removing it clears the custom icon") { button_editor.button.custom_icon.nil? }
      d.shot("05-custom-icon")
    end
  end

  d.step("Select Action opens the real ActionSelector page in the navigation view") do
    current_editor(app).then do |editor|
      button_editor = editor.send(:editor_bin).child
      d.check("it starts on the button-properties page") do
        button_editor.send(:navigation_view).visible_page.equal?(button_editor.send(:button_page))
      end
      button_editor.send(:select_action_row).signal_emit("activated")
      d.check("the actions page is showing now") do
        button_editor.send(:navigation_view).visible_page.equal?(button_editor.send(:actions_page))
      end
      d.check("all four plugin factories built an expander row") do
        %w[Stream\ Deck Launcher Network Gaming].all? do |title|
          find_widget(button_editor.send(:action_selector)) { |w| w.is_a?(Adwaita::ExpanderRow) && w.title == title }
        end
      end
      d.check("the hidden Go Up action is not listed") do
        find_widget(button_editor.send(:action_selector)) { |w| w.is_a?(Adwaita::ActionRow) && w.title == "Go Up" }.nil?
      end
      d.shot("06-action-selector")
    end
  end

  d.step("picking Folder assigns a SwitchPageAction and pops back") do
    current_editor(app).then do |editor|
      button_editor = editor.send(:editor_bin).child
      find_widget(button_editor.send(:action_selector)) do |w|
        w.is_a?(Adwaita::ActionRow) && w.title == "Folder"
      end.then do |folder_row|
        d.check("the row was found") { !folder_row.nil? }
        folder_row.signal_emit("activated")
      end
      d.check("the button's action is the Folder action") do
        button_editor.button.action.is_a?(Boatswain::Plugins::Default::SwitchPageAction)
      end
      d.check("its icon followed") { button_editor.button.icon.icon_name == "folder-symbolic" }
      d.check("the view popped back to the button page") do
        button_editor.send(:navigation_view).visible_page.equal?(button_editor.send(:button_page))
      end
      d.check("the remove-action row is visible now there is an action") { button_editor.send(:remove_group).visible? }
      d.shot("07-folder-action")
    end
  end

  d.step("double-clicking the key in the grid runs its action (pushes the subpage)") do
    current_editor(app).then do |editor|
      editor.send(:regions_grid).first_child.send(:flowbox).first_child.then do |key|
        key.signal_emit("activate")
      end
    end
    d.check("the device pushed a page") { app.window.current_device.active_page.root? == false }
  end

  d.step("the first key of the new subpage is the fixed Go Up key") do
    current_editor(app).then do |editor|
      editor.send(:regions_grid).first_child.send(:flowbox).first_child.then do |key|
        d.check("its action is Go Up") { key.button.action.is_a?(Boatswain::Plugins::Default::PageUpAction) }
        # button_widget.rb's fixed_first_slot? checks for upstream's own
        # id, "default-switch-page-action" (is_move_page_up_action), not
        # "default-page-up-action" that PageUpAction actually carries — a
        # faithfully-reproduced upstream quirk (see that method's comment),
        # so the real Go Up key is, upstream included, draggable here.
        d.check("upstream's own id mismatch leaves it draggable, quirk included") do
          key.send(:drag_source).propagation_phase == :bubble
        end
      end
    end
    key = current_editor(app).send(:regions_grid).first_child.send(:flowbox).first_child
    key.signal_emit("activate")
    d.check("activating Go Up pops back to the root page") { app.window.current_device.active_page.root? }
  end

  d.step("Remove Action clears a key back to empty") do
    current_editor(app).then do |editor|
      button_editor = editor.send(:editor_bin).child
      button_editor.send(:remove_action_row).signal_emit("activated")
      d.check("the action is empty again") { button_editor.button.action.is_a?(Boatswain::EmptyAction) }
      d.check("the remove-action row hid itself") { !button_editor.send(:remove_group).visible? }
    end
  end

  d.step("a key's drag source offers itself, and the drop target accepts the same shape") do
    current_editor(app).then do |editor|
      editor.send(:regions_grid).first_child.send(:flowbox).first_child.then do |key|
        key.send(:drag_source).signal_emit("prepare", 2.0, 2.0).then do |provider|
          d.check("prepare answers a content provider") { provider.is_a?(Gdk::ContentProvider) }
        end
        d.check("the drop target is armed for the same key") { key.send(:drop_target).is_a?(Gtk::DropTarget) }
        d.check("drag moves rather than copies") { key.send(:drag_source).actions == :move }
      end
    end
  end

  d.step("dragging one key onto another swaps their actions") do
    grid = current_editor(app).send(:regions_grid).first_child.send(:flowbox)
    first_key = grid.first_child
    second_key = first_key.next_sibling

    first_key.button.action = Boatswain::Plugins::Default::SwitchPageAction.new(app.window.instance_variable_get(:@context))

    d.check("the drop is accepted") do
      second_key.send(:on_drop, "#{first_key.button.region.id}\t#{first_key.button.position}")
    end
    # Port of on_drop_target_drop_cb: only the dropped-on button's action
    # is set to the dragged one; the dragged button's own action is left
    # exactly as it was (only custom icons move both ways) — upstream's
    # own swap is one-directional for actions, faithfully reproduced here.
    d.check("the first (dragged) key keeps its own action, untouched by the drop") do
      first_key.button.action.is_a?(Boatswain::Plugins::Default::SwitchPageAction)
    end
    d.check("the second key now holds the Folder action that was dragged") do
      second_key.button.action.is_a?(Boatswain::Plugins::Default::SwitchPageAction)
    end
    d.check("a drop of the same key on itself is refused") { !second_key.send(:on_drop, "#{second_key.button.region.id}\t#{second_key.button.position}") }
  end

  d.step("the brightness action's own preferences build and drive the device") do
    Boatswain::Plugins::Default::BrightnessAction.new.then do |action|
      d.check("it defaults to Constant / 0.01") { action.mode == :set_value && action.value == 0.01 }
      prefs = action.preferences
      d.check("the preferences are a boxed ListBox") { prefs.is_a?(Gtk::ListBox) }
      action.mode = :increase
      d.check("increase mode took the sunrise icon") { action.icon.icon_name == "daytime-sunrise-symbolic" }
      action.value = 0.2
      app.window.current_device.brightness = 0.5
      action.handle_event(Boatswain::ButtonEvent.new(:button_press, app.window.current_device, nil))
      d.check("pressing it raised the device's brightness by the step") { app.window.current_device.brightness == 0.7 }
      d.check("settings round-trip") do
        reloaded = Boatswain::Plugins::Default::BrightnessAction.new
        reloaded.deserialize_settings(action.serialize_settings)
        reloaded.mode == :increase && reloaded.value == 0.2
      end
    end
  end

  d.step("the switch-profile action's combo rows list the real devices and profiles") do
    Boatswain::Plugins::Default::SwitchProfileAction.new(app.window.instance_variable_get(:@context)).then do |action|
      prefs = action.preferences
      d.check("the preferences are a PreferencesGroup") { prefs.is_a?(Adwaita::PreferencesGroup) }
      devices_row = find_widget(prefs) { |w| w.is_a?(Adwaita::ComboRow) && w.title == "Stream Deck" }
      profiles_row = find_widget(prefs) { |w| w.is_a?(Adwaita::ComboRow) && w.title == "Profile" }
      d.check("the device row is populated") { devices_row.model.n_items == 2 }
      # devices_row already defaults to index 0 (no serial number means
      # device_position falls back to 0), so setting it to 0 again is not a
      # change and GTK never re-fires "notify::selected" for a no-op set —
      # picking index 1 is a real change and genuinely exercises the reload.
      devices_row.selected = 1
      d.check("picking a device re-lists its profiles") do
        profiles_row.model.n_items == app.window.instance_variable_get(:@context).devices[1].profiles.n_items
      end
      action.handle_event(Boatswain::ButtonEvent.new(:button_press, app.window.current_device, nil))
      d.check("pressing it loaded the chosen device's chosen profile") do
        app.window.instance_variable_get(:@context).devices[1].active_profile.equal?(app.window.instance_variable_get(:@context).devices[1].profiles[0])
      end
    end
  end

  d.step("the multi-action runs its entries with the delays between them honoured") do
    Boatswain::Plugins::Default::MultiAction.new(app.window.instance_variable_get(:@context)).then do |multi|
      ran = []
      multi.add_action(Class.new(Boatswain::Action) { define_method(:handle_event) { |_e| } }.new)
      multi.add_delay(50)
      multi.add_action(Class.new(Boatswain::Action) do
        define_method(:handle_event) { |_e| }
      end.new)
      multi.entries.each { |entry| ran << entry.entry_type }
      d.check("the entries recorded in order") { ran == [:action, :delay, :action] }
      d.check("its preferences memoize one MultiActionEditor") { multi.preferences.equal?(multi.preferences) }
    end
  end

  d.step("the launcher plugin's three actions build their preferences without crashing") do
    d.check("Launch Application's preferences group builds") do
      Boatswain::Plugins::Launcher::LaunchAction.new.preferences.is_a?(Adwaita::PreferencesGroup)
    end
    d.check("Open File's preferences group builds") do
      Boatswain::Plugins::Launcher::OpenFileAction.new.preferences.is_a?(Adwaita::PreferencesGroup)
    end
    Boatswain::Plugins::Launcher::OpenUrlAction.new.then do |action|
      prefs = action.preferences
      d.check("Open URL's preferences group builds") { prefs.is_a?(Adwaita::PreferencesGroup) }
      find_widget(prefs) { |w| w.is_a?(Adwaita::EntryRow) }.then do |row|
        row.text = "https://example.com"
        d.check("typing the URL reaches the action") { action.url == "https://example.com" }
      end
    end
  end

  d.step("the network plugin's HTTP action builds its preferences") do
    Boatswain::Plugins::Network::ActionFactory.new(app.window.instance_variable_get(:@context)).then do |factory|
      factory.create_action(factory.info("network-http-action")).then do |action|
        d.check("it is an HttpAction") { action.is_a?(Boatswain::Plugins::Network::HttpAction) }
        d.check("its preferences build") { !action.preferences.nil? }
        action.uri = " https://example.com/hook "
        d.check("the URI is stripped") { action.uri == "https://example.com/hook" }
      end
    end
  end

  d.step("the gaming plugin's score action counts up, down and resets") do
    Boatswain::Plugins::Gaming::ScoreAction.new.then do |action|
      d.check("it starts at 0") { action.score == 0 }
      device = app.window.current_device
      action.handle_event(Boatswain::TouchscreenEvent.new(:touchscreen_short_press, device, nil))
      d.check("a short touch increments") { action.score == 1 }
      action.handle_event(Boatswain::TouchscreenEvent.new(:touchscreen_long_press, device, nil))
      d.check("a long touch decrements") { action.score == 0 }
      action.handle_event(Boatswain::DialEvent.new(:dial_rotate, device, nil, 3))
      d.check("rotating the dial applies the rotation") { action.score == 3 }
      d.check("its caption follows the score") { action.icon.text == "3" }
      d.check("its preferences build") { action.preferences.is_a?(Gtk::Box) }
    end
  end

  d.step("Icon's model layer: fallback through a relative icon, and emptiness") do
    base = Boatswain::Icon.new_empty
    base.icon_name = "folder-symbolic"
    overlay = Boatswain::Icon.new_empty
    overlay.relative = base
    d.check("an icon with nothing of its own takes its relative's name") { overlay.effective_icon_name == "folder-symbolic" }
    d.check("a fresh icon reports itself empty") { Boatswain::Icon.new_empty.empty? }
    d.check("one with a caption does not") do
      Boatswain::Icon.new_empty.tap { |icon| icon.text = "x" }.empty? == false
    end
    d.check("to_json_data round-trips background colour") do
      Boatswain::Icon.from_json(Boatswain::Icon.with_background(Gdk::RGBA.parse("#112233")).to_json_data)
        .background_color.to_s == Gdk::RGBA.parse("#112233").to_s
    end
  end

  d.step("the gudev USB detector and the ashpd background portal are genuinely exercised") do
    detector = Boatswain::Devices::Usb::HardwareDetector.new
    found = detector.detect
    d.check("HardwareDetector#detect runs a real GUdev enumeration without raising") { found.is_a?(Array) }
    d.check("its client is the real gudev gem's GUdev::Client") { detector.send(:client).is_a?(GUdev::Client) }
    d.check("its enumerator is the real gudev gem's GUdev::Enumerator") do
      detector.send(:enumerator).is_a?(GUdev::Enumerator)
    end

    captured = StringIO.new
    original_stderr = $stderr
    $stderr = captured
    begin
      app.send(:request_background)
    ensure
      $stderr = original_stderr
    end
    d.check("request_background called the real Ashpd::Background API") do
      captured.string.include?("Cannot hold Boatswain in the background")
    end
    d.check("it hit the documented, currently-missing portal, naming the real gem's version") do
      captured.string.include?("ashpd #{Ashpd::VERSION} has no Background portal")
    end
  end

  d.step("the application's actions and accelerators are installed on app, not window") do
    d.check("app.quit exists and is enabled") do
      action = app.app.lookup_action("quit")
      action.is_a?(Gio::SimpleAction) && action.enabled?
    end
    # GTK normalises the "<primary>" alias it was set with to the platform's
    # actual modifier ("<Control>" on Linux) the moment it is read back.
    d.check("its accel is Ctrl+Q") { app.app.get_accels_for_action("app.quit") == ["<Control>q"] }
    d.check("app.shortcuts exists") { app.app.lookup_action("shortcuts").is_a?(Gio::SimpleAction) }
    d.check("its accel is Ctrl+?") { app.app.get_accels_for_action("app.shortcuts") == ["<Control>question"] }
  end

  # Adwaita::ShortcutsDialog/AboutDialog are Adw.Dialog, presented inside the
  # window's own surface (libadwaita's modern dialog model), not as a
  # separate GtkWindow toplevel — so the window's own "visible-dialog" is
  # where it shows up, the same place the ruby-gtk-testing skill's "dialogs
  # stack, close as you go" note points at, not Gtk::Window.list_toplevels.
  d.step("app.shortcuts opens the real ShortcutsDialog with upstream's two items") do
    app.app.activate_action("shortcuts", nil)
    pump(300)
    d.check("a dialog appeared") { !app.window.visible_dialog.nil? }
    app.window.visible_dialog.then do |dialog|
      d.check("its title is Keyboard Shortcuts") { dialog.title == "Keyboard Shortcuts" }
      d.check("it lists Show Shortcuts") { !find_widget(dialog) { |w| w.is_a?(Gtk::Label) && w.label == "Show Shortcuts" }.nil? }
      d.check("and Quit") { !find_widget(dialog) { |w| w.is_a?(Gtk::Label) && w.label == "Quit" }.nil? }
      d.shot("08-shortcuts", dialog)
      dialog.close
    end
    pump(300)
  end

  d.step("win.about opens the real AboutDialog with upstream's credits") do
    # Gtk::Widget#activate_action("win.about", nil) silently does nothing in
    # this binding (confirmed in isolation: the "about" action itself is
    # real, enabled and works — Gio::ActionGroup#activate_action on the
    # window's own action group runs it correctly); go through the action
    # group directly instead, the same action a real menu click resolves to.
    app.window.send(:action_group).activate_action("about", nil)
    pump(300)
    d.check("a dialog appeared") { !app.window.visible_dialog.nil? }
    app.window.visible_dialog.then do |dialog|
      d.check("its application name is Boatswain") { dialog.application_name == "Boatswain" }
      d.check("its version is the port's own") { dialog.version == Boatswain::Rb::VERSION }
      d.check("its copyright is upstream's") { dialog.copyright == Boatswain::AboutDialog::COPYRIGHT }
      d.check("its website is upstream's") { dialog.website == Boatswain::AboutDialog::WEBSITE }
      d.check("its issue tracker is upstream's") { dialog.issue_url == Boatswain::AboutDialog::ISSUE_URL }
      d.check("licence is GPL-3.0") { dialog.license_type == :gpl_3_0 }
      d.shot("09-about", dialog)
      dialog.close
    end
    pump(300)
  end

  d.step("the primary menu carries the three items upstream's menu does") do
    app.window.send(:primary_menu).then do |menu|
      d.check("it is a Gio::Menu") { menu.is_a?(Gio::Menu) }
      d.check("it has one section") { menu.n_items == 1 }
    end
  end

  d.step("<primary>w is bound to window.close on the window itself") do
    d.check("the window has a shortcut controller bound to Ctrl+W") do
      shortcut = app.window.send(:close_shortcut)
      trigger = shortcut.trigger
      _success, keyval, mods = Gtk.accelerator_parse("<primary>w")
      trigger.is_a?(Gtk::KeyvalTrigger) && trigger.keyval == keyval && trigger.modifiers == mods
    end
  end

  d.step("Style's CSS parses cleanly, with no errors swallowed") do
    errors = []
    Gtk::CssProvider.new.tap do |provider|
      provider.signal_connect("parsing-error") { |_p, _section, error| errors << error.message }
      provider.load_from_string(Boatswain::Style::CSS)
    end
    d.check("no parsing errors") { errors.empty? }
  end

  d.step("Settings persists the one GSettings key as a JSON file") do
    Dir.mktmpdir("boatswain-settings").then do |dir|
      path = File.join(dir, "settings.json")
      Boatswain::Settings.new(path: path).then do |settings|
        d.check("the default restore token is empty") { settings.desktop_controller_restore_token == "" }
        settings.desktop_controller_restore_token = "abc123"
        d.check("it wrote to disk") { File.exist?(path) }
        Boatswain::Settings.new(path: path).then do |reloaded|
          d.check("a fresh instance reads it back") { reloaded.desktop_controller_restore_token == "abc123" }
        end
      end
    end
  end

  d.step("a device's profiles survive a save and a fresh read from disk") do
    device = app.window.current_device
    device.save_profiles
    JSON.parse(File.read(profile_path_for(device))).then do |saved|
      d.check("the active profile id was saved") { saved["active-profile"] == device.active_profile.id }
      d.check("every profile the device holds is in the file") do
        saved["profiles"].map { |p| p["name"] } == device.profiles.map(&:name)
      end
      d.check("a realized action serialised onto its page") do
        saved["profiles"].first["page"]["regions"].any? { |region| region["items"].any? { |item| item["type"] } }
      end
    end
  end

  d.step("a dial grid and a touchscreen region build and behave in a device editor") do
    device = app.window.current_device
    selection_controller = Boatswain::SelectionController.new

    dial_grid = Boatswain::DialGrid.new(
      id:           "test-dials",
      device:       device,
      n_dials:      3,
      grid_columns: 3,
      column:       0,
      row:          1,
    )
    touchscreen = Boatswain::Touchscreen.new(
      id:         "test-touchscreen",
      device:     device,
      image_info: {width: 800, height: 100},
      n_slots:    4,
      column:     0,
      row:        2,
    )
    device.regions.append(dial_grid)
    device.regions.append(touchscreen)

    editor = Boatswain::DeviceEditor.new(app.window.instance_variable_get(:@context), device)
    region_widgets = editor.instance_variable_get(:@region_widgets)

    dial_flowbox = region_widgets[dial_grid]
    touchscreen_widget = region_widgets[touchscreen]

    d.check("the dial grid became a 3-column flow box of DialWidgets") do
      dial_flowbox.is_a?(Gtk::FlowBox) &&
        dial_flowbox.min_children_per_line == 3 &&
        (0...3).all? { |i| dial_flowbox.get_child_at_index(i).child.is_a?(Boatswain::DialWidget) }
    end
    d.check("each dial widget carries its own Dial, in position order") do
      (0...3).all? do |i|
        dial_flowbox.get_child_at_index(i).child.dial.equal?(dial_grid.dials[i])
      end
    end
    d.check("dial flow box children cannot be targeted (not selectable, matching upstream)") do
      (0...3).all? { |i| !dial_flowbox.get_child_at_index(i).can_target? }
    end

    d.check("the touchscreen became a TouchscreenWidget with 4 slot widgets") do
      touchscreen_widget.is_a?(Boatswain::TouchscreenWidget) &&
        touchscreen.slots.length == 4
    end
    touchscreen_flowbox = touchscreen_widget.send(:flowbox)
    d.check("the slots are TouchscreenSlotWidgets wrapping the real slots in order") do
      (0...4).all? do |i|
        touchscreen_flowbox.get_child_at_index(i).slot.equal?(touchscreen.slots[i])
      end
    end

    first_slot_widget = touchscreen_flowbox.get_child_at_index(0)
    touchscreen_flowbox.select_child(first_slot_widget)
    d.check("tapping a slot drives the shared selection controller, like a button does") do
      selection = editor.instance_variable_get(:@selection_controller).selection
      !selection.nil? && selection[0].equal?(touchscreen) && selection[1].equal?(first_slot_widget.slot)
    end

    dial = dial_grid.dials[0]
    pressed_events = []
    dial.on_pressed_changed { |d2| pressed_events << d2.pressed? }
    dial.pressed = true
    dial.pressed = true
    dial.pressed = false
    d.check("a dial's pressed state only notifies on an actual change") { pressed_events == [true, false] }

    action = Boatswain::EmptyAction.new
    slot_icon_updates = []
    touchscreen.slots[1].on_content_changed { slot_icon_updates << touchscreen.slots[1] }
    touchscreen.slots[1].action = action
    d.check("assigning a slot's action notifies its widget through content-changed") do
      slot_icon_updates == [touchscreen.slots[1]]
    end
  end

  d.step("an unreadable profile file is refused, not crashed on") do
    Dir.mktmpdir("boatswain-bad-profile").then do |dir|
      original_data_home = ENV["XDG_DATA_HOME"]
      ENV["XDG_DATA_HOME"] = dir
      context = Boatswain::Context.new
      device = Boatswain::Devices::Mock::Device.new(context, model: :hangar_mild)
      FileUtils.mkdir_p(File.join(dir, "boatswain"))
      File.write(profile_path_for(device), "not json at all")

      raised = false
      begin
        device.load
      rescue StandardError
        raised = true
      end

      d.check("reading garbage JSON warns and falls back, rather than raising") { !raised }
      d.check("it still has a Default profile to fall back to") { device.profiles.map(&:name) == ["Default"] }
    ensure
      ENV["XDG_DATA_HOME"] = original_data_home
    end
  end
end
