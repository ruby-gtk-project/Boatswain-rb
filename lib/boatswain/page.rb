# frozen_string_literal: true

require_relative "empty_action"
require_relative "page_item"

module Boatswain
  # Page is the port of lib/bs-page.c: one screenful of a profile — the items
  # of every region of the device, plus each region's own saved data.
  #
  # Upstream keeps a hash of region id to PageRegion, each holding the region's
  # JSON blob and an array of ItemData (the saved item, and once loaded, the
  # live action and custom icon). The same shape is kept here. Items are loaded
  # lazily: a page only builds its actions when the device pushes it.
  class Page
    MAIN_BUTTON_GRID = "main-button-grid"
    VERSION = 1

    # Port of bs_page_new_empty: a subpage, so it gets the Go Up item.
    def self.new_empty(context)
      new(context).tap(&:ensure_first_subpage_item_is_move_up)
    end

    # Port of bs_page_new_from_json.
    def self.from_json(context, data)
      new(context).tap do |page|
        page.load_from_json(data)
        page.ensure_first_subpage_item_is_move_up
      end
    end

    # Port of bs_page_new_root: the profile's own page, which has no Go Up.
    def self.new_root(context, data = nil)
      new(context, root: true).tap do |page|
        if data
          page.load_from_json(data)
        end
      end
    end

    def initialize(context, root: false)
      @context = context
      @root = root
      @regions = {}
      @loaded = false
    end

    attr_reader :context

    def root? = @root

    def loaded? = @loaded

    # Port of load_page_from_json. Upstream asserts version 1 and a "regions"
    # array; a page written by an older version has no regions array at all,
    # and convert_page_v0_to_v1 wraps its flat item list into the main button
    # grid — the same conversion is done here.
    def load_from_json(data)
      converted(data).then do |page_data|
        Array(page_data["regions"]).each do |region_data|
          region(region_data["id"]).then do |page_region|
            page_region[:data] = region_data["region-data"] || {}
            page_region[:items] = Array(region_data["items"]).map do |item_data|
              { item: PageItem.from_json(item_data) }
            end
          end
        end
      end
    end

    # Port of bs_page_to_json.
    def to_json_data
      { "version" => VERSION, "regions" => regions_json }
    end

    # Port of bs_page_get_region_data / bs_page_set_region_data.
    def region_data(region_id) = region(region_id)[:data]

    def set_region_data(region_id, data)
      region(region_id)[:data] = data
    end

    # Port of bs_page_realize: answers [custom_icon, action] for one slot.
    # A slot that has never been filled answers an EmptyAction, exactly as
    # upstream's early `*out_action = bs_empty_action_new ()` does.
    def realize(region_id, position)
      item_data(region_id, position).then do |data|
        if data.nil?
          [nil, EmptyAction.new]
        elsif @loaded
          [data[:custom_icon], data[:action]]
        else
          data[:item].realize(@context)
        end
      end
    end

    # Port of bs_page_update_item.
    def update_item(region_id, position, action, custom_icon)
      item_data(region_id, position).then do |existing|
        unless existing
          add_item(PageItem.new, region_id, position)
        end

        item_data(region_id, position).then do |data|
          data[:item].update(action, custom_icon)
          data[:action] = action
          data[:custom_icon] = custom_icon
        end
      end
    end

    # Port of bs_page_load_items.
    def load_items
      unless @loaded
        each_item_data do |region_id, data|
          realize_item_data(region_id, data)
        end
        @loaded = true
      end
    end

    # Port of bs_page_unload_items.
    def unload_items
      if @loaded
        each_item_data do |_region_id, data|
          data[:item].update(data[:action], data[:custom_icon])
          data[:action] = nil
          data[:custom_icon] = nil
        end
        @loaded = false
      end
    end

    # Port of bs_page_update_items.
    def update_items
      each_item_data do |_region_id, data|
        data[:item].update(data[:action], data[:custom_icon])
      end
    end

    # Port of ensure_first_subpage_item_is_move_up: every page but the root one
    # has Go Up in its first slot, so there is always a way back.
    def ensure_first_subpage_item_is_move_up
      unless @root
        item_at(MAIN_BUTTON_GRID, 0).then do |item|
          if item.nil?
            add_item(page_up_item, MAIN_BUTTON_GRID, 0)
          elsif !page_up?(item)
            make_page_up(item)
          end
        end
      end
    end

    private

      def page_up?(item)
        item.item_type == PageItem::ACTION &&
          item.factory == "default" &&
          item.action == "default-page-up-action"
      end

      def page_up_item
        PageItem.new.tap { |item| make_page_up(item) }
      end

      def make_page_up(item)
        item.tap do |saved|
          saved.item_type = PageItem::ACTION
          saved.factory = "default"
          saved.action = "default-page-up-action"
          saved.settings = nil
        end
      end

      # Port of convert_page / convert_page_v0_to_v1.
      def converted(data)
        if data.is_a?(Array)
          { "version" => VERSION, "regions" => [v0_region(data)] }
        elsif data.is_a?(Hash) && data["version"].to_i >= VERSION
          data
        else
          { "version" => VERSION, "regions" => [] }
        end
      end

      def v0_region(items)
        { "id" => MAIN_BUTTON_GRID, "region-data" => {}, "items" => items }
      end

      def regions_json
        @regions.map do |region_id, page_region|
          {
            "id"          => region_id,
            "region-data" => page_region[:data],
            "items"       => page_region[:items].map { |data| data[:item].to_json_data },
          }
        end
      end

      # Port of ensure_page_region.
      def region(region_id)
        @regions[region_id] ||= { data: {}, items: [] }
      end

      def item_data(region_id, position)
        @regions[region_id].then do |page_region|
          if page_region && position < page_region[:items].length
            page_region[:items][position]
          end
        end
      end

      def item_at(region_id, position) = item_data(region_id, position)&.fetch(:item)

      # Port of add_item. The item list is positional — slot n of a region is
      # element n — so a slot filled before the ones in front of it needs
      # those to exist first; upstream's GPtrArray insert has the same
      # requirement.
      def add_item(item, region_id, position)
        region(region_id)[:items].then do |items|
          while items.length < position
            items << { item: PageItem.new }
          end
          items.insert(position, { item: item })
        end
      end

      def each_item_data
        @regions.each do |region_id, page_region|
          page_region[:items].each { |data| yield(region_id, data) }
        end
      end

      def realize_item_data(region_id, data)
        data[:custom_icon], data[:action] = data[:item].realize(@context)
      rescue PageItem::UnknownFactoryError => e
        warn "Error creating action for region '#{region_id}': #{e.message}"
        data[:custom_icon] = nil
        data[:action] = EmptyAction.new
      end
  end
end
