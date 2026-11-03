# frozen_string_literal: true

require "adwaita"

module Boatswain
  # ShortcutsDialog is the port of src/shortcuts-dialog.ui: one "General"
  # section with the two shortcuts upstream lists.
  #
  # Upstream has a loose end here. The primary menu and this dialog both
  # reference app.shortcuts, but no source file installs that action or loads
  # the dialog, so upstream's menu item does nothing. Application installs the
  # action and presents this dialog; the gap is filled rather than inherited.
  class ShortcutsDialog < Adwaita::ShortcutsDialog
    SECTIONS = {
      "General" => [
        ["Show Shortcuts", "app.shortcuts"],
        ["Quit", "app.quit"],
      ],
    }.freeze

    def initialize
      super()
      self.title = "Keyboard Shortcuts"
      build
    end

    def build
      tap do |dialog|
        SECTIONS.each { |title, items| dialog.add(section(title, items)) }
      end
    end

    private

      def section(title, items)
        Adwaita::ShortcutsSection.new.tap do |section|
          section.title = title
          items.each { |label, action_name| section.add(item(label, action_name)) }
        end
      end

      # Positional, not keyword, arguments: AdwShortcutsItem's Ruby
      # constructor takes the title and then the accelerator, and an item that
      # names an action instead of an accelerator sets action-name afterwards.
      def item(label, action_name)
        Adwaita::ShortcutsItem.new(label, "").tap do |shortcut|
          shortcut.action_name = action_name
        end
      end
  end
end
