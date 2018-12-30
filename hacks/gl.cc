#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <boost/bind.hpp>
#include <iostream>

using std::cout;

class GLView : public wxGLCanvas
{
public:
	GLView (wxFrame* parent);
	~GLView ();

private:
	void paint (wxPaintEvent& event);

	wxGLContext* _context;
};

GLView::GLView(wxFrame *parent)
	: wxGLCanvas (parent, wxID_ANY, 0)
{
	_context = new wxGLContext (this);
	Bind (wxEVT_PAINT, boost::bind(&GLView::paint, this, _1));
}

GLView::~GLView ()
{
	delete _context;
}

void GLView::paint (wxPaintEvent &)
{
	SetCurrent (*_context);
	wxPaintDC (this);
	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor (0.0f, 0.0f, 0.0f, 1.0f);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glViewport (0, 0, GetSize().x, GetSize().y);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho (0, GetSize().x, GetSize().y, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	int const width = 1998;
	int const height = 1080;
	unsigned char image[width * height * 3];
	unsigned char* p = image;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			if (((y / 16) % 3) == 0) {
				*p++ = char(x * 256 / width);
			} else {
				*p++ = 0;
			}
			if (((y / 16) % 3) == 1) {
				*p++ = char(x * 256 / width);
			} else {
				*p++ = 0;
			}
			if (((y / 16) % 3) == 2) {
				*p++ = char(x * 256 / width);
			} else {
				*p++ = 0;
			}
		}
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	//upload to GPU texture
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	GLenum e = glGetError ();
	if (e != GL_NO_ERROR) {
		cout << "something else failed " << e << "\n";
	}
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	e = glGetError ();
	if (e != GL_NO_ERROR) {
		cout << "glTexImage2D failed " << e << " ";
		if (e == GL_INVALID_VALUE) {
			cout << "invalid value\n";
		} else if (e == GL_INVALID_ENUM) {
			cout << "invalid enum\n";
		} else {
			cout << "unknown\n";
		}
	}
	glBindTexture(GL_TEXTURE_2D, 0);

	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1, 1, 1);
	glBindTexture(GL_TEXTURE_2D, tex);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2i(0, 0); glVertex2i(0, 0);
	glTexCoord2i(0, 1); glVertex2i(0, height);
	glTexCoord2i(1, 1); glVertex2i(width, height);
	glTexCoord2i(1, 0); glVertex2i(width, 0);
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	glFlush();
	SwapBuffers();
}

class MyApp : public wxApp
{
	bool OnInit()
	{
		wxFrame *frame = new wxFrame (0, -1, wxT("Hello GL World"), wxPoint(50,50), wxSize(200,200));
		new GLView(frame);

		frame->Show (true);
		return true;
	}
};

IMPLEMENT_APP(MyApp)
