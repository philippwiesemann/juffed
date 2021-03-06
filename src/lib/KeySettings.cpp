/*
JuffEd - An advanced text editor
Copyright 2007-2010 Mikhail Murzin

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License 
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "KeySettings.h"
#include "Settings.h"

QKeySequence KeySettings::keySequence(const QString& id) {
	if ( id.isEmpty() ) {
		return QKeySequence();
	}
	QString s = Settings::instance()->stringValue("keys", id);
	return QKeySequence(s);
}

void KeySettings::setKeySequence(const QString& id, const QKeySequence& seq) {
	if ( id.isEmpty() ) {
		return;
	}
	Settings::instance()->setValue("keys", id, seq.toString());
}

bool KeySettings::contains(const QString& id) {
	return Settings::instance()->keyList("keys").contains(id);
}

