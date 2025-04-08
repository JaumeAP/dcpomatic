/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_button.h"
#include "file_dialog.h"
#include "verify_dcp_result_panel.h"
#include "wx_util.h"
#include "lib/verify_dcp_job.h"
#include <dcp/verify.h>
#include <dcp/verify_report.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/grid.h>
#include <wx/notebook.h>
#include <wx/treectrl.h>
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>


using std::list;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;


VerifyDCPResultPanel::VerifyDCPResultPanel(wxWindow* parent)
	: wxPanel(parent, wxID_ANY)
	, _types{
		dcp::VerificationNote::Type::ERROR,
		dcp::VerificationNote::Type::BV21_ERROR,
		dcp::VerificationNote::Type::WARNING
	}
{
	auto sizer = new wxBoxSizer(wxVERTICAL);
	auto notebook = new wxNotebook(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 400));
	sizer->Add(notebook, 1, wxEXPAND | wxALL, DCPOMATIC_DIALOG_BORDER);

	auto names = map<dcp::VerificationNote::Type, wxString>{
		{ dcp::VerificationNote::Type::ERROR, _("Errors") },
		{ dcp::VerificationNote::Type::BV21_ERROR, _("SMPTE Bv2.1 errors") },
		{ dcp::VerificationNote::Type::WARNING, _("Warnings") }
	};

	for (auto const type: _types) {
		auto window = new wxScrolledWindow(notebook, wxID_ANY);
		auto sizer = new wxBoxSizer(wxVERTICAL);
		auto grid = new wxGrid(window, wxID_ANY);

		_pages[type] = { window, sizer, grid };

		window->SetSizer(sizer);
		notebook->AddPage(window, names[type]);

		grid->CreateGrid(0, 10);
		grid->HideRowLabels();
		grid->SetColLabelValue(0, names[type]);
		grid->SetColLabelValue(1, _("Reel"));
		grid->SetColLabelValue(2, _("Frame"));
		grid->SetColLabelValue(3, _("Timecode"));
		grid->SetColLabelValue(4, _("Line"));
		grid->SetColLabelValue(5, _("Component"));
		grid->SetColLabelValue(6, _("File"));
		grid->SetColLabelValue(7, _("CPL"));
		grid->SetColLabelValue(8, _("PKL"));
		grid->SetColLabelValue(9, _("Asset"));

		grid->AutoSize();
		grid->DisableCellEditControl();
		grid->EnableEditing(false);
		grid->HideCellEditControl();
		grid->DisableDragRowMove();
		grid->DisableDragRowSize();
		grid->SetDefaultCellFitMode(wxGridFitMode::Clip());
		grid->SetCellHighlightROPenWidth(0);
		grid->SetCellHighlightPenWidth(0);
		grid->SetSelectionMode(wxGrid::wxGridSelectNone);

		sizer->Add(grid, 1, wxEXPAND);
		sizer->Layout();
	}

	_summary = new wxStaticText(this, wxID_ANY, {});
	sizer->Add(_summary, 0, wxALL, DCPOMATIC_DIALOG_BORDER);

	auto save_sizer = new wxBoxSizer(wxHORIZONTAL);
	_save_text_report = new Button(this, _("Save report as text..."));
	save_sizer->Add(_save_text_report, 0, wxALL, DCPOMATIC_SIZER_GAP);
	_save_html_report = new Button(this, _("Save report as HTML..."));
	save_sizer->Add(_save_html_report, 0, wxALL, DCPOMATIC_SIZER_GAP);
	sizer->Add(save_sizer);

	SetSizer(sizer);
	sizer->Layout();
	sizer->SetSizeHints(this);

	_save_text_report->bind(&VerifyDCPResultPanel::save_text_report, this);
	_save_html_report->bind(&VerifyDCPResultPanel::save_html_report, this);

	_save_text_report->Enable(false);
	_save_html_report->Enable(false);
}


void
VerifyDCPResultPanel::add_jobs(vector<shared_ptr<const VerifyDCPJob>> jobs)
{
	_jobs = jobs;

	for (auto const type: _types) {
		auto grid = _pages[type].grid;
		if (auto rows = grid->GetNumberRows()) {
			grid->DeleteRows(0, rows);
		}
	}

	map<dcp::VerificationNote::Type, int> counts;
	for (auto type: _types) {
		counts[type] = 0;
	}

	for (auto job: jobs) {
		auto job_counts = add_job(job);
		for (auto const type: _types) {
			counts[type] += job_counts[type];
		}
	}

	for (auto type: _types) {
		_pages[type].grid->AutoSize();
		_pages[type].sizer->Layout();
	}

	wxString summary_text;

	if (counts[dcp::VerificationNote::Type::ERROR] == 1) {
		/// TRANSLATORS: this will be used at the start of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text = _("1 error, ");
	} else {
		/// TRANSLATORS: this will be used at the start of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text = wxString::Format(_("%d errors, "), counts[dcp::VerificationNote::Type::ERROR]);
	}

	if (counts[dcp::VerificationNote::Type::BV21_ERROR] == 1) {
		/// TRANSLATORS: this will be used in the middle of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text += _("1 Bv2.1 error, ");
	} else {
		/// TRANSLATORS: this will be used in the middle of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text += wxString::Format(_("%d Bv2.1 errors, "), counts[dcp::VerificationNote::Type::BV21_ERROR]);
	}

	if (counts[dcp::VerificationNote::Type::WARNING] == 1) {
		/// TRANSLATORS: this will be used at the end of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text += _("and 1 warning.");
	} else {
		/// TRANSLATORS: this will be used at the end of a string like "1 error, 2 Bv2.1 errors and 3 warnings."
		summary_text += wxString::Format(_("and %d warnings."), counts[dcp::VerificationNote::Type::WARNING]);
	}

	_summary->SetLabel(summary_text);

	_save_text_report->Enable(true);
	_save_html_report->Enable(true);
}


map<dcp::VerificationNote::Type, int>
VerifyDCPResultPanel::add_job(shared_ptr<const VerifyDCPJob> job)
{
	map<dcp::VerificationNote::Type, int> counts;
	for (auto type: _types) {
		counts[type] = 0;
	}

	int constexpr limit_per_type = 20;

	for (auto type: _types) {
		auto grid = _pages[type].grid;
		bool const spacer = grid->GetNumberRows() > 0;
		grid->AppendRows(spacer ? 2 : 1);
		auto const row = grid->GetNumberRows() - 1;
		grid->SetCellValue(row, 0, std_to_wx(job->directories()[0].filename().string()));
		auto font = grid->GetCellFont(row, 0);
		font.SetWeight(wxFONTWEIGHT_BOLD);
		grid->SetCellFont(row, 0, font);
		if (spacer) {
			grid->SetCellSize(row - 1, 0, 1, grid->GetNumberCols());
		}
		grid->SetCellSize(row, 0, 1, grid->GetNumberCols());
		grid->SetCellBackgroundColour(row, 0, *wxLIGHT_GREY);
	}

	auto substitute = [](wxString message, dcp::VerificationNote const& note) {
		message = std_to_wx(dcp::substitute(wx_to_std(message), note));

		// XXX: sort out timecode in the UI
		if (note.frame()) {
			message.Replace(char_to_wx("%frame"), std_to_wx(fmt::to_string(note.frame().get())));
			message.Replace(
				char_to_wx("%timecode"),
				std_to_wx(
					dcp::Time(note.frame().get(), note.frame_rate()->numerator, note.frame_rate()->numerator).as_string(dcp::Standard::SMPTE)
					));
		}

		return message;
	};

	auto add = [this, &substitute](vector<dcp::VerificationNote> const& notes, wxString message, wxString more_message = {}) {
		for (auto const& note: notes) {
			auto grid = _pages[note.type()].grid;
			grid->AppendRows();
			auto const row = grid->GetNumberRows() - 1;
			int column = 1;

			grid->SetCellValue(row, 0, substitute(message, note));
			if (auto reel_index = note.reel_index()) {
				grid->SetCellValue(row, column, wxString::Format(_("%d"), *reel_index + 1));
			}
			column++;
			if (auto frame = note.frame()) {
				grid->SetCellValue(row, column, wxString::Format(_("%d"), *frame));
				grid->SetCellValue(row, column + 1, std_to_wx(
					dcp::Time(note.frame().get(), note.frame_rate()->numerator, note.frame_rate()->numerator).as_string(dcp::Standard::SMPTE))
				);
			}
			column += 2;
			if (auto line = note.line()) {
				grid->SetCellValue(row, column, wxString::Format(_("%d"), *line));
			}
			column++;
			if (auto component = note.component()) {
				grid->SetCellValue(row, column, wxString::Format(_("%d"), *component));
			}
			column++;
			if (auto file = note.file()) {
				grid->SetCellValue(row, column, std_to_wx(file->filename().string()));
			}
			column++;
			if (auto cpl_id = note.cpl_id()) {
				grid->SetCellValue(row, column, std_to_wx(*cpl_id));
			}
			column++;
			if (auto pkl_id = note.pkl_id()) {
				grid->SetCellValue(row, column, std_to_wx(*pkl_id));
			}
			column++;
			if (auto asset_id = note.asset_id()) {
				grid->SetCellValue(row, column, std_to_wx(*asset_id));
			}
			column++;
		}
		if (notes.size() == limit_per_type && !more_message.IsEmpty()) {
			auto grid = _pages[notes[0].type()].grid;
			grid->AppendRows();
			grid->SetCellValue(grid->GetNumberRows() - 1, 0, more_message);
		}
	};

	if (job->finished_in_error() && job->error_summary() != "") {
		/* We have an error that did not come from dcp::verify */
		auto grid = _pages[dcp::VerificationNote::Type::ERROR].grid;
		grid->AppendRows();
		grid->SetCellValue(grid->GetNumberRows() - 1, 0, std_to_wx(job->error_summary()));
	}

	/* Gather notes by code, discarding more than limit_per_type so we don't get overwhelmed if
	 * every frame of a long DCP has a note.
	 */
	std::map<dcp::VerificationNote::Code, std::vector<dcp::VerificationNote>> notes_by_code;

	for (auto const& note: job->result().notes) {
		counts[note.type()]++;
		auto type_iter = notes_by_code.find(note.code());
		if (type_iter != notes_by_code.end()) {
			if (type_iter->second.size() < limit_per_type) {
				type_iter->second.push_back(note);
			}
		} else {
			notes_by_code[note.code()] = { note };
		}
	}

	for (auto const& i: notes_by_code) {
		switch (i.first) {
		case dcp::VerificationNote::Code::FAILED_READ:
			add(i.second, _("Could not read DCP (%error)"));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_CPL_HASHES:
			add(i.second, _("The hash (%reference_hash) of a CPL in the PKL does not agree with the CPL file (%calculated_hash).  This probably means that the CPL file is corrupt."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE:
			add(i.second, _("The picture in a reel has a frame rate of %frame_rate, which is not valid."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_PICTURE_HASH:
			add(i.second, _("The hash (%calculated_hash) of a picture asset does not agree with the PKL file (%reference_hash).  This probably means that the asset file is corrupt."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_PICTURE_HASHES:
			add(i.second, _("The PKL and CPL hashes disagree for picture asset %f."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_SOUND_HASH:
			add(i.second, _("The hash (%calculated_hash) of a sound asset does not agree with the PKL file (%reference_hash).  This probably means that the asset file is corrupt."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_SOUND_HASHES:
			add(i.second, _("The PKL and CPL hashes disagree a sound asset."));
			break;
		case dcp::VerificationNote::Code::EMPTY_ASSET_PATH:
			add(i.second, _("An asset has an empty path in the ASSETMAP."));
			break;
		case dcp::VerificationNote::Code::MISSING_ASSET:
			add(i.second, _("An asset is missing."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_STANDARD:
			add(i.second, _("Parts of the DCP are written according to the Interop standard and parts according to SMPTE."));
			break;
		case dcp::VerificationNote::Code::INVALID_XML:
			for (auto const& note: i.second) {
				if (note.line()) {
					add({ note }, _("XML is malformed on line %l (%error)."));
				} else {
					add({ note }, _("XML is malformed (%error)."));
				}
			}
			break;
		case dcp::VerificationNote::Code::MISSING_ASSETMAP:
			add(i.second, _("No ASSETMAP or ASSETMAP.xml file was found."));
			break;
		case dcp::VerificationNote::Code::INVALID_INTRINSIC_DURATION:
			add(i.second, _("An asset has an intrinsic duration of less than 1 second, which is invalid."));
			break;
		case dcp::VerificationNote::Code::INVALID_DURATION:
			add(i.second, _("An asset has a duration of less than 1 second, which is invalid."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_SIZE_IN_BYTES:
			add(
				i.second,
				_("Frame has an instantaneous bit rate that is over the limit of 250Mbit/s."),
				_("More frames (not listed) have an instantaneous bit rate that is over the limit of 250Mbit/s.")
			);
			break;
		case dcp::VerificationNote::Code::NEARLY_INVALID_PICTURE_FRAME_SIZE_IN_BYTES:
			add(
				i.second,
				_("Frame has an instantaneous bit rate that is close to the limit of 250Mbit/s."),
				_("More frames (not listed) have an instantaneous bit rate that is close to the limit of 250Mbit/s.")
			);
			break;
		case dcp::VerificationNote::Code::EXTERNAL_ASSET:
			add(i.second, _("This DCP refers to at least one asset in another DCP (and perhaps others), so it is a \"version file\" (VF)"));
			break;
		case dcp::VerificationNote::Code::THREED_ASSET_MARKED_AS_TWOD:
			add(i.second, _("An asset is 3D but its MXF is marked as 2D."));
			break;
		case dcp::VerificationNote::Code::INVALID_STANDARD:
			add(i.second, _("This DCP uses the Interop standard, but it should be made with SMPTE."));
			break;
		case dcp::VerificationNote::Code::INVALID_LANGUAGE:
			add(i.second, _("The invalid language tag %language is used."));
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_PICTURE_SIZE_IN_PIXELS:
			add(i.second, _("A JPEG2000 video asset uses the invalid image size %size_in_pixels."));
			break;
		case dcp::VerificationNote::Code::INVALID_MPEG2_PICTURE_SIZE_IN_PIXELS:
			add(i.second, _("A MPEG2 video asset uses the invalid image size %size_in_pixels."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE_FOR_2K:
			add(i.second, _("A video asset uses the invalid frame rate %frame_rate."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_FRAME_RATE_FOR_4K:
			add(i.second, _("A video asset uses the frame rate %frame_rate which is invalid for 4K video."));
			break;
		case dcp::VerificationNote::Code::INVALID_PICTURE_ASSET_RESOLUTION_FOR_3D:
			add(i.second, _("A video asset uses the frame rate %frame_rate which is invalid for 3D video."));
			break;
		case dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_XML_SIZE_IN_BYTES:
			add(i.second, _("The XML in a closed caption asset takes up %size_in_bytes bytes, which is over the 256KB limit."));
			break;
		case dcp::VerificationNote::Code::INVALID_TIMED_TEXT_SIZE_IN_BYTES:
			add(i.second, _("A timed text asset takes up %size_in_bytes bytes, which is over the 115MB limit."));
			break;
		case dcp::VerificationNote::Code::INVALID_TIMED_TEXT_FONT_SIZE_IN_BYTES:
			add(i.second, _("The fonts in a timed text asset take up %size_in_bytes bytes, which is over the 10MB limit."));
			break;
		case dcp::VerificationNote::Code::MISSING_SUBTITLE_LANGUAGE:
			add(i.second, _("A subtitle asset contains no <Language> tag."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_SUBTITLE_LANGUAGES:
			add(i.second, _("Not all subtitle assets specify the same <Language> tag."));
			break;
		case dcp::VerificationNote::Code::MISSING_SUBTITLE_START_TIME:
			add(i.second, _("A subtitle asset contains no <StartTime> tag."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_START_TIME:
			add(i.second, _("A subtitle asset has a <StartTime> which is not zero."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_FIRST_TEXT_TIME:
			add(i.second, _("The first subtitle or closed caption happens before 4s into the first reel."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_DURATION:
			add(i.second, _("At least one subtitle lasts less than 15 frames."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_SPACING:
			add(i.second, _("At least one pair of subtitles is separated by less than 2 frames."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_LINE_COUNT:
			add(i.second, _("There are more than 3 subtitle lines in at least one place."));
			break;
		case dcp::VerificationNote::Code::NEARLY_INVALID_SUBTITLE_LINE_LENGTH:
			add(i.second, _("There are more than 52 characters in at least one subtitle line."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_LINE_LENGTH:
			add(i.second, _("There are more than 79 characters in at least one subtitle line."));
			break;
		case dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_LINE_COUNT:
			add(i.second, _("There are more than 3 closed caption lines in at least one place."));
			break;
		case dcp::VerificationNote::Code::INVALID_CLOSED_CAPTION_LINE_LENGTH:
			add(i.second, _("There are more than 32 characters in at least one closed caption line."));
			break;
		case dcp::VerificationNote::Code::INVALID_SOUND_FRAME_RATE:
			add(i.second, _("A sound asset has an invalid frame rate of %frame_rate."));
			break;
		case dcp::VerificationNote::Code::INVALID_SOUND_BIT_DEPTH:
			add(i.second, _("A sound asset has an invalid bit depth of %bit_depth."));
			break;
		case dcp::VerificationNote::Code::MISSING_CPL_ANNOTATION_TEXT:
			add(i.second, _("A CPL has no <AnnotationText> tag."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_CPL_ANNOTATION_TEXT:
			add(i.second, _("A CPL has an <AnnotationText> which is not the same as its <ContentTitleText>."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_ASSET_DURATION:
			add(i.second, _("At least one asset in a reel does not have the same duration as the others."));
			break;
		case dcp::VerificationNote::Code::MISSING_MAIN_SUBTITLE_FROM_SOME_REELS:
			add(i.second, _("The DCP has subtitles but at least one reel has no subtitle asset."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_CLOSED_CAPTION_ASSET_COUNTS:
			add(i.second, _("The DCP has closed captions but not every reel has the same number of closed caption assets."));
			break;
		case dcp::VerificationNote::Code::MISSING_SUBTITLE_ENTRY_POINT:
			add(i.second, _("A subtitle asset has no <EntryPoint> tag."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_SUBTITLE_ENTRY_POINT:
			add(i.second, _("A subtitle asset has a non-zero <EntryPoint>."));
			break;
		case dcp::VerificationNote::Code::MISSING_CLOSED_CAPTION_ENTRY_POINT:
			add(i.second, _("A closed caption asset has no <EntryPoint> tag."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_CLOSED_CAPTION_ENTRY_POINT:
			add(i.second, _("A closed caption asset has a non-zero <EntryPoint>."));
			break;
		case dcp::VerificationNote::Code::MISSING_HASH:
			add(i.second, _("An asset has no <Hash> in the CPL."));
			break;
		case dcp::VerificationNote::Code::MISSING_FFEC_IN_FEATURE:
			add(i.second, _("The DCP is a feature but has no FFEC (first frame of end credits) marker."));
			break;
		case dcp::VerificationNote::Code::MISSING_FFMC_IN_FEATURE:
			add(i.second, _("The DCP is a feature but has no FFMC (first frame of moving credits) marker."));
			break;
		case dcp::VerificationNote::Code::MISSING_FFOC:
			add(i.second, _("The DCP has no FFOC (first frame of content) marker."));
			break;
		case dcp::VerificationNote::Code::MISSING_LFOC:
			add(i.second, _("The DCP has no LFOC (last frame of content) marker."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_FFOC:
			add(i.second, _("The DCP has a FFOC of %marker_position instead of 1."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_LFOC:
			add(i.second, _("The DCP has a LFOC of %marker_position iustead of the reel duration minus one."));
			break;
		case dcp::VerificationNote::Code::MISSING_CPL_METADATA:
			add(i.second, _("A CPL has no CPL metadata tag"));
			break;
		case dcp::VerificationNote::Code::MISSING_CPL_METADATA_VERSION_NUMBER:
			add(i.second, _("A CPL has no CPL metadata version number tag."));
			break;
		case dcp::VerificationNote::Code::MISSING_EXTENSION_METADATA:
			add(i.second, _("A CPL has no CPL extension metadata tag."));
			break;
		case dcp::VerificationNote::Code::INVALID_EXTENSION_METADATA:
			add(i.second, _("A CPL has an invalid CPL extension metadata tag (%error)"));
			break;
		case dcp::VerificationNote::Code::UNSIGNED_CPL_WITH_ENCRYPTED_CONTENT:
			add(i.second, _("A CPL has encrypted content but is not signed."));
			break;
		case dcp::VerificationNote::Code::UNSIGNED_PKL_WITH_ENCRYPTED_CONTENT:
			add(i.second, _("A PKL has encrypted content but is not signed."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_PKL_ANNOTATION_TEXT_WITH_CPL:
			add(i.second, _("A PKL has an <AnnotationText> which does not match its CPL's <ContentTitleText>."));
			break;
		case dcp::VerificationNote::Code::PARTIALLY_ENCRYPTED:
			add(i.second, _("The DCP has encrypted content, but not all its assets are encrypted."));
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_CODESTREAM:
			add(
				i.second,
				_("A picture frame has an invalid JPEG2000 codestream (%error)."),
				_("More picture frames (not listed) have invalid JPEG2000 codestreams.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_2K:
			add(
				i.second,
				_("A 2K JPEG2000 frame has %guard_bits guard bits instead of 1."),
				_("More 2K JPEG2000 frames (not listed) have an invalid number of guard bits.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_GUARD_BITS_FOR_4K:
			add(
				i.second,
				_("A 4K JPEG2000 frame has %guard_bits guard bits instead of 2."),
				_("More 4K JPEG2000 frames (not listed) have an invalid number of guard bits.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_TILE_SIZE:
			add(
				i.second,
				_("A JPEG2000 tile size does not match the image size."),
				_("More JPEG2000 tile sizes (not listed) do not match the image size.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_CODE_BLOCK_WIDTH:
			add(
				i.second,
				_("A JPEG2000 frame has a code-block width of %code_block_size instead of 32."),
				_("More JPEG2000 frames (not listed) have an invalid code-block width.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_CODE_BLOCK_HEIGHT:
			add(
				i.second,
				_("A JPEG2000 frame has a code-block height of %code_block_size instead of 32."),
				_("More JPEG2000 frames (not listed) have an invalid code-block height.")
			);
			break;
		case dcp::VerificationNote::Code::INCORRECT_JPEG2000_POC_MARKER_COUNT_FOR_2K:
			add(
				i.second,
				_("A 2K JPEG2000 frame has %poc_markers POC marker(s) instead of 0."),
				_("More 2K JPEG2000 frames (not listed) have too many POC markers.")
			);
			break;
		case dcp::VerificationNote::Code::INCORRECT_JPEG2000_POC_MARKER_COUNT_FOR_4K:
			add(
				i.second,
				_("A 4K JPEG2000 frame has %poc_markers POC marker(s) instead of 1."),
				_("More 4K JPEG2000 frames (not listed) have too many POC markers.")
			);
			break;
		case dcp::VerificationNote::Code::INCORRECT_JPEG2000_POC_MARKER:
			add(
				i.second,
				_("A JPEG2000 frame contains an invalid POC marker (%error)."),
				_("More JPEG2000 frames (not listed) contain invalid POC markers.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_POC_MARKER_LOCATION:
			add(
				i.second,
				_("A JPEG2000 frame contains a POC marker in an invalid location."),
				_("More JPEG2000 frames (not listed) contain POC markers in invalid locations.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_TILE_PARTS_FOR_2K:
			add(
				i.second,
				_("A 2K JPEG2000 frame contains %tile_parts tile parts instead of 3."),
				_("More 2K JPEG2000 frames (not listed) contain the wrong number of tile parts.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_TILE_PARTS_FOR_4K:
			add(
				i.second,
				_("A 4K JPEG2000 frame contains %tile_parts tile parts instead of 6."),
				_("More JPEG2000 frames (not listed) contain the wrong number of tile parts.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_RSIZ_FOR_2K:
			add(
				i.second,
				_("A 2K JPEG2000 frame contains an invalid Rsiz (capabilities) value of %jpeg2000_capabilities"),
				_("More JPEG2000 frames (not listed) contain invalid Rsiz values.")
			);
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_RSIZ_FOR_4K:
			add(
				i.second,
				_("A 4K JPEG2000 frame contains an invalid Rsiz (capabilities) value of %jpeg2000_capabilities"),
				_("More JPEG2000 frames (not listed) contain invalid Rsiz values.")
			);
			break;
		case dcp::VerificationNote::Code::MISSING_JPEG2000_TLM_MARKER:
			add(
				i.second,
				_("A JPEG2000 frame has no TLM marker."),
				_("More JPEG2000 frames (not listed) have no TLM marker.")
			);
			break;
		case dcp::VerificationNote::Code::SUBTITLE_OVERLAPS_REEL_BOUNDARY:
			add(i.second, _("A subtitle lasts longer than the reel it is in."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_TIMED_TEXT_RESOURCE_ID:
			add(i.second, _("The Resource ID in a timed text MXF did not match the ID of the contained XML."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_TIMED_TEXT_ASSET_ID:
			add(i.second, _("The Asset ID in a timed text MXF is the same as the Resource ID or that of the contained XML."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_TIMED_TEXT_DURATION:
			add(i.second, _("The reel duration (%reel_duration) of some timed text is not the same as the ContainerDuration (%asset_duration) of its MXF."));
			break;
		case dcp::VerificationNote::Code::MISSED_CHECK_OF_ENCRYPTED:
			add(i.second, _("Part of the DCP could not be checked because no KDM was available."));
			break;
		case dcp::VerificationNote::Code::EMPTY_TEXT:
			add(i.second, _("At least one <Text> node in a subtitle or closed caption is empty."));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_CLOSED_CAPTION_VALIGN:
			add(i.second, _("Some closed <Text> or <Image> nodes have different vertical alignments within a <Subtitle>."));
			break;
		case dcp::VerificationNote::Code::INCORRECT_CLOSED_CAPTION_ORDERING:
			add(i.second, _("Some closed captions are not listed in the order of their vertical position."));
			break;
		case dcp::VerificationNote::Code::UNEXPECTED_ENTRY_POINT:
			add(i.second, _("There is a <EntryPoint> tag inside a <MainMarkers>."));
			break;
		case dcp::VerificationNote::Code::UNEXPECTED_DURATION:
			add(i.second, _("There is a <Duration> tag inside a <MainMarkers>."));
			break;
		case dcp::VerificationNote::Code::INVALID_CONTENT_KIND:
			add(i.second, _("An invalid <ContentKind> %content_kind has been used."));
			break;
		case dcp::VerificationNote::Code::INVALID_MAIN_PICTURE_ACTIVE_AREA:
			add(i.second, _("The <MainPictureActiveArea> is either not a multiple of 2, or is bigger than an asset."));
			break;
		case dcp::VerificationNote::Code::DUPLICATE_ASSET_ID_IN_PKL:
			add(i.second, _("A PKL has more than one asset with the same ID."));
			break;
		case dcp::VerificationNote::Code::DUPLICATE_ASSET_ID_IN_ASSETMAP:
			add(i.second, _("The ASSETMAP %asset_map_id has more than one asset with the same ID."));
			break;
		case dcp::VerificationNote::Code::MISSING_SUBTITLE:
			add(i.second, _("A subtitle asset contains no subtitles."));
			break;
		case dcp::VerificationNote::Code::INVALID_SUBTITLE_ISSUE_DATE:
			add(i.second, _("<IssueDate> has an invalid value %issue_date"));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_SOUND_CHANNEL_COUNTS:
			add(i.second, _("Sound assets do not all have the same channel count."));
			break;
		case dcp::VerificationNote::Code::INVALID_MAIN_SOUND_CONFIGURATION:
			add(i.second, _("<MainSoundConfiguration> is invalid (%error)"));
			break;
		case dcp::VerificationNote::Code::MISSING_FONT:
			add(i.second, _("The font file for font ID \"%load_font_id\" was not found, or was not referred to in the ASSETMAP."));
			break;
		case dcp::VerificationNote::Code::INVALID_JPEG2000_TILE_PART_SIZE:
			add(
				i.second,
				_("A frame has an image component that is too large (component is %size_in_bytes bytes in size)."),
				_("More frames (not listed) have image components that are too large.")
			);
			break;
		case dcp::VerificationNote::Code::INCORRECT_SUBTITLE_NAMESPACE_COUNT:
			add(i.second, _("The XML in a subtitle asset has more than one namespace declaration."));
			break;
		case dcp::VerificationNote::Code::MISSING_LOAD_FONT_FOR_FONT:
			add(i.second, _("A subtitle or closed caption refers to a font with ID %load_font_id that does not have a corresponding <LoadFont> node."));
			break;
		case dcp::VerificationNote::Code::MISSING_LOAD_FONT:
			add(i.second, _("A SMPTE subtitle asset has <Text> nodes but no <LoadFont> node"));
			break;
		case dcp::VerificationNote::Code::MISMATCHED_ASSET_MAP_ID:
			add(i.second, _("The asset with ID %mismatched_asset_id_from_asset_map in the asset map actually has an id of %mismatched_asset_id_from_file"));
			break;
		case dcp::VerificationNote::Code::EMPTY_CONTENT_VERSION_LABEL_TEXT:
			add(i.second, _("The <LabelText> in a <ContentVersion> in a CPL is empty"));
			break;
		case dcp::VerificationNote::Code::INVALID_CPL_NAMESPACE:
			add(i.second, _("A CPL has an invalid namespace %xml_namespace"));
			break;
		case dcp::VerificationNote::Code::MISSING_CPL_CONTENT_VERSION:
			add(i.second, _("A CPL has no <ContentVersion> tag"));
			break;
		case dcp::VerificationNote::Code::MATCHING_CPL_HASHES:
		case dcp::VerificationNote::Code::CORRECT_PICTURE_HASH:
		case dcp::VerificationNote::Code::VALID_PICTURE_FRAME_SIZES_IN_BYTES:
		case dcp::VerificationNote::Code::VALID_RELEASE_TERRITORY:
		case dcp::VerificationNote::Code::VALID_CPL_ANNOTATION_TEXT:
		case dcp::VerificationNote::Code::MATCHING_PKL_ANNOTATION_TEXT_WITH_CPL:
		case dcp::VerificationNote::Code::ALL_ENCRYPTED:
		case dcp::VerificationNote::Code::NONE_ENCRYPTED:
		case dcp::VerificationNote::Code::VALID_CONTENT_KIND:
		case dcp::VerificationNote::Code::VALID_MAIN_PICTURE_ACTIVE_AREA:
		case dcp::VerificationNote::Code::VALID_CONTENT_VERSION_LABEL_TEXT:
			/* These are all "OK" messages which we don't report here */
			break;
		case dcp::VerificationNote::Code::INVALID_PKL_NAMESPACE:
			add(i.second, _("A PKL has an invalid namespace %xml_namespace"));
			break;
		}
	}

	if (counts[dcp::VerificationNote::Type::ERROR] == 0) {
		auto grid = _pages[dcp::VerificationNote::Type::ERROR].grid;
		grid->AppendRows();
		grid->SetCellValue(grid->GetNumberRows() - 1, 0, _("No errors found."));
	}

	if (counts[dcp::VerificationNote::Type::BV21_ERROR] == 0) {
		auto grid = _pages[dcp::VerificationNote::Type::BV21_ERROR].grid;
		grid->AppendRows();
		grid->SetCellValue(grid->GetNumberRows() - 1, 0, _("No SMPTE Bv2.1 errors found."));
	}

	if (counts[dcp::VerificationNote::Type::WARNING] == 0) {
		auto grid = _pages[dcp::VerificationNote::Type::WARNING].grid;
		grid->AppendRows();
		grid->SetCellValue(grid->GetNumberRows() - 1, 0, _("No warnings found."));
	}

	return counts;
}


template <class T>
void save(wxWindow* parent, wxString filter, vector<shared_ptr<const VerifyDCPJob>> jobs)
{
	FileDialog dialog(parent, _("Verification report"), filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT, "SaveVerificationReport");
	if (!dialog.show()) {
		return;
	}

	T formatter(dialog.path());
	auto results = std::vector<dcp::VerificationResult>();
	for (auto job: jobs) {
		results.push_back(job->result());
	}
	dcp::verify_report(results, formatter);
}


void
VerifyDCPResultPanel::save_text_report()
{
	save<dcp::TextFormatter>(this, char_to_wx("Text files (*.txt)|*.txt"), _jobs);
}


void
VerifyDCPResultPanel::save_html_report()
{
	save<dcp::HTMLFormatter>(this, char_to_wx("HTML files (*.htm;*html)|*.htm;*.html"), _jobs);
}
