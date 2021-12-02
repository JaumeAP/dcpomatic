/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


/** @file  src/lib/video_panel.h
 *  @brief VideoPanel class.
 */


#include "content_sub_panel.h"
#include "content_widget.h"
#include "timecode.h"
#include "lib/film.h"


class wxButton;
class wxChoice;
class wxSpinCtrl;
class wxStaticText;
class wxToggleButton;


/** @class VideoPanel
 *  @brief The video tab of the film editor.
 */
class VideoPanel : public ContentSubPanel
{
public:
	explicit VideoPanel (ContentPanel *);

	void create () override;
	void film_changed (Film::Property) override;
	void film_content_changed (int) override;
	void content_selection_changed () override;

private:
	void reference_clicked ();
	void colour_conversion_changed ();
	void edit_colour_conversion_clicked ();
	void range_changed ();
	void fade_in_changed ();
	void fade_out_changed ();
	void add_to_grid () override;
	void scale_fit_clicked ();
	void scale_custom_clicked ();
	bool scale_custom_edit_clicked ();
	void left_right_link_clicked ();
	void top_bottom_link_clicked ();
	void left_crop_changed ();
	void right_crop_changed ();
	void top_crop_changed ();
	void bottom_crop_changed ();

	void setup_description ();
	void setup_sensitivity ();

	wxCheckBox* _reference = nullptr;
	wxStaticText* _reference_note = nullptr;
	wxStaticText* _type_label = nullptr;
	ContentChoice<VideoContent, VideoFrameType>* _frame_type = nullptr;
	wxStaticText* _crop_label = nullptr;
	wxStaticText* _left_crop_label = nullptr;
	ContentSpinCtrl<VideoContent>* _left_crop = nullptr;
	wxToggleButton* _left_right_link = nullptr;
	wxStaticText* _right_crop_label = nullptr;
	ContentSpinCtrl<VideoContent>* _right_crop = nullptr;
	wxStaticText* _top_crop_label = nullptr;
	ContentSpinCtrl<VideoContent>* _top_crop = nullptr;
	wxToggleButton* _top_bottom_link = nullptr;
	wxStaticText* _bottom_crop_label = nullptr;
	ContentSpinCtrl<VideoContent>* _bottom_crop = nullptr;
	wxStaticText* _fade_in_label = nullptr;
	Timecode<dcpomatic::ContentTime>* _fade_in = nullptr;
	wxStaticText* _fade_out_label = nullptr;
	Timecode<dcpomatic::ContentTime>* _fade_out = nullptr;
	wxStaticText* _scale_label = nullptr;
	wxRadioButton* _scale_fit = nullptr;
	wxRadioButton* _scale_custom = nullptr;
	wxButton* _scale_custom_edit = nullptr;
	wxStaticText* _description = nullptr;
	wxStaticText* _colour_conversion_label = nullptr;
	wxChoice* _colour_conversion = nullptr;
	wxButton* _edit_colour_conversion_button = nullptr;
	wxStaticText* _range_label = nullptr;
	wxChoice* _range = nullptr;

	bool _left_changed_last = true;
	bool _top_changed_last = true;
};
