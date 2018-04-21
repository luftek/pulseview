/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <extdef.h>

#include <cassert>
#include <cmath>

#include <QApplication>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLineEdit>

#include "trace.hpp"
#include "tracepalette.hpp"
#include "view.hpp"

#include "pv/globalsettings.hpp"
#include "pv/widgets/colorbutton.hpp"
#include "pv/widgets/popup.hpp"

using std::pair;
using std::shared_ptr;

namespace pv {
namespace views {
namespace trace {

const QPen Trace::AxisPen(QColor(0, 0, 0, 30 * 256 / 100));
const int Trace::LabelHitPadding = 2;

const QColor Trace::BrightGrayBGColor = QColor(0, 0, 0, 10 * 255 / 100);
const QColor Trace::DarkGrayBGColor = QColor(0, 0, 0, 15 * 255 / 100);

Trace::Trace(shared_ptr<data::SignalBase> channel) :
	base_(channel),
	axis_pen_(AxisPen),
	segment_display_mode_(ShowLastSegmentOnly),  // Will be overwritten by View
	current_segment_(0),
	popup_(nullptr),
	popup_form_(nullptr)
{
	connect(channel.get(), SIGNAL(name_changed(const QString&)),
		this, SLOT(on_name_changed(const QString&)));
	connect(channel.get(), SIGNAL(color_changed(const QColor&)),
		this, SLOT(on_color_changed(const QColor&)));
}

shared_ptr<data::SignalBase> Trace::base() const
{
	return base_;
}

void Trace::paint_label(QPainter &p, const QRect &rect, bool hover)
{
	const int y = get_visual_y();

	p.setBrush(base_->color());

	if (!enabled())
		return;

	const QRectF r = label_rect(rect);

	// When selected, move the arrow to the left so that the border can show
	const QPointF offs = (selected()) ? QPointF(-2, 0) : QPointF(0, 0);

	// Paint the label
	const float label_arrow_length = r.height() / 2;
	QPointF points[] = {
		offs + r.topLeft(),
		offs + QPointF(r.right() - label_arrow_length, r.top()),
		offs + QPointF(r.right(), y),
		offs + QPointF(r.right() - label_arrow_length, r.bottom()),
		offs + r.bottomLeft()
	};
	QPointF highlight_points[] = {
		offs + QPointF(r.left() + 1, r.top() + 1),
		offs + QPointF(r.right() - label_arrow_length, r.top() + 1),
		offs + QPointF(r.right() - 1, y),
		offs + QPointF(r.right() - label_arrow_length, r.bottom() - 1),
		offs + QPointF(r.left() + 1, r.bottom() - 1)
	};

	if (selected()) {
		p.setPen(highlight_pen());
		p.setBrush(Qt::transparent);
		p.drawPolygon(points, countof(points));
	}

	p.setPen(Qt::transparent);
	p.setBrush(hover ? base_->color().lighter() : base_->color());
	p.drawPolygon(points, countof(points));

	p.setPen(base_->color().lighter());
	p.setBrush(Qt::transparent);
	p.drawPolygon(highlight_points, countof(highlight_points));

	p.setPen(base_->color().darker());
	p.setBrush(Qt::transparent);
	p.drawPolygon(points, countof(points));

	// Paint the text
	p.setPen(select_text_color(base_->color()));
	p.setFont(QApplication::font());
	p.drawText(QRectF(r.x(), r.y(),
		r.width() - label_arrow_length, r.height()),
		Qt::AlignCenter | Qt::AlignVCenter, base_->name());
}

QMenu* Trace::create_context_menu(QWidget *parent)
{
	QMenu *const menu = ViewItem::create_context_menu(parent);

	return menu;
}

pv::widgets::Popup* Trace::create_popup(QWidget *parent)
{
	using pv::widgets::Popup;

	popup_ = new Popup(parent);
	popup_->set_position(parent->mapToGlobal(
		drag_point(parent->rect())), Popup::Right);

	create_popup_form();

	connect(popup_, SIGNAL(closed()), this, SLOT(on_popup_closed()));

	return popup_;
}

QRectF Trace::label_rect(const QRectF &rect) const
{
	QFontMetrics m(QApplication::font());
	const QSize text_size(
		m.boundingRect(QRect(), 0, base_->name()).width(), m.height());
	const QSizeF label_size(
		text_size.width() + LabelPadding.width() * 2,
		ceilf((text_size.height() + LabelPadding.height() * 2) / 2) * 2);
	const float half_height = label_size.height() / 2;
	return QRectF(
		rect.right() - half_height - label_size.width() - 0.5,
		get_visual_y() + 0.5f - half_height,
		label_size.width() + half_height,
		label_size.height());
}

void Trace::set_current_segment(const int segment)
{
	current_segment_ = segment;
}

int Trace::get_current_segment() const
{
	return current_segment_;
}

void Trace::paint_back(QPainter &p, ViewItemPaintParams &pp)
{
	const View *view = owner_->view();
	assert(view);

	if (view->colored_bg())
		p.setBrush(base_->bgcolor());
	else
		p.setBrush(pp.next_bg_color_state() ? BrightGrayBGColor : DarkGrayBGColor);

	p.setPen(QPen(Qt::NoPen));

	const pair<int, int> extents = v_extents();
	p.drawRect(pp.left(), get_visual_y() + extents.first,
		pp.width(), extents.second - extents.first);
}

void Trace::paint_axis(QPainter &p, ViewItemPaintParams &pp, int y)
{
	p.setRenderHint(QPainter::Antialiasing, false);

	p.setPen(axis_pen_);
	p.drawLine(QPointF(pp.left(), y), QPointF(pp.right(), y));

	p.setRenderHint(QPainter::Antialiasing, true);
}

void Trace::add_color_option(QWidget *parent, QFormLayout *form)
{
	using pv::widgets::ColorButton;

	ColorButton *const color_button = new ColorButton(
		TracePalette::Rows, TracePalette::Cols, parent);
	color_button->set_palette(TracePalette::Colors);
	color_button->set_color(base_->color());
	connect(color_button, SIGNAL(selected(const QColor&)),
		this, SLOT(on_coloredit_changed(const QColor&)));

	form->addRow(tr("Color"), color_button);
}

void Trace::create_popup_form()
{
	// Clear the layout

	// Transfer the layout and the child widgets to a temporary widget
	// which we delete after the event was handled. This way, the layout
	// and all widgets contained therein are deleted after the event was
	// handled, leaving the parent popup_ time to handle the change.
	if (popup_form_) {
		QWidget *suicidal = new QWidget();
		suicidal->setLayout(popup_form_);
		suicidal->deleteLater();
	}

	// Repopulate the popup
	popup_form_ = new QFormLayout(popup_);
	popup_->setLayout(popup_form_);
	populate_popup_form(popup_, popup_form_);
}

void Trace::populate_popup_form(QWidget *parent, QFormLayout *form)
{
	QLineEdit *const name_edit = new QLineEdit(parent);
	name_edit->setText(base_->name());
	connect(name_edit, SIGNAL(textChanged(const QString&)),
		this, SLOT(on_nameedit_changed(const QString&)));
	form->addRow(tr("Name"), name_edit);

	add_color_option(parent, form);
}

void Trace::set_segment_display_mode(SegmentDisplayMode mode)
{
	segment_display_mode_ = mode;

	if (owner_)
		owner_->row_item_appearance_changed(true, true);
}

void Trace::on_name_changed(const QString &text)
{
	/* This event handler is called by SignalBase when the name was changed there */
	(void)text;

	if (owner_) {
		owner_->extents_changed(true, false);
		owner_->row_item_appearance_changed(true, false);
	}
}

void Trace::on_color_changed(const QColor &color)
{
	/* This event handler is called by SignalBase when the color was changed there */
	(void)color;

	if (owner_)
		owner_->row_item_appearance_changed(true, true);
}

void Trace::on_popup_closed()
{
	popup_ = nullptr;
	popup_form_ = nullptr;
}

void Trace::on_nameedit_changed(const QString &name)
{
	/* This event handler notifies SignalBase that the name changed */
	base_->set_name(name);
}

void Trace::on_coloredit_changed(const QColor &color)
{
	/* This event handler notifies SignalBase that the color changed */
	base_->set_color(color);
}

} // namespace trace
} // namespace views
} // namespace pv
