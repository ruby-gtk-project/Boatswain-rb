/*
 * gaming.js
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

import Bs from 'gi://Bs';
import GObject from 'gi://GObject';

import { GamingScoreAction } from './score.js';

export const GamingActionFactory = GObject.registerClass({
    GTypeName: 'GamingActionFactory',
}, class GamingActionFactory extends Bs.ActionFactory {
    constructor(params={}) {
        super(params);

        const entries = [
            {
                id: 'gaming-score-action',
                icon_name: 'score-symbolic',
                name: _('Score'),
                description: _('Keep track of your score. Reset with a long press.'),
            },
        ];

        for (const entry of entries)
            this.add_action(new Bs.ActionInfo(entry));
    }

    vfunc_create_action(info) {
        switch (info.id) {
        case 'gaming-score-action':
            return new GamingScoreAction();
        default:
            return null;
        }
    }
});
