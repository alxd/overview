// GLUtil.h
//

#ifndef __GLUtil_h__
#define __GLUtil_h__

#define BUFFER_OFFSET(i) ((char *) NULL + (i))

#include "Texture.h"


class CGLUtil
{
// Attributes
protected:
	// Standard OpenGL members
	HDC m_hDC;
	HGLRC m_hGLRC;

	// Members for GL_ARB_vertex_buffer_object
	unsigned int m_nVertexBuffer;

public:
	static CGLUtil *m_pMain;

// Operations
public:
	CGLUtil();
	~CGLUtil();
	void Init();
	void Cleanup();
	void InitRenderContext(HDC hDC=NULL, HGLRC hGLRC=NULL);

	HDC GetHDC()					{ return m_hDC; }
	HGLRC GetHGLRC()				{ return m_hGLRC; }


	void BeginOrtho2D(int nWidth=640, int nHeight=480)
	{
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_LIGHTING);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluOrtho2D(0, nWidth, 0, nHeight);
	}
	void EndOrtho2D()
	{
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glEnable(GL_LIGHTING);
		glEnable(GL_DEPTH_TEST);
	}
};

inline CGLUtil *GLUtil()			{ return CGLUtil::m_pMain; }

#endif // __GLUtil_h__
