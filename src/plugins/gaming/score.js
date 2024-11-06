/*
 * score.js
 *
 * Copyright 2023 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import Adw from 'gi://Adw';
import Bs from 'gi://Bs';
import Gdk from 'gi://Gdk?version=4.0';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Graphene from 'gi://Graphene';
import Gtk from 'gi://Gtk?version=4.0';
import Json from 'gi://Json';
import Pango from 'gi://Pango';
import PangoCairo from 'gi://PangoCairo';

Gio._promisify(Gtk.FileDialog.prototype, 'save');

function getDefaultFile() {
    const path = GLib.build_filenamev([
        GLib.get_user_special_dir(GLib.UserDirectory.DIRECTORY_DOCUMENTS),
        _('score.txt'),
    ]);
    return Gio.File.new_for_path(path);
}

const GamingScorePaintable = GObject.registerClass({
    GTypeName: 'GamingScorePaintable',
    Implements: [Gdk.Paintable],
    Properties: {
        'color': GObject.ParamSpec.boxed(
            'color', null, null,
            GObject.ParamFlags.READWRITE,
            Gdk.RGBA),
        'score': GObject.ParamSpec.int64(
            'score', null, null,
            GObject.ParamFlags.READWRITE,
            0, GLib.MAXINT64_BIGINT, 0),
    }
}, class GamingScorePaintable extends GObject.Object {
    constructor(params={}) {
        super({
            color: new Gdk.RGBA({red: 1.0, green: 1.0, blue: 1.0, alpha: 1.0}),
            ...params,
        });

        this._fontDescription = Pango.FontDescription.from_string("Cantarell 22");

        const pangoContext = PangoCairo.FontMap.get_default().create_context();
        pangoContext.set_language(Gtk.get_default_language());
        pangoContext.set_font_description(this._fontDescription);

        this._layout = Pango.Layout.new(pangoContext);
        this._layout.set_text(`${this.score}`, -1);

        this.connect('notify', () => this._update());
    }

    _update() {
        console.assert(this._layout !== null);
        this._layout.set_text(`${this.score}`, -1);
        this.invalidate_contents();
    }

    vfunc_get_intrinsic_width() {
        return 72;
    }

    vfunc_get_intrinsic_height() {
        return 72;
    }

    vfunc_snapshot(snapshot, width, height) {
        console.assert(this._layout !== null);

        this._layout.set_width(width);

        const [textWidth, textHeight] = this._layout.get_pixel_size();
        const textSize = Math.max(textWidth, textHeight);

        snapshot.save();

        snapshot.translate(new Graphene.Point({
            x: (width - textWidth) / 2.0,
            y: (height - textHeight) / 2.0,
        }));
        snapshot.append_layout(this._layout, this.color);

        snapshot.restore();
    }
});

const ScoreAction = {
    INCREMENT: 0,
    DECREMENT: 1,
    RESET: 2,
};

const TimeoutState = {
    DISABLED: 0,
    SHORT: 1,
    LONG: 2,
    VERY_LONG: 3,
};

export const GamingScoreAction = GObject.registerClass({
    GTypeName: 'GamingScoreAction',
    Properties: {
        'file': GObject.ParamSpec.object(
            'file', null, null,
            GObject.ParamFlags.READWRITE,
            Gio.File),
        'restore-score': GObject.ParamSpec.boolean(
            'restore-score', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'save-to-file': GObject.ParamSpec.boolean(
            'save-to-file', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'score': GObject.ParamSpec.int64(
            'score', null, null,
            GObject.ParamFlags.READWRITE,
            0, GLib.MAXINT64_BIGINT, 0),
    }
}, class GamingScoreAction extends Bs.Action {
    constructor(streamDeckButton) {
        super({button: streamDeckButton});

        this._paintable = new GamingScorePaintable();
        this.get_icon().paintable = this._paintable;

        this.bind_property('score',
            this._paintable, 'score',
            GObject.BindingFlags.SYNC_CREATE);

        this.connect('notify', () => this.changed());

        this._setState(TimeoutState.DISABLED);
    }

    _saveToFile() {
        if (this.saveToFile && this.file) {
            try {
                GLib.file_set_contents(this.file.get_path(), `${this.score}`);
            } catch(e) {
                console.log(e.stack);
            }
        }
    }

    _setState(state) {
        console.debug(`New state: ${state}`);
        this._state = state;
    }

    _applyAction(action) {
        switch (action) {
        case ScoreAction.INCREMENT:
            console.debug('Incrementing');
            this.score++;
            break;

        case ScoreAction.DECREMENT:
            console.debug('Decrementing');
            this.score--;
            break;

        case ScoreAction.RESET:
            console.debug('Resetting');
            this.score = 0;
            break;

        default:
            console.assert(false, 'Unreachable code reached');
            return;
        }

        this._saveToFile();
    }

    _queueStateChange(nextState, timeout, callback) {
        if (this._timeoutId) {
            GLib.source_remove(this._timeoutId);
            delete this._timeoutId;
        }

        this._timeoutId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            timeout,
            () => {
                delete this._timeoutId;

                this._setState(nextState);
                callback();

                return GLib.SOURCE_REMOVE;
            });
    }

    vfunc_handle_event(event) {
        switch (event.get_event_type()) {
        case Bs.EventType.BUTTON_PRESS:
            console.assert(this._timeoutId === undefined);
            console.assert(this._state === TimeoutState.DISABLED);

            const timeout = Gtk.Settings.get_default().gtk_long_press_time;

            this._setState(TimeoutState.SHORT);

            this._queueStateChange(
                TimeoutState.LONG,
                timeout,
                () => {

                    this._applyAction(ScoreAction.DECREMENT);

                    this._queueStateChange(
                        TimeoutState.VERY_LONG,
                        timeout * 5,
                        () => {
                            this._applyAction(ScoreAction.RESET);
                            this._setState(TimeoutState.DISABLED);
                        }
                    );
                }
            );
            break;

        case Bs.EventType.BUTTON_RELEASE:
            switch (this._state) {
            case TimeoutState.SHORT:
                this._applyAction(ScoreAction.INCREMENT);
                break;

            case TimeoutState.LONG:
            case TimeoutState.VERY_LONG:
            case TimeoutState.DISABLED:
            default:
                break;
            }

            this._setState(TimeoutState.DISABLED);

            if (this._timeoutId) {
                GLib.source_remove(this._timeoutId);
                delete this._timeoutId;
            }
            break;

        case Bs.EventType.TOUCHSCREEN_SHORT_PRESS:
            this._applyAction(ScoreAction.INCREMENT);
            break;

        case Bs.EventType.TOUCHSCREEN_LONG_PRESS:
            this._applyAction(ScoreAction.DECREMENT);
            break;

        case Bs.EventType.TOUCHSCREEN_SWIPE:
            break;
    }

    }

    vfunc_serialize_settings() {
        const builder = new Json.Builder();

        builder.begin_object();

        builder.set_member_name('restore-score');
        builder.add_boolean_value(this.restoreScore);

        builder.set_member_name('score');
        builder.add_int_value(this.score);

        builder.set_member_name('text-color');
        builder.add_string_value(this._paintable.color.to_string());

        builder.set_member_name('save-to-file');
        builder.add_boolean_value(this.saveToFile);

        builder.set_member_name('file');
        if (this.file)
            builder.add_string_value(this.file.get_path());
        else
            builder.add_null_value();

        builder.end_object();

        return builder.get_root();
    }

    vfunc_deserialize_settings(settings) {
        this.freeze_notify();

        this.restoreScore = settings.get_boolean_member_with_default('restore-score', false);
        if (this.restoreScore)
            this.score = settings.get_int_member('score');

        const color = new Gdk.RGBA();
        color.parse(settings.get_string_member_with_default('text-color', '#FFF'));
        this._paintable.color = color;

        this.saveToFile = settings.get_boolean_member_with_default('save-to-file', false);
        if (settings.has_member('file') && !settings.get_null_member('file'))
            this.file = Gio.File.new_for_path(settings.get_string_member('file'));

        this.thaw_notify();
    }

    vfunc_get_preferences() {
        const box = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
            spacing: 18,
        });

        const mainGroup = new Adw.PreferencesGroup();
        box.append(mainGroup);

        /* Restore score */
        const restoreRow = new Adw.SwitchRow({title: _('Restore Score')});
        this.bind_property('restore-score',
            restoreRow, 'active',
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL);
        mainGroup.add(restoreRow);

        /* Text color */
        const colorDialog = new Gtk.ColorDialog({
           modal: true,
           with_alpha: false,
        });
        const colorButton = new Gtk.ColorDialogButton({
            valign: Gtk.Align.CENTER,
            dialog: colorDialog,
        });
        this._paintable.bind_property('color',
            colorButton, 'rgba',
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL);

        const colorRow = new Adw.ActionRow({title: _('Text Color')});
        colorRow.add_suffix(colorButton);
        colorRow.activatable_widget = colorButton;
        mainGroup.add(colorRow);


        /* ** Output File ** */

        const fileGroup = new Adw.PreferencesGroup();
        box.append(fileGroup);

        /* Save to file */
        const saveToFileRow = new Adw.SwitchRow({
            title: _('Save to File'),
            subtitle: _('Save score to a text file. Other apps such as OBS Studio may read from it.')
        });
        this.bind_property('save-to-file',
            saveToFileRow, 'active',
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL);
        fileGroup.add(saveToFileRow);

        /* Output file */
        const fileRow = new Adw.ActionRow({
            title: _('File'),
            activatable: true,
        });

        this.bind_property_full('file',
            fileRow, 'subtitle',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, source ? source.get_basename() : ''],
            null);

        this.bind_property_full('file',
            fileRow, 'tooltip-text',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, source ? source.get_path() : ''],
            null);

        fileRow.connect('activated', async () => {
            const filters = new Gio.ListStore({item_type: Gtk.FileFilter});
            filters.append(new Gtk.FileFilter({
                name: _('Text File (.txt)'),
                suffixes: ['txt'],
            }));

            const initialFile = this.file ? this.file : getDefaultFile();

            const dialog = new Gtk.FileDialog({
                modal: true,
                initialFile,
                filters,
            });

            try {
                this.file = await dialog.save(fileRow.get_root(), null);
                this._saveToFile();
            } catch(e) {
                console.logError(e);
            }
        });

        fileGroup.add(fileRow);

        return box;
    }
});
