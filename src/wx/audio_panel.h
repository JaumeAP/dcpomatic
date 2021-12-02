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


#include "lib/audio_mapping.h"
#include "content_sub_panel.h"
#include "content_widget.h"


class wxSpinCtrlDouble;
class wxButton;
class wxChoice;
class wxStaticText;
class AudioMappingView;
class AudioDialog;
class LanguageTagWidget;


class AudioPanel : public ContentSubPanel
{
public:
	explicit AudioPanel (ContentPanel *);
	~AudioPanel ();

	void create () override;
	void film_changed (Film::Property) override;
	void film_content_changed (int) override;
	void content_selection_changed () override;
	void set_film (std::shared_ptr<Film>);

private:
	void show_clicked ();
	void gain_calculate_button_clicked ();
	void mapping_changed (AudioMapping);
	void setup_description ();
	void setup_peak ();
	void active_jobs_changed (boost::optional<std::string>, boost::optional<std::string>);
	void setup_sensitivity ();
	void reference_clicked ();
	void add_to_grid () override;
	boost::optional<float> peak () const;

	wxCheckBox* _reference = nullptr;
	wxStaticText* _reference_note = nullptr;
	wxButton* _show = nullptr;
	wxStaticText* _gain_label = nullptr;
	wxStaticText* _gain_db_label = nullptr;
	ContentSpinCtrlDouble<AudioContent>* _gain = nullptr;
	wxButton* _gain_calculate_button = nullptr;
	wxStaticText* _peak = nullptr;
	wxStaticText* _delay_label = nullptr;
	wxStaticText* _delay_ms_label = nullptr;
	ContentSpinCtrl<AudioContent>* _delay = nullptr;
	AudioMappingView* _mapping = nullptr;
	wxStaticText* _description = nullptr;
	AudioDialog* _audio_dialog = nullptr;

	boost::signals2::scoped_connection _mapping_connection;
	boost::signals2::scoped_connection _active_jobs_connection;
};
