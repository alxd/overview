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

#include "Master.h"
#include "GameApp.h"
#include "GameEngine.h"
#include "GLUtil.h"


CGameEngine::CGameEngine()
{
	m_bShowTexture = false;

	//GetApp()->MessageBox((const char *)glGetString(GL_EXTENSIONS));
	GLUtil()->Init();
	m_fFont.Init(GetGameApp()->GetHDC());
	m_nPolygonMode = GL_FILL;
	m_3DCamera.SetPosition(CDoubleVector(0, 0, 25));
	m_vLight = CVector(1000, 1000, 1000);
	m_vLightDirection = m_vLight / m_vLight.Magnitude();
	CTexture::InitStaticMembers(238653, 256);

	m_nSamples = 4;		// Number of sample rays to use in integral equation
	m_Kr = 0.0025f;		// Rayleigh scattering constant
	m_Kr4PI = m_Kr*4.0f*PI;
	m_Km = 0.0025f;		// Mie scattering constant
	m_Km4PI = m_Km*4.0f*PI;
	m_ESun = 15.0f;		// Sun brightness constant
	m_g = -0.75f;		// The Mie phase asymmetry factor

	m_fInnerRadius = 10.0f;
	m_fOuterRadius = 10.15f;
	m_fScale = 1 / (m_fOuterRadius - m_fInnerRadius);

	m_fWavelength[0] = 0.650f;		// 650 nm for red
	m_fWavelength[1] = 0.570f;		// 570 nm for green
	m_fWavelength[2] = 0.475f;		// 475 nm for blue
	m_fWavelength4[0] = powf(m_fWavelength[0], 4.0f);
	m_fWavelength4[1] = powf(m_fWavelength[1], 4.0f);
	m_fWavelength4[2] = powf(m_fWavelength[2], 4.0f);

	m_fRayleighScaleDepth = 0.25f;
	m_fMieScaleDepth = 0.1f;
	m_pbOpticalDepth.MakeOpticalDepthBuffer(m_fInnerRadius, m_fOuterRadius, m_fRayleighScaleDepth, m_fMieScaleDepth);

	m_sphereInner.Init(m_fInnerRadius, 50, 50);
	m_sphereOuter.Init(m_fOuterRadius, 100, 100);
}

CGameEngine::~CGameEngine()
{
	GLUtil()->Cleanup();
}

void CGameEngine::SetColor(SVertex *pVertex)
{
	CVector vPos = pVertex->vPos;

	// Get the ray from the camera to the vertex, and its length (which is the far point of the ray passing through the atmosphere)
	CVector vCamera = m_3DCamera.GetPosition();
	CVector vRay = vPos - vCamera;
	float fFar = vRay.Magnitude();
	vRay /= fFar;

	// Calculate the closest intersection of the ray with the outer atmosphere (which is the near point of the ray passing through the atmosphere)
	float B = 2.0f * (vCamera | vRay);
	float Bsq = B * B;
	float C = (vCamera | vCamera) - m_fOuterRadius*m_fOuterRadius;
	float fDet = Max(0.0f, B*B - 4.0f * C);
	float fNear = 0.5f * (-B - sqrtf(fDet));

	bool bCameraInAtmosphere = false;
	bool bCameraAbove = true;
	float fCameraDepth[4] = {0, 0, 0, 0};
	float fLightDepth[4];
	float fSampleDepth[4];
	if(fNear <= 0)
	{
		// If the near point is behind the camera, it means the camera is inside the atmosphere
		bCameraInAtmosphere = true;
		fNear = 0;
		float fCameraHeight = vCamera.Magnitude();
		float fCameraAltitude = (fCameraHeight - m_fInnerRadius) * m_fScale;
		bCameraAbove = fCameraHeight >= vPos.Magnitude();
		float fCameraAngle = ((bCameraAbove ? -vRay : vRay) | vCamera) / fCameraHeight;
		m_pbOpticalDepth.Interpolate(fCameraDepth, fCameraAltitude, 0.5f - fCameraAngle * 0.5f);
	}
	else
	{
		// Otherwise, move the camera up to the near intersection point
		vCamera += vRay * fNear;
		fFar -= fNear;
		fNear = 0;
	}

	// If the distance between the points on the ray is negligible, don't bother to calculate anything
	if(fFar <= DELTA)
	{
		glColor4f(0, 0, 0, 1);
		return;
	}

	// Initialize a few variables to use inside the loop
	float fRayleighSum[3] = {0, 0, 0};
	float fMieSum[3] = {0, 0, 0};
	float fSampleLength = fFar / m_nSamples;
	float fScaledLength = fSampleLength * m_fScale;
	CVector vSampleRay = vRay * fSampleLength;

	// Start at the center of the first sample ray, and loop through each of the others
	vPos = vCamera + vSampleRay * 0.5f;
	for(int i=0; i<m_nSamples; i++)
	{
		float fHeight = vPos.Magnitude();

		// Start by looking up the optical depth coming from the light source to this point
		float fLightAngle = (m_vLightDirection | vPos) / fHeight;
		float fAltitude = (fHeight - m_fInnerRadius) * m_fScale;
		m_pbOpticalDepth.Interpolate(fLightDepth, fAltitude, 0.5f - fLightAngle * 0.5f);

		// If no light light reaches this part of the atmosphere, no light is scattered in at this point
		if(fLightDepth[0] < DELTA)
			continue;

		// Get the density at this point, along with the optical depth from the light source to this point
		float fRayleighDensity = fScaledLength * fLightDepth[0];
		float fRayleighDepth = fLightDepth[1];
		float fMieDensity = fScaledLength * fLightDepth[2];
		float fMieDepth = fLightDepth[3];

		// If the camera is above the point we're shading, we calculate the optical depth from the sample point to the camera
		// Otherwise, we calculate the optical depth from the camera to the sample point
		if(bCameraAbove)
		{
			float fSampleAngle = (-vRay | vPos) / fHeight;
			m_pbOpticalDepth.Interpolate(fSampleDepth, fAltitude, 0.5f - fSampleAngle * 0.5f);
			fRayleighDepth += fSampleDepth[1] - fCameraDepth[1];
			fMieDepth += fSampleDepth[3] - fCameraDepth[3];
		}
		else
		{
			float fSampleAngle = (vRay | vPos) / fHeight;
			m_pbOpticalDepth.Interpolate(fSampleDepth, fAltitude, 0.5f - fSampleAngle * 0.5f);
			fRayleighDepth += fCameraDepth[1] - fSampleDepth[1];
			fMieDepth += fCameraDepth[3] - fSampleDepth[3];
		}

		// Now multiply the optical depth by the attenuation factor for the sample ray
		fRayleighDepth *= m_Kr4PI;
		fMieDepth *= m_Km4PI;

		// Calculate the attenuation factor for the sample ray
		float fAttenuation[3];
		fAttenuation[0] = expf(-fRayleighDepth / m_fWavelength4[0] - fMieDepth);
		fAttenuation[1] = expf(-fRayleighDepth / m_fWavelength4[1] - fMieDepth);
		fAttenuation[2] = expf(-fRayleighDepth / m_fWavelength4[2] - fMieDepth);

		fRayleighSum[0] += fRayleighDensity * fAttenuation[0];
		fRayleighSum[1] += fRayleighDensity * fAttenuation[1];
		fRayleighSum[2] += fRayleighDensity * fAttenuation[2];

		fMieSum[0] += fMieDensity * fAttenuation[0];
		fMieSum[1] += fMieDensity * fAttenuation[1];
		fMieSum[2] += fMieDensity * fAttenuation[2];

		// Move the position to the center of the next sample ray
		vPos += vSampleRay;
	}

	// Calculate the angle and phase values (this block of code could be handled by a small 1D lookup table, or a 1D texture lookup in a pixel shader)
	float fAngle = -vRay | m_vLightDirection;
	float fPhase[2];
	float fAngle2 = fAngle*fAngle;
	float g2 = m_g*m_g;
	fPhase[0] = 0.75f * (1.0f + fAngle2);
	fPhase[1] = 1.5f * ((1 - g2) / (2 + g2)) * (1.0f + fAngle2) / powf(1 + g2 - 2*m_g*fAngle, 1.5f);
	fPhase[0] *= m_Kr * m_ESun;
	fPhase[1] *= m_Km * m_ESun;

	// Calculate the in-scattering color and clamp it to the max color value
	float fColor[3] = {0, 0, 0};
	fColor[0] = fRayleighSum[0] * fPhase[0] / m_fWavelength4[0] + fMieSum[0] * fPhase[1];
	fColor[1] = fRayleighSum[1] * fPhase[0] / m_fWavelength4[1] + fMieSum[1] * fPhase[1];
	fColor[2] = fRayleighSum[2] * fPhase[0] / m_fWavelength4[2] + fMieSum[2] * fPhase[1];
	fColor[0] = Min(fColor[0], 1.0f);
	fColor[1] = Min(fColor[1], 1.0f);
	fColor[2] = Min(fColor[2], 1.0f);

	// Last but not least, set the color
	pVertex->cColor = CColor(fColor[0], fColor[1], fColor[2]);
}

void CGameEngine::RenderFrame(int nMilliseconds)
{
	int i;
	// Determine the FPS
	static char szFrameCount[20] = {0};
	static int nTime = 0;
	static int nFrames = 0;
	nTime += nMilliseconds;
	if(nTime >= 1000)
	{
		m_fFPS = (float)(nFrames * 1000) / (float)nTime;
		sprintf(szFrameCount, "%2.2f FPS", m_fFPS);
		nTime = nFrames = 0;
	}
	nFrames++;

	// Move the camera
	HandleInput(nMilliseconds * 0.001f);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPushMatrix();
	glLoadMatrixf(m_3DCamera.GetViewMatrix());
	glLightfv(GL_LIGHT0, GL_POSITION, CVector4(m_vLight.x, m_vLight.y, m_vLight.z, 1));
	glDisable(GL_LIGHTING);

	C3DObject obj;
	glMultMatrixf(obj.GetModelMatrix(&m_3DCamera));

	if(m_bShowTexture)
	{
		CTexture t(&m_pbOpticalDepth);
		t.Enable();
		glBegin(GL_QUADS);
		glTexCoord2f(0, 0);
		glVertex3f(-3.0f, 3.0f, 0.0f);
		glTexCoord2f(0, 1);
		glVertex3f(-3.0f, -3.0f, 0.0f);
		glTexCoord2f(1, 1);
		glVertex3f(3.0f, -3.0f, 0.0f);
		glTexCoord2f(1, 0);
		glVertex3f(3.0f, 3.0f, 0.0f);
		glEnd();
		t.Disable();
	}
	else
	{
		// Update the color for the vertices of each sphere
		CVector vCamera = m_3DCamera.GetPosition();
		SVertex *pBuffer = m_sphereInner.GetVertexBuffer();
		for(int i=0; i<m_sphereInner.GetVertexCount(); i++)
		{
			if((vCamera | pBuffer[i].vPos) > 0)		// Cheap optimization: Don't update vertices on the back half of the sphere
				SetColor(&pBuffer[i]);
		}
		pBuffer = m_sphereOuter.GetVertexBuffer();
		for(i=0; i<m_sphereOuter.GetVertexCount(); i++)
		{
			if((vCamera | pBuffer[i].vPos) > 0)		// Cheap optimization: Don't update vertices on the back half of the sphere
				SetColor(&pBuffer[i]);
		}

		// Then draw the two spheres
		m_sphereInner.Draw();
		glFrontFace(GL_CW);
		m_sphereOuter.Draw();
		glFrontFace(GL_CCW);
	}

	glPopMatrix();
	glEnable(GL_LIGHTING);

	// Draw info in the top-left corner
	char szBuffer[256];
	m_fFont.Begin();
	glColor3d(1.0, 1.0, 1.0);
	m_fFont.SetPosition(0, 0);
	m_fFont.Print(szFrameCount);
	m_fFont.SetPosition(0, 15);
	sprintf(szBuffer, "Samples (+/-): %d", m_nSamples);
	m_fFont.Print(szBuffer);
	m_fFont.SetPosition(0, 30);
	sprintf(szBuffer, "Kr (F5/Sh+F5): %-4.4f", m_Kr);
	m_fFont.Print(szBuffer);
	m_fFont.SetPosition(0, 45);
	sprintf(szBuffer, "Km (F6/Sh+F6): %-4.4f", m_Km);
	m_fFont.Print(szBuffer);
	m_fFont.SetPosition(0, 60);
	sprintf(szBuffer, "g (F7/Sh+F7): %-2.2f", m_g);
	m_fFont.Print(szBuffer);
	m_fFont.SetPosition(0, 75);
	sprintf(szBuffer, "ESun (F8/Sh+F8): %-1.1f", m_ESun);
	m_fFont.Print(szBuffer);
	m_fFont.SetPosition(0, 90);
	sprintf(szBuffer, "Red (F9/Sh+F9): %-3.3f", m_fWavelength[0]);
	m_fFont.Print(szBuffer);
	m_fFont.SetPosition(0, 105);
	sprintf(szBuffer, "Green (F10/Sh+F10): %-3.3f", m_fWavelength[1]);
	m_fFont.Print(szBuffer);
	m_fFont.SetPosition(0, 120);
	sprintf(szBuffer, "Blue (F11/Sh+F11): %-3.3f", m_fWavelength[2]);
	m_fFont.Print(szBuffer);
	m_fFont.End();
	glFlush();
}

void CGameEngine::OnChar(WPARAM c)
{
	switch(c)
	{
		case 'p':
			m_nPolygonMode = (m_nPolygonMode == GL_FILL) ? GL_LINE : GL_FILL;
			glPolygonMode(GL_FRONT, m_nPolygonMode);
			break;
		case 't':
			m_bShowTexture = !m_bShowTexture;
			break;
		case '+':
			m_nSamples++;
			break;
		case '-':
			m_nSamples--;
			break;
	}
}

void CGameEngine::HandleInput(float fSeconds)
{
	if((GetKeyState(VK_F5) & 0x8000))
	{
		if((GetKeyState(VK_SHIFT) & 0x8000))
			m_Kr = Max(0.0f, m_Kr - 0.0001f);
		else
			m_Kr += 0.0001f;
		m_Kr4PI = m_Kr*4.0f*PI;
	}
	else if((GetKeyState(VK_F6) & 0x8000))
	{
		if((GetKeyState(VK_SHIFT) & 0x8000))
			m_Km = Max(0.0f, m_Km - 0.0001f);
		else
			m_Km += 0.0001f;
		m_Km4PI = m_Km*4.0f*PI;
	}
	else if((GetKeyState(VK_F7) & 0x8000))
	{
		if((GetKeyState(VK_SHIFT) & 0x8000))
			m_g = Max(-1.0f, m_g-0.01f);
		else
			m_g = Min(1.0f, m_g+0.01f);
	}
	else if((GetKeyState(VK_F8) & 0x8000))
	{
		if((GetKeyState(VK_SHIFT) & 0x8000))
			m_ESun = Max(0.0f, m_ESun - 0.1f);
		else
			m_ESun += 0.1f;
	}
	else if((GetKeyState(VK_F9) & 0x8000))
	{
		if((GetKeyState(VK_SHIFT) & 0x8000))
			m_fWavelength[0] = Max(0.001f, m_fWavelength[0] -= 0.001f);
		else
			m_fWavelength[0] += 0.001f;
		m_fWavelength4[0] = powf(m_fWavelength[0], 4.0f);
	}
	else if((GetKeyState(VK_F10) & 0x8000))
	{
		if((GetKeyState(VK_SHIFT) & 0x8000))
			m_fWavelength[1] = Max(0.001f, m_fWavelength[1] -= 0.001f);
		else
			m_fWavelength[1] += 0.001f;
		m_fWavelength4[1] = powf(m_fWavelength[1], 4.0f);
	}
	else if((GetKeyState(VK_F11) & 0x8000))
	{
		if((GetKeyState(VK_SHIFT) & 0x8000))
			m_fWavelength[2] = Max(0.001f, m_fWavelength[2] -= 0.001f);
		else
			m_fWavelength[2] += 0.001f;
		m_fWavelength4[2] = powf(m_fWavelength[2], 4.0f);
	}


	const float ROTATE_SPEED = 1.0f;

	// Turn left/right means rotate around the up axis
	if((GetKeyState(VK_NUMPAD6) & 0x8000) || (GetKeyState(VK_RIGHT) & 0x8000))
		m_3DCamera.Rotate(m_3DCamera.GetUpAxis(), fSeconds * -ROTATE_SPEED);
	if((GetKeyState(VK_NUMPAD4) & 0x8000) || (GetKeyState(VK_LEFT) & 0x8000))
		m_3DCamera.Rotate(m_3DCamera.GetUpAxis(), fSeconds * ROTATE_SPEED);

	// Turn up/down means rotate around the right axis
	if((GetKeyState(VK_NUMPAD8) & 0x8000) || (GetKeyState(VK_UP) & 0x8000))
		m_3DCamera.Rotate(m_3DCamera.GetRightAxis(), fSeconds * -ROTATE_SPEED);
	if((GetKeyState(VK_NUMPAD2) & 0x8000) || (GetKeyState(VK_DOWN) & 0x8000))
		m_3DCamera.Rotate(m_3DCamera.GetRightAxis(), fSeconds * ROTATE_SPEED);

	// Roll means rotate around the view axis
	if(GetKeyState(VK_NUMPAD7) & 0x8000)
		m_3DCamera.Rotate(m_3DCamera.GetViewAxis(), fSeconds * -ROTATE_SPEED);
	if(GetKeyState(VK_NUMPAD9) & 0x8000)
		m_3DCamera.Rotate(m_3DCamera.GetViewAxis(), fSeconds * ROTATE_SPEED);

#define THRUST		1.0f	// Acceleration rate due to thrusters (units/s*s)
#define RESISTANCE	0.1f	// Damping effect on velocity

	// Handle acceleration keys
	CVector vAccel(0.0f);
	if(GetKeyState(VK_SPACE) & 0x8000)
		m_3DCamera.SetVelocity(CVector(0.0f));	// Full stop
	else
	{
		// Add camera's acceleration due to thrusters
		float fThrust = THRUST;
		if(GetKeyState(VK_CONTROL) & 0x8000)
			fThrust *= 10.0f;

		// Thrust forward/reverse affects velocity along the view axis
		if(GetKeyState('W') & 0x8000)
			vAccel += m_3DCamera.GetViewAxis() * fThrust;
		if(GetKeyState('S') & 0x8000)
			vAccel += m_3DCamera.GetViewAxis() * -fThrust;

		// Thrust left/right affects velocity along the right axis
		if(GetKeyState('D') & 0x8000)
			vAccel += m_3DCamera.GetRightAxis() * fThrust;
		if(GetKeyState('A') & 0x8000)
			vAccel += m_3DCamera.GetRightAxis() * -fThrust;

		m_3DCamera.Accelerate(vAccel, fSeconds, RESISTANCE);
		CVector vPos = m_3DCamera.GetPosition();
		float fMagnitude = vPos.Magnitude();
		if(fMagnitude < m_fInnerRadius)
		{
			vPos *= (m_fInnerRadius * (1 + DELTA)) / fMagnitude;
			m_3DCamera.SetPosition(CDoubleVector(vPos.x, vPos.y, vPos.z));
			m_3DCamera.SetVelocity(-m_3DCamera.GetVelocity());
		}
	}
}

