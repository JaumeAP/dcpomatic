/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "metal_video_view.h"
#include <dcp/warnings.h>

LIBDCP_DISABLE_WARNINGS
#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define MTK_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include <AppKit/AppKit.hpp>
#include <MetalKit/MetalKit.hpp>
LIBDCP_ENABLE_WARNINGS


class ViewDelegate : public MTK::ViewDelegate
{
public:
	ViewDelegate(MTL::Device* device)
		: _device(device)
	{}

	virtual void drawInMTKView(MTK::View* view) override
	{
		std::cout << "AWOOGA\n";
	}

private:
	MTL::Device* _device;
};


MetalVideoView::MetalVideoView(FilmViewer* viewer, wxWindow* parent)
	: VideoView(viewer, false)
	, _canvas(new wxWindow(parent, wxID_ANY))
{
	_device = NS::TransferPtr(MTLCreateSystemDefaultDevice());
	_view = MTK::View::alloc()->init(CGRect{{0, 0}, {512, 512}}, _device);
	_view->setColorPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);
	_view->setClearColor(MTL::ClearColor::Make(1.0, 0.0, 0.0, 1.0));
	_delegate = new ViewDelegate(_device);
	_view->setDelegate(_delegate);

	set_layer(_canvas->GetHandle(), _view->currentDrawable()->layer());
	std::cout << "Create MetalVideoView\n";
}


MetalVideoView::~MetalVideoView()
{
	_device->release();
}


void
MetalVideoView::update()
{
	std::cout << "MetalVideoView::update()\n";
}


void
MetalVideoView::start()
{
	std::cout << "MetalVideoView::start()\n";
}


void
MetalVideoView::stop()
{
	std::cout << "MetalVideoView::stop()\n";
}


VideoView::NextFrameResult
MetalVideoView::display_next_frame(bool non_blocking)
{
	std::cout << "MetalVideoView::display_next_frame()\n";
}

