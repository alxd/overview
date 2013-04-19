// GLUtil.cpp
//

#include "Master.h"
#include "GLUtil.h"

CGLUtil g_glUtil;
CGLUtil *CGLUtil::m_pMain = &g_glUtil;

CGLUtil::CGLUtil()
{
	// Start by clearing out all the member variables
}

CGLUtil::~CGLUtil()
{
}

void CGLUtil::Init()
{
	// Start by storing the current HDC and HGLRC
	m_hDC = wglGetCurrentDC();
	m_hGLRC = wglGetCurrentContext();

	// Finally, initialize the default rendering context
	InitRenderContext(m_hDC, m_hGLRC);
}

void CGLUtil::Cleanup()
{
}

void CGLUtil::InitRenderContext(HDC hDC, HGLRC hGLRC)
{
	wglMakeCurrent(hDC, hGLRC);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, CVector4(0.0f));

	wglMakeCurrent(m_hDC, m_hGLRC);
}
