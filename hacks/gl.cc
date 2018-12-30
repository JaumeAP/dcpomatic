#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <boost/bind.hpp>

class GLView : public wxGLCanvas
{
public:
	GLView (wxFrame* parent);

private:
	void paint (wxPaintEvent& event);
        void Render();
};

GLView::GLView(wxFrame *parent)
	: wxGLCanvas(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, wxT("GLCanvas"))
{
	Bind (wxEVT_PAINT, boost::bind(&GLView::paint, this, _1));
}

void GLView::paint (wxPaintEvent &)
{
	SetCurrent();
	wxPaintDC(this);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glViewport(0, 0, (GLint)GetSize().x, (GLint)GetSize().y);

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
	glTexCoord2i(0, 0); glVertex2f(-0.5, -0.5);
	glTexCoord2i(0, 1); glVertex2f(-0.5, 0.5);
	glTexCoord2i(1, 1); glVertex2f(0.5, 0.5);
	glTexCoord2i(1, 0); glVertex2f(0.5, -0.5);
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
