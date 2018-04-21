/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2017 Soeren Apel <soeren@apelpie.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_GLOBALSETTINGS_HPP
#define PULSEVIEW_GLOBALSETTINGS_HPP

#include <map>

#include <glib.h>

#include <QSettings>
#include <QString>
#include <QVariant>

using std::map;
using std::vector;

namespace pv {

class GlobalSettingsInterface
{
public:
	virtual void on_setting_changed(const QString &key, const QVariant &value) = 0;
};


class GlobalSettings : public QSettings
{
	Q_OBJECT

public:
	static const QString Key_View_ZoomToFitDuringAcq;
	static const QString Key_View_ZoomToFitAfterAcq;
	static const QString Key_View_TriggerIsZeroTime;
	static const QString Key_View_ColoredBG;
	static const QString Key_View_StickyScrolling;
	static const QString Key_View_ShowSamplingPoints;
	static const QString Key_View_ShowAnalogMinorGrid;
	static const QString Key_View_ConversionThresholdDispMode;
	static const QString Key_View_DefaultDivHeight;
	static const QString Key_View_DefaultLogicHeight;
	static const QString Key_Dec_InitialStateConfigurable;
	static const QString Key_Log_BufferSize;
	static const QString Key_Log_NotifyOfStacktrace;

	enum ConvThrDispMode {
		ConvThrDispMode_None = 0,
		ConvThrDispMode_Background,
		ConvThrDispMode_Dots
	};

public:
	GlobalSettings();

	void set_defaults_where_needed();

	static void add_change_handler(GlobalSettingsInterface *cb);
	static void remove_change_handler(GlobalSettingsInterface *cb);

	void setValue(const QString& key, const QVariant& value);

	/**
	 * Begins the tracking of changes. All changes will
	 * be recorded until stop_tracking() is called.
	 * The change tracking is global and doesn't support nesting.
	 */
	void start_tracking();

	/**
	 * Ends the tracking of changes without any changes to the settings.
	 */
	void stop_tracking();

	/**
	 * Ends the tracking of changes, undoing the changes since the
	 * change tracking began.
	 */
	void undo_tracked_changes();

	static void store_gvariant(QSettings &settings, GVariant *v);

	static GVariant* restore_gvariant(QSettings &settings);

private:
	static vector<GlobalSettingsInterface*> callbacks_;

	static bool tracking_;
	static map<QString, QVariant> tracked_changes_;
};

} // namespace pv

#endif // PULSEVIEW_GLOBALSETTINGS_HPP
