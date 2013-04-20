/*
s_p_oneil@hotmail.com
Copyright (c) 2000, Sean O'Neil
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.
* Neither the name of this project nor the names of its contributors
  may be used to endorse or promote products derived from this software
  without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __GameEngine_h__
#define __GameEngine_h__

#include "GLUtil.h"
#include "GameApp.h"
#include "Texture.h"
#include "Font.h"
#include "Viewer.h"


#define SAMPLE_SIZE		5

struct SVertex
{
	CVector vPos;
	CColor cColor;
};

class CSphere
{
protected:
	float m_fRadius;
	int m_nSlices;
	int m_nSections;

	SVertex *m_pVertex;
	unsigned short m_nVertices;

public:
	CSphere()	{}

	int GetVertexCount()		{ return m_nVertices; }
	SVertex *GetVertexBuffer()	{ return m_pVertex; }
	int i;

	void Init(float fRadius, int nSlices, int nSections)
	{
		
		m_fRadius = fRadius;
		m_nSlices = nSlices;
		m_nSections = nSections;

		m_nVertices = nSlices * (nSections-1) + 2;
		m_pVertex = new SVertex[m_nVertices];

		float fSliceArc = 2*PI / nSlices;
		float fSectionArc = PI / nSections;
		float *fRingz = new float[nSections+1];
		float *fRingSize = new float[nSections+1];
		float *fRingx = new float[nSlices+1];
		float *fRingy = new float[nSlices+1];
		for(int i=0; i<=nSections; i++)
		{
			fRingz[i] = cosf(fSectionArc * i);
			fRingSize[i] = sinf(fSectionArc * i);
		}
		for(i=0; i<=nSlices; i++)
		{
			fRingx[i] = cosf(fSliceArc * i);
			fRingy[i] = sinf(fSliceArc * i);
		}

		int nIndex = 0;
		m_pVertex[nIndex++].vPos = CVector(0, 0, fRadius);
		for(int j=1; j<nSections; j++)
		{
			for(int i=0; i<nSlices; i++)
			{
				CVector v;
				v.x = fRingx[i] * fRingSize[j];
				v.y = fRingy[i] * fRingSize[j];
				v.z = fRingz[j];
				v *= fRadius / v.Magnitude();
				m_pVertex[nIndex++].vPos = v;
			}
		}

		m_pVertex[nIndex++].vPos = CVector(0, 0, -fRadius);
	}

	void Draw()
	{
		int i;
		glBegin(GL_TRIANGLE_FAN);
		glColor4ubv(m_pVertex[0].cColor);
		glVertex3fv(m_pVertex[0].vPos);
		for(i=0; i<m_nSlices; i++)
		{
			glColor4ubv(m_pVertex[i+1].cColor);
			glVertex3fv(m_pVertex[i+1].vPos);
		}
		glColor4ubv(m_pVertex[1].cColor);
		glVertex3fv(m_pVertex[1].vPos);
		glEnd();

		int nIndex1 = 1;
		int nIndex2 = 1 + m_nSlices;
		for(int j=1; j<m_nSections-1; j++)
		{
			glBegin(GL_TRIANGLE_STRIP);
			for(int i=0; i<m_nSlices; i+=2)
			{
				glColor4ubv(m_pVertex[nIndex1+i].cColor);
				glVertex3fv(m_pVertex[nIndex1+i].vPos);
				glColor4ubv(m_pVertex[nIndex2+i].cColor);
				glVertex3fv(m_pVertex[nIndex2+i].vPos);
				glColor4ubv(m_pVertex[nIndex1+1+i].cColor);
				glVertex3fv(m_pVertex[nIndex1+1+i].vPos);
				glColor4ubv(m_pVertex[nIndex2+1+i].cColor);
				glVertex3fv(m_pVertex[nIndex2+1+i].vPos);
			}
			glColor4ubv(m_pVertex[nIndex1].cColor);
			glVertex3fv(m_pVertex[nIndex1].vPos);
			glColor4ubv(m_pVertex[nIndex2].cColor);
			glVertex3fv(m_pVertex[nIndex2].vPos);
			glEnd();
			nIndex1 += m_nSlices;
			nIndex2 += m_nSlices;
		}


		glBegin(GL_TRIANGLE_FAN);
		glColor4ubv(m_pVertex[m_nVertices-1].cColor);
		glVertex3fv(m_pVertex[m_nVertices-1].vPos);
		for(i=0; i<m_nSlices; i++)
		{
			glColor4ubv(m_pVertex[m_nVertices-2-i].cColor);
			glVertex3fv(m_pVertex[m_nVertices-2-i].vPos);
		}
		glColor4ubv(m_pVertex[m_nVertices-2].cColor);
		glVertex3fv(m_pVertex[m_nVertices-2].vPos);
		glEnd();
	}
};


class CGameEngine
{
protected:
	float m_fFPS;
	int m_nTime;
	CFont m_fFont;

	C3DObject m_3DCamera;
	CVector m_vLight;
	CVector m_vLightDirection;
	
	// Variables that can be tweaked with keypresses
	bool m_bShowTexture;
	int m_nSamples;
	GLenum m_nPolygonMode;
	float m_Kr, m_Kr4PI;
	float m_Km, m_Km4PI;
	float m_ESun;
	float m_g;

	float m_fInnerRadius;
	float m_fOuterRadius;
	float m_fScale;
	float m_fWavelength[3];
	float m_fWavelength4[3];
	float m_fRayleighScaleDepth;
	float m_fMieScaleDepth;
	CPixelBuffer m_pbOpticalDepth;

	CSphere m_sphereInner;
	CSphere m_sphereOuter;
	SampleViewer * sampleViewer;

	bool initial, headFront, headBack, headLeft, headRight, handLeft, handRight;
	int skip, jointIdx;

	nite::SkeletonJoint jointHistoryH[1000], jointHistoryRH[1000];
	float initialH_x, initialRH_x;
	float initialH_z, initialRH_z;


public:
	CGameEngine(SampleViewer * s);
	~CGameEngine();
	void RenderFrame(int nMilliseconds);
	void Pause()	{}
	void Restore()	{}
	void HandleInput(float fSeconds);
	void OnChar(WPARAM c);

	void SetColor(SVertex *pVertex);
};

#endif // __GameEngine_h__
