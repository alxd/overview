/*******************************************************************************
*                                                                              *
*   PrimeSense NiTE 2.0 - User Viewer Sample                                   *
*   Copyright (C) 2012 PrimeSense Ltd.                                         *
*                                                                              *
*******************************************************************************/

#ifndef _NITE_USER_VIEWER_H_
#define _NITE_USER_VIEWER_H_

#include "../includeNite/NiTE.h"

#define MAX_DEPTH 10000

class SampleViewer
{
public:
	SampleViewer(const char* strSampleName);
	virtual ~SampleViewer();

	virtual openni::Status Init(int argc, char **argv);
	virtual openni::Status Run();	//Does not return

public:
	virtual void Display();
	virtual void DisplayPostDraw(){};	// Overload to draw over the screen image

	virtual void OnKey(unsigned char key, int x, int y);

	virtual openni::Status InitOpenGL(int argc, char **argv);
	void InitOpenGLHooks();

	void Finalize();
	void SampleViewer::SetRes(openni::VideoFrameRef depthFrame );
	void DrawStatusLabel(nite::UserTracker* pUserTracker, const nite::UserData& user);
	void DrawCenterOfMass(nite::UserTracker* pUserTracker, const nite::UserData& user);
	void DrawBoundingBox(const nite::UserData& user);
	void DrawSkeleton(nite::UserTracker* pUserTracker, const nite::UserData& userData);
	void DrawLimb(nite::UserTracker* pUserTracker, const nite::SkeletonJoint& joint1, const nite::SkeletonJoint& joint2, int color);

public:
	SampleViewer(const SampleViewer&);
	SampleViewer& operator=(SampleViewer&);
	SampleViewer();
	char m_error[300];
	nite::UserTracker* m_pUserTracker;
	void updateUserState(const nite::UserData& user, uint64_t ts);

private:	
	static SampleViewer* ms_self;
	static void glutIdle();
	static void glutDisplay();
	static void glutKeyboard(unsigned char key, int x, int y);

	float				m_pDepthHist[MAX_DEPTH];
	char			m_strSampleName[ONI_MAX_STR];
	openni::RGB888Pixel*		m_pTexMap;
	unsigned int		m_nTexMapX;
	unsigned int		m_nTexMapY;

	openni::Device		m_device;

	nite::UserId m_poseUser;
	uint64_t m_poseTime;
};


#endif // _NITE_USER_VIEWER_H_
