#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <boost/bind.hpp>

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

	//create test checker image
	unsigned char texDat[64];
	for (int i = 0; i < 64; ++i)
		texDat[i] = ((i + (i / 8)) % 2) * 128 + 127;

	//upload to GPU texture
	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, 8, 8, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, texDat);
	glBindTexture(GL_TEXTURE_2D, 0);

	//match projection to window resolution (could be in reshape callback)
//    glMatrixMode(GL_PROJECTION);
//    glOrtho(0, 800, 0, 600, -1, 1);
//    glMatrixMode(GL_MODELVIEW);

	glClear(GL_COLOR_BUFFER_BIT);
	glColor3f(1, 1, 1);
	glBindTexture(GL_TEXTURE_2D, tex);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	glTexCoord2i(0, 0); glVertex2f(0, 0);
	glTexCoord2i(0, 1); glVertex2f(0, 100);
	glTexCoord2i(1, 1); glVertex2f(100, 100);
	glTexCoord2i(1, 0); glVertex2f(100, 0);
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
