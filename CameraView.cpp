// CameraView.cpp : implementation of the CCameraView class
//

#include "stdafx.h"

#include <afxbutton.h>

//#include "VisionTool.h"
#include "Resource.h"
#include "CameraView.h"
#include "CameraPaneWnd.h"

#include "misc/StopWatch.h"
#include "xMathUtil/xMathUtil.h"
#include "misc/MemDC.h"

#include "MessageDlg.h"

#define _USE_MATH_DEFINES
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern CIPClient* s_pIPClient;
extern CProfile* s_pProfile;
extern CSimpleLog* s_pLog;

IMPLEMENT_DYNAMIC_CREATE(CCameraView)

//-----------------------------------------------------------------------------
// CCameraView::MOUSE_ACTION
//
void CCameraView::CMouseAction::Init() {
	//pThis = NULL;		// pThis 에 NULL 을 넣으면 안됨!
	eMouseMode = MM_NONE;
	ptsStage.DeleteAll();
	ptsImage.DeleteAll();
	if (pIPCmd) {
		if (s_pIPClient && s_pIPClient->IsValidCmd(pIPCmd))
			s_pIPClient->ContinueIPStepCommand(pIPCmd, IPPS_STOP);
	}
	pIPCmd = NULL;
	bHideAttr = FALSE;
	if (GetCapture() == pThis)
		ReleaseCapture();

	bShowText = FALSE;
	crText = RGB(0, 0, 0);
	ptText.SetPoint(0, 0);
	font.DeleteObject();
	strText.Empty();

	bRubberBand = FALSE;
	if (wndTrack.m_hWnd)
		wndTrack.ShowWindow(SW_HIDE);
}
BOOL CCameraView::SavePositionToVar(CIPVar& var, const TList<CPoint2d>* pptsStage, const TList<CPoint2d>* pptsImage, cv::Mat* pImg) {
#define _SAVE_PTS(F_NUMBER, F_SIGN_X, F_SIGN_Y, pts)\
	{\
		CPoint2d pt;\
		var.SetChildItem(F_NUMBER, pts.N());\
		for (int i = 0; i < pts.N(); i++) {\
			pt += pts[i];\
			var.SetChildItem(FormatA(F_SIGN_X "%d", i), pts[i].x);\
			var.SetChildItem(FormatA(F_SIGN_Y "%d", i), pts[i].y);\
		}\
		if (pts.N()) {\
			if (pts.N() != 1) {\
				pt.x /= pts.N();\
				pt.y /= pts.N();\
			}\
			var.SetChildItem(F_SIGN_X, pt.x);\
			var.SetChildItem(F_SIGN_Y, pt.y);\
		}\
	}\

	CPoint2d ptCenterOffset = m_ptCenterOffset[GetCurrentLens()];
	if (pptsStage && pptsStage->N()) {
		TList<CPoint2d> ptsStage = *pptsStage;
		_SAVE_PTS("nPtsMachine", F_MACHINE_X, F_MACHINE_Y, ptsStage);
	}

	if (pptsImage && pptsImage->N()) {
		TList<CPoint2d> ptsImage = *pptsImage;
		_SAVE_PTS("nPtsImage", F_IMAGE_X, F_IMAGE_Y, ptsImage);

		CPoint2d pt = m_ptCenterOffset[GetCurrentLens()];
		CPoint2d ptOffset(m_sizeCamera.width/2+pt.x, m_sizeCamera.height/2+pt.y);
		for (int i = 0; i < ptsImage.N(); i++)
			ptsImage[i] -= ptOffset;
		_SAVE_PTS("nPtsLaserImage", F_LASER_IMAGE_X, F_LASER_IMAGE_Y, ptsImage);
	}

	if (pImg && !pImg->empty()) {
		var.AddChildItemMat(*pImg, "matImage");
	}
#undef _SAVE_PTS
	return TRUE;
}

BOOL CCameraView::SavePositionToVar(CIPVar& var, const CPoint2d* pptStage, const CPoint2d* pptImage, cv::Mat* pImg) {
	TList<CPoint2d> ptsStage;
	if (pptStage)
		ptsStage.Attach(new CPoint2d(*pptStage));

	TList<CPoint2d> ptsImage;
	if (pptImage)
		ptsImage.Attach(new CPoint2d(*pptImage));

	return SavePositionToVar(var, &ptsStage, &ptsImage, pImg);
}

BOOL CCameraView::CMouseAction::NotifyIP(cv::Mat* pImg)  {
	if (!pThis) return FALSE;
	if (!pIPCmd || !pThis->m_pIPClient->IsValidCmd(pIPCmd))
		return FALSE;

	pThis->SavePositionToVar(pIPCmd->m_varResult, &ptsStage, &ptsImage, pImg);

	pThis->ContinueIPStepCommand(pIPCmd);
	return TRUE;
}



//-----------------------------------------------------------------------------
// CCameraView::CDrawObject
//
HCURSOR CCameraView::CDrawObject::hCursor = NULL;
void CCameraView::CDrawObject::Init() {
	eDrawMode = DM_NONE;
	dThickness = 0.100;
	crObject = RGB(255, 0, 0);
	eShape = CShapeObject::S_NONE;
	rObjectCurrent.Release();
	rObjectEdit.Release();
	pptStageCurrent = NULL;
	ptsStage.clear();
	ptStage.SetPointAll(0);
	group.m_objects.DeleteAll();
	if (!hCursor) {
		if (s_pIPClient) {
			CString strPath = s_pIPClient->GetAbsPath(_T("..\\icon\\vision.size.cur"));
			hCursor = ::LoadCursorFromFile(strPath);
		}
	}
	if (!hCursor) {
		extern HINSTANCE g_hInstance;
		hCursor = LoadCursor(g_hInstance, _T("IDC_CURSOR_MOVE"));
	}
}

BOOL CCameraView::CDrawObject::OnMouse(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen, double& dMinDistance, TRefPointer<CShapeObject>& rObjectEdit, CPoint2d** ppPoints) {
	for (int i = 0; i < group.m_objects.size(); i++) {
		TRefPointer<CShapeObject> rObject = group.m_objects.GetItem(i);
		switch (group.m_objects[i].GetShape()) {
		case CShapeObject::S_LINE :
		case CShapeObject::S_POLY_LINE :
			{
				TLineD* pLine = &((CShapeLine*)(CShapeObject*)rObject)->m_pts;
				if (pLine) {
					for (int i = 0; i < pLine->size(); i++) {
						double dDistance = (*pLine)[i].Distance(ptStage);
						if (dDistance < dMinDistance) {
							if (rObjectEdit.GetPointer() != rObject.GetPointer())
								rObjectEdit = rObject;
							if (ppPoints)
								*ppPoints = &(*pLine)[i];
							dMinDistance = dDistance;
						}
					}
				}
			}
			break;
		}
	}
	return rObjectEdit.GetPointer() ? TRUE : FALSE;
}
BOOL CCameraView::CDrawObject::OnLButtonDown(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen) {
	CPoint2d ptStage = ctScreenToStage(CPoint2d(ptScreen));
	if (!rObjectEdit && group.m_objects.size()) {
		double dMinDistance = ctScreenToStage.Trans(5.0);	// 5 pixels -> mm

		if (OnMouse(ctScreenToStage, ptScreen, dMinDistance, rObjectEdit, &pptStageCurrent))
			return TRUE;
	}

	if (eDrawMode == DM_NONE)
		return FALSE;

	if ( (eDrawMode == DM_SINGLE) || (eDrawMode == DM_MULTI) ) {
		if (rObjectCurrent) {
		} else {
			if (eDrawMode == DM_SINGLE)
				group.m_objects.DeleteAll();
			switch (eShape) {
			case CShapeObject::S_LINE :			rObjectCurrent = new CShapeLine(crObject); break;
			case CShapeObject::S_POLY_LINE :	rObjectCurrent = new CShapePolyLine(crObject); break;
			//case CShapeObject::S_CIRCLE :		rObjectCurrent = new CShapeCircle(crObject); break;
			}
			if (rObjectCurrent)
				rObjectCurrent->SetHatching(SH_NONE, dThickness);
		}
		if (rObjectCurrent) {
			ptsStage.push_back(ptStage);
			rObjectCurrent->SetFromPoints(ptsStage);

			BOOL bComplete = FALSE;
			switch (rObjectCurrent->GetShape()) {
			case CShapeObject::S_LINE :			bComplete = ptsStage.size() >= 2; break;
			case CShapeObject::S_POLY_LINE :	bComplete = FALSE; break;
			}

			if (bComplete)
				AddCurrentObject();
		}
	}

	return TRUE;
}
BOOL CCameraView::CDrawObject::OnLButtonUp(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen) {
	if (!rObjectEdit)
		return FALSE;
	pptStageCurrent = NULL;
	rObjectEdit.Release();
	return TRUE;
}
//BOOL CCameraView::CDrawObject::OnRButtonDown(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen) { return FALSE; }
//BOOL CCameraView::CDrawObject::OnRButtonUp(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen) { return FALSE; }
BOOL CCameraView::CDrawObject::OnMouseMove(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen) {
	ptStage = ctScreenToStage(CPoint2d(ptScreen));
	if (pptStageCurrent)
		*pptStageCurrent = ptStage;
	return FALSE;
}
BOOL CCameraView::CDrawObject::OnKeyDown(int eKey) {
	switch (eKey) {
	case VK_ESCAPE :
		if (rObjectCurrent || rObjectEdit) {
		} else {
			if (group.m_objects.size())
				Init();
			else
				return FALSE;
			break;
		}
		//break;

	case VK_DELETE :
		if (rObjectCurrent || rObjectEdit) {
			if (rObjectCurrent) {
				if (rObjectCurrent->GetShape() == CShapeObject::S_POLY_LINE) {
					AddCurrentObject();
					break;
				}
				pptStageCurrent = NULL;
				ptsStage.clear();
				group.m_objects.Delete(rObjectCurrent);
				rObjectCurrent.Release();
			}
			if (rObjectEdit) {
				//if (pptStageCurrent) {
				//	for (int i = 0; i < ptsStage.size(); i++) {
				//		if (pptStageCurrent == &ptsStage[i]) {
				//			ptsStage.erase(ptsStage.begin() + i);
				//			if (i > 0)
				//				pptStageCurrent = &ptsStage[i-1];
				//			break;
				//		}
				//	}
				//	break;
				//} else {
					group.m_objects.Delete(rObjectEdit);
					rObjectEdit.Release();
				//}
			}
		} else {
			pptStageCurrent = NULL;
			if (group.m_objects.size()) {
				group.m_objects.Pop();
			} else
				return FALSE;
		}
		break;

	case VK_BACK :
		if (ptsStage.size()) {
			if (pptStageCurrent == &ptsStage.back())
				pptStageCurrent = NULL;
			ptsStage.pop_back();
		}
		break;

	case VK_RETURN :
		if (rObjectCurrent && (rObjectCurrent->GetShape() == CShapeObject::S_POLY_LINE) ) {
			AddCurrentObject();
		}
	}
	return TRUE;
}
BOOL CCameraView::CDrawObject::AddCurrentObject() {
	if (!rObjectCurrent)
		return FALSE;

	rObjectCurrent->SetFromPoints(ptsStage);
	pptStageCurrent = NULL;
	ptsStage.clear();

	if (eDrawMode == DM_SINGLE)
		group.m_objects.DeleteAll();

	if (group.m_objects.Find(rObjectCurrent) < 0)
	{
		if (bRectangle == TRUE)
		{
			//rObjectCurret 값이 있을 경우(하나의 Line 즉 두점이 존재하기에...)
			CPoint2d ptTopLeft, ptBottomRight, ptBottomLeft, ptTopRight;
			CRect2d rectStage;
			std::vector<CPoint2d> ptsStage;
			TRefPointer<CShapeObject> rObjectRectangle;
			rObjectCurrent->GetStartEndPoint(rectStage.pt0, rectStage.pt1);
			rObjectRectangle = new CShapeLine(crObject);

			ptTopLeft = rectStage.TopLeft();
			ptsStage.push_back(ptTopLeft);

			ptTopRight.x = rectStage.right;
			ptTopRight.y = rectStage.top;
			ptsStage.push_back(ptTopRight);
			
			ptBottomRight = rectStage.BottomRight();
			ptsStage.push_back(ptBottomRight);
			
			ptBottomLeft.x = rectStage.left;
			ptBottomLeft.y = rectStage.bottom;
			ptsStage.push_back(ptBottomLeft);
						
			rObjectRectangle->SetFromPoints(ptsStage);
			group.m_objects.Push(rObjectRectangle);

			rObjectRectangle.Release();
		}
		else
		{
			if (bCircle)
			{
				CPoint2d ptCenter, ptTemp;
				double dRadius, dTLength, dT0;
				CRect2d rectStage;
				TRefPointer<CShapeObject> rObjectCircle;
				rObjectCurrent->GetStartEndPoint(rectStage.pt0, rectStage.pt1);

				ptCenter = (rectStage.TopLeft() + rectStage.BottomRight()) / 2;
				ptTemp.x = rectStage.left;
				ptTemp.y = (rectStage.top + rectStage.bottom) / 2;
				dRadius = ptCenter.Distance(ptTemp);
				dT0 = 0;
				dTLength = 2 * M_PI;
				CShapeCircle* pCircle = new CShapeCircle(crObject);
				pCircle->m_dRadius = dRadius; 
				pCircle->m_ptCenter = ptCenter;
				pCircle->m_dTLength = dTLength;

				rObjectCircle = pCircle;
				group.m_objects.Push(rObjectCircle);
				
				//rObjectCircle.Release();
				//SAFE_DELETE(pCircle);
			}
			else
				group.m_objects.Push(rObjectCurrent);
		}
	}
	rObjectCurrent.Release();

	return TRUE;
}

HCURSOR CCameraView::CDrawObject::OnSetCursor(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen) {
	TRefPointer<CShapeObject> rObject;
	double dMinDistance = ctScreenToStage.Trans(5.0);	// 5 pixels -> mm
	if (OnMouse(ctScreenToStage, ptScreen, dMinDistance, rObject, NULL)) {
		return hCursor;
	}

	return NULL;
}
//-----------------------------------------------------------------------------
// CCameraView
//

CCameraView::CCameraView() : ICameraView(s_pIPClient), m_evtStopAll(FALSE, TRUE, NULL, NULL), m_imgProcessor(this), m_mouse(this) {
	m_hPreProcessor = NULL;
	m_hPostProcessor = NULL;

	m_dwTick0 = GetTickCount();
	m_nFrames = 0;
	m_bLiveBefore = FALSE;


	m_bInterlockedMode = FALSE;

	m_mouse.Init();

	// ADD BY LSW
	m_mouse.bSearchMode = FALSE;

	m_moveDisplayRegion.bMoveDisplayRegion = FALSE;
	m_moveDisplayRegion.bCaptured = FALSE;

	m_view.bDisplayRegion = TRUE;
	m_view.bDisplayGrid = FALSE;
	m_view.bDisplaySlit = FALSE;
	m_view.bDisplayFocusValue = FALSE;
	m_view.bDisplaySelectedRegionSize = TRUE;
	m_view.dGridIntervalX = 0.015;	// 15 um
	m_view.dGridIntervalY = 0.015;	// 15 um
	m_view.dGridSizeX = 0.015;
	m_view.dGridSizeY = 0.015;
	m_view.bDisplayLaserCenterOffset = TRUE;
	m_view.crOffset = RGB(255, 0, 0);
	m_view.sizeOffset = CSize(200, 200);
	//m_view.bDisplayGeometryInfo = TRUE;
	m_view.bMeasureMode = FALSE;

	m_pre.dDiff = 0.0;

	m_dZoom = 1.0;

	m_rStage = NULL;

	m_hCursorCross = NULL;
	if (s_pIPClient) {
		CString strPath = s_pIPClient->GetAbsPath(_T("..\\icon\\vision.cur"));
		m_hCursorCross = ::LoadCursorFromFile(strPath);
	}
	if (!m_hCursorCross) {
		extern HINSTANCE g_hInstance;
		m_hCursorCross = LoadCursor(g_hInstance, _T("IDC_CROSS2"));
	}
#ifdef _DEBUG
	m_iIndex = 0;
#endif

	SetLog(s_pLog);

}

CCameraView::~CCameraView() {
	CloseAll();
}

BEGIN_MESSAGE_MAP(CCameraView, ICameraView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_SHOWWINDOW()

	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()

	ON_WM_MOUSEWHEEL()
	ON_WM_MOUSEMOVE()
	ON_WM_SETCURSOR()

	ON_WM_HSCROLL()
	ON_WM_VSCROLL()

	ON_WM_SYSKEYDOWN()
	ON_WM_SYSKEYUP()

	ON_WM_TIMER()

	ON_COMMAND(IDC_CB_PAUSE, OnImagePause)
	ON_COMMAND(ID_IMAGE_LOAD, OnImageLoad)
	ON_COMMAND(ID_IMAGE_SAVE, OnImageSave)
	ON_COMMAND(ID_IMAGE_SAVE_REGION,OnImageSaveRegion)
	ON_COMMAND(ID_IMAGE_SAVE_EX, OnImageSaveEx)
	ON_COMMAND(ID_IMAGE_VIDEO_CAPTURE, OnVideoCapture)

	ON_COMMAND(ID_IMAGE_PATTERN_MATCHING, OnImagePatternMatching)
	ON_COMMAND(ID_IMAGE_FIND_EDGE, OnImageFindEdge)
	ON_COMMAND(ID_IMAGE_FIND_CORNER, OnImageFindCorner)
	ON_COMMAND(ID_IMAGE_FIND_LINE, OnImageFindLine)
	ON_COMMAND(ID_IMAGE_FIND_DOT, OnImageFindDot)
	ON_COMMAND(ID_IMAGE_FIND_SIMPLE_OBJECT, OnImageFindSimpleObject)

	ON_COMMAND(ID_VIEW_DISPLAY_REGION, OnViewDisplayRegion)
	ON_COMMAND(ID_VIEW_GRID, OnViewGrid)
	ON_COMMAND(ID_VIEW_SLIT, OnViewSlit)
	ON_COMMAND(ID_VIEW_FOCUS_VALUE, OnViewFocusValue)
	ON_COMMAND(ID_VIEW_SELECTED_REGION_SIZE, OnViewSelectedRegionSize)
	ON_COMMAND(ID_VIEW_CENTER_CROSS, OnViewLaserCenter)
	//ON_COMMAND(ID_VIEW_GEOMETRY_INFO, OnViewGeometryInfo)
	ON_COMMAND(ID_VIEW_MEASURE, OnViewMeasure)
	//ON_COMMAND(ID_VIEW_TOGGLE_MAIN_MENU, OnViewToggleMainMenu)
	ON_COMMAND(ID_VIEW_BOOST_IMAGE, OnViewBoostImage)
	//ON_UPDATE_COMMAND_UI(ID_VIEW_DISPLAY_REGION, OnUpdateViewDisplayRegion)
	ON_COMMAND(ID_VIEW_POPUP, OnViewPopup)

	//ON_COMMAND(ID_DRAW_INIT, OnDrawInit)
	//ON_COMMAND(ID_DRAW_ADD_LINE, OnDrawAddLine)
	//ON_COMMAND(ID_DRAW_ADD_POLYLINE, OnDrawAddPolyline)

	ON_COMMAND(ID_CALIBRATE_CAMERA_TO_STAGE, OnCalibrateCameraToStage)
	ON_COMMAND(ID_CALIBRATE_LASER_CENTER_OFFSET, OnCalibrateLaserCenterOffset)
	ON_COMMAND(ID_CALIBRATE_RESET_LASER_CENTER_OFFSET, OnCalibrateResetLaserCenterOffset)
	ON_COMMAND(ID_CALIBRATE_SLIT, OnCalibrateSlit)

	ON_COMMAND(ID_SETTINGS_STAGE, OnSettingsStage)
	ON_COMMAND(ID_SETTINGS_CAMERA, OnSettingsCamera)
END_MESSAGE_MAP()

BEGIN_IP_COMMAND_TABLE(CCameraView, ICameraView)
	ON_IP_EXCL("Image", "SelectRegion", OnIPImageSelectRegion)

	ON_IP("Image", "Load", OnIPImageLoad)
	ON_IP("Image", "Save", OnIPImageSave)
	ON_IP("Image", "SaveRegion", OnIPImageSaveRegion)
	ON_IP("Image", "SaveHardcopy", OnIPImageSaveHardcopy)
	ON_IP("Image", "StartVideoCapture", OnIPImageStartVideoCapture)
	ON_IP("Image", "StopVideoCapture", OnIPImageStopVideoCapture)

	ON_IP_EXCL("Calibrate", "ScreenToMachine", OnIPCalibrateScreenToMachine)
	ON_IP_EXCL("Calibrate", "LaserCenterOffset", OnIPCalibrateLaserCenterOffset)
	ON_IP_EXCL("Calibrate", "ResetLaserCenterOffset", OnIPCalibrateResetLaserCenterOffset)
	ON_IP_EXCL("Calibrate", "GetLaserCenterOffset", OnIPCalibrateGetLaserCenterOffset)

	// M : Machine, C : Camera Image, S : Screen
	ON_IP("Conv", "M2C", OnIPConvM2C)
	ON_IP("Conv", "C2M", OnIPConvC2M)
	ON_IP("Conv", "M2S", OnIPConvM2S)
	ON_IP("Conv", "S2M", OnIPConvS2M)
	ON_IP("Conv", "C2S", OnIPConvC2S)
	ON_IP("Conv", "S2C", OnIPConvS2C)

	ON_IP("Conv", "GetCT", OnIPConvGetCT)

	ON_IP("Conv", "GetLaserOffset", OnIPConvGetLaserOffset)

	ON_IP("Conv", "SlitP2M", OnIPConvSlitP2M)
	ON_IP("Conv", "SlitM2P", OnIPConvSlitM2P)

	ON_IP("Camera", "Setting", OnIPCameraSetting)

	ON_IP("View", "ToolMenu", OnIPViewToolMenu)
	ON_IP("View", "Activate", OnIPViewActivate)
	ON_IP("View", "DisplaySubRegion", OnIPViewDisplayRegion)
	ON_IP("View", "CrossMark", OnIPViewCrossMark)
	ON_IP("View", "Grid", OnIPViewGrid)
	ON_IP("View", "Slit", OnIPViewSlit)
	ON_IP("View", "FocusValue", OnIPViewFocusValue)
	ON_IP("View", "SelectedRegionSize", OnIPViewSelectedRegionSize)
	ON_IP("View", "Measure", OnIPViewMeasure)
	//ON_IP("View", "MainMenu", OnIPViewMainMenu)
	ON_IP("View", "BoostImage", OnIPViewBoostImage)
	ON_IP("View", "Zoom", OnIPViewZoom)
	ON_IP("View", "ShowText", OnIPViewShowText)
	ON_IP("View", "Popup", OnIPViewPopup)

	ON_IP("Draw", "Init", OnIPDrawInit)
	ON_IP("Draw", "Start", OnIPDrawStart)
	ON_IP("Draw", "GetObject", OnIPDrawGetObject)

END_IP_COMMAND_TABLE()

// CCameraView message handlers

BOOL CCameraView::PreCreateWindow(CREATESTRUCT& cs) {
	if (!ICameraView::PreCreateWindow(cs))
		return FALSE;

	//cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), reinterpret_cast<HBRUSH>(COLOR_WINDOW+1), NULL);

	return TRUE;
}

int CCameraView::OnCreate(LPCREATESTRUCT lpCreateStruct) {
	if (ICameraView::OnCreate(lpCreateStruct) == -1)
		return -1;

	m_imgProcessor.CreateIPCommandTarget(m_pIPClient);

	m_wndMenu.Create(_T("IDDB_MENU"), this);

	SetFocus();

	m_dlgSaveImage.Create(this);
	m_dlgVideoCapture.Create(this);
	m_dlgFindPattern.Create(this);
	m_dlgFindSimpleObject.Create(this);
	m_view.dlgBoostImage.Create(this);

	m_dlgCalibrationCameraToStage.Create(this);

	m_dlgCalibrationSlit.Create(this);

	CRect rect;
	GetClientRect(rect);
	m_mouse.wndTrack.CreateEx(WS_EX_TRANSPARENT, NULL, NULL, WS_THICKFRAME|WS_CHILD, CRect(rect.left, rect.top, rect.left+10, rect.top+10), this, 0);

	SetTimer(T_POLLING, 10, NULL);

	return 0;
}

void CCameraView::OnDestroy() {
	CloseAll();
	ICameraView::OnDestroy();
}

void CCameraView::OnPaint() {
	CPaintDC dcMain(this); // device context for painting
//#ifdef _DEBUG
//	if (m_iIndex++%2)	// Drop every other image.
//		return;
//#endif

	if (GetOSVersionMajor() <= HIBYTE(HIWORD(NTDDI_WINXP))) {
		// Windows XP... CMemDC가 정상동작하지않음.
		CRect rectClientP;
		__super::GetClientRect(rectClientP);
		if (rectClientP.IsRectEmpty())
			return ;

		CMemDCEx::CMemDC memDC(&dcMain, rectClientP);

		// Background.
		CBrush brush(GetSysColor(CTLCOLOR_DLG));
		memDC.FillRect(rectClientP, &brush);

		CRect rectClient;
		GetClientRect(rectClient);
		OnDraw(&memDC, rectClient);

		m_wndMenu.DrawShadow(memDC);
	} else {
		// Windows 7 이상
		CRect rectClient;
		GetClientRect(rectClient);
		if (rectClient.IsRectEmpty())
			return ;

		CMemDC memDC(dcMain, rectClient);

		// Background.
		CBrush brush(GetSysColor(CTLCOLOR_DLG));
		memDC.GetDC().FillRect(rectClient, &brush);

		OnDraw(&memDC.GetDC(), rectClient);

		m_wndMenu.DrawShadow(memDC.GetDC());
	}
}

BOOL CCameraView::ActivateView() {
	for (CWnd* pWndParent = GetParent(); pWndParent; pWndParent = pWndParent->GetParent()) {
		if (pWndParent->IsKindOf(RUNTIME_CLASS(CCameraPaneWnd))) {
			CCameraPaneWnd* pWndPane = (CCameraPaneWnd*)pWndParent;
			return pWndPane->ActivateChildPane(this);
		}
	}
	return FALSE;
}

void CCameraView::OnDraw(CDC* pDC, const CRect& rectClient) {
	CDC& dc = *pDC;

	// Dispaly Image
	{
		UpdateImage();
		if (m_imgBGR.empty())
			return;

		cv::Mat imgBGR = m_imgBGR;

		if (m_view.dlgBoostImage.m_boost.bBoost) {
			if (m_view.dlgBoostImage.m_boost.bEqualizeHistogram) {
				std::vector<cv::Mat> imgs;
				cv::split(imgBGR, imgs);
				for (unsigned int i = 0; i < imgs.size(); i++)
					cv::equalizeHist(imgs[i], imgs[i]);
				cv::merge(imgs, imgBGR);
			} else {
				if ( (m_view.dlgBoostImage.m_boost.iBrightnessR != 0) || (m_view.dlgBoostImage.m_boost.iBrightnessG != 0) || (m_view.dlgBoostImage.m_boost.iBrightnessB != 0) ) {
					cv::add(imgBGR, cv::Scalar(m_view.dlgBoostImage.m_boost.iBrightnessB, m_view.dlgBoostImage.m_boost.iBrightnessG, m_view.dlgBoostImage.m_boost.iBrightnessR), imgBGR);
				}
				if ( (m_view.dlgBoostImage.m_boost.dContrastR != 1.0) || (m_view.dlgBoostImage.m_boost.dContrastG != 1.0) || (m_view.dlgBoostImage.m_boost.dContrastB != 1.0) ) {
					cv::multiply(imgBGR, cv::Scalar(m_view.dlgBoostImage.m_boost.dContrastB, m_view.dlgBoostImage.m_boost.dContrastG, m_view.dlgBoostImage.m_boost.dContrastR), imgBGR);
				}
			}
		}

		MatToDC(imgBGR, m_sizeEffective, dc, rectClient);
	}

	{
		// Draw Rect Effective
		if (m_view.bDisplayRegion)
			DrawRectEffective(dc, rectClient);

		// Draw Selected Region
		if ( (m_mouse.eMouseMode == MM_SELECT_REGION_ONECLICK) || (m_mouse.eMouseMode == MM_SELECT_REGION_TWOCLICK) || m_mouse.bRubberBand )
			DrawSelRegion(dc, rectClient, m_mouse.bHideAttr ? FALSE : TRUE, m_view.bDisplaySelectedRegionSize);

		// Draw Grid
		if (m_view.bDisplayGrid && !m_mouse.bHideAttr)
			DrawGrid(dc, rectClient);

		// Draw Slit
		if (m_view.bDisplaySlit && !m_mouse.bHideAttr)
			DrawSlit(dc, rectClient);

		// Draw Cross
		if (m_view.bDisplayLaserCenterOffset && !m_mouse.bHideAttr)
			DrawCross(dc, rectClient);

		// Add by LSW
		// Measure
		if ( m_view.bMeasureMode && (m_mouse.eMouseMode == MM_MEASURE))
			DrawMeasure(dc, rectClient);

		// ADD BY LSW
		// SerchMode
		if (m_mouse.bSearchMode) {
			DrawSearchCross(dc, rectClient);
		}

		if (m_mouse.bShowText) {
			DrawText(dc, rectClient);
		}

		DrawObject(dc, rectClient);

		if ( m_video.writer.isOpened() ) {
			CString strText = _T("Recording...");
			static DWORD dwTickLast = 0;
			if (dwTickLast == 0)
				dwTickLast = GetTickCount();
			DWORD dwTick = GetTickCount();
			COLORREF cr = ( (GetTickCount() / 500) & BIT(0) ) ? RGB(255, 0, 0) : RGB(0, 0, 0);
			COLORREF crOld = dc.SetTextColor(cr);
			int eOldBkMode = dc.SetBkMode(TRANSPARENT);
			dc.DrawText(strText, CRect(rectClient), TA_LEFT|TA_TOP);
			dc.SetTextColor(crOld);
			dc.SetBkMode(eOldBkMode);
		}
	}
}

BOOL CCameraView::DrawRectEffective(CDC& dc, const CRect& rectClient) {
	int cx = m_sizeCamera.width;
	int cy = m_sizeCamera.height;
	if ( (m_rectEffective.width == cx) && (m_rectEffective.height == cy) )
		return FALSE;

	CCoordTrans ctI(m_ctCameraToScreen.GetInverse());
	CPoint ptImgTopLeft = ctI(rectClient.TopLeft());
	CPoint ptImgBottomRight = ctI(rectClient.BottomRight());

	int rcx = rectClient.Width()/10;
	int rcy = rcx * cy / cx;

	CCoordTrans ct(rcx/(double)cx,
		1, 0, 0, 1,
		cx/2, cy/2,
		rcx/2, rcy/2
		);

	CRect rectFullSize(ct.Trans(CPoint(0, 0)), ct.Trans(CPoint(cx, cy)));
	CRect rectDisplayed(ct.Trans(ptImgTopLeft), ct.Trans(ptImgBottomRight));

	CImage img;
	img.Create(rcx, rcy, 24);
	CDC dcBox;
	dcBox.Attach(img.GetDC());
	CBrush brushFullSize(RGB(255, 255, 255));
	dcBox.SelectStockObject(BLACK_PEN);
	dcBox.SelectObject(&brushFullSize);
	dcBox.Rectangle(rectFullSize);
	CBrush brushDisplayed(RGB(0, 0, 0));
	dcBox.SelectObject(&brushDisplayed);
	dcBox.FillRect(rectDisplayed, &brushDisplayed);
	dcBox.Detach();
	img.ReleaseDC();
	img.AlphaBlend(dc, rectClient.right-rcx-50, rectClient.bottom-rcy-50, rcx, rcy, 0, 0, rcx, rcy, 100);

	return TRUE;
}

BOOL CCameraView::DrawSelRegion(CDC& dc, const CRect& rectClient, BOOL bFillInside, BOOL bShowSize) {
	CPoint2d ptTopLeft, ptBottomRight;
	CRect2d rectStage;
	rectStage.pt0 = (m_mouse.ptsStage.N() >= 1) ? m_mouse.ptsStage[0] : m_mouse.ptStage;
	rectStage.pt1 = (m_mouse.ptsStage.N() >= 2) ? m_mouse.ptsStage[1] : m_mouse.ptStage;

	if (m_mouse.bRubberBand && m_mouse.wndTrack.m_hWnd && m_mouse.wndTrack.IsWindowVisible()) {
		const CCoordTrans& ct = m_ctScreenToStage;
		CRect rect;
		m_mouse.wndTrack.GetWindowRect(rect);
		ScreenToClient(rect);
		rectStage.pt0 = ct.Trans<CPoint2d>(rect.left, rect.top);
		rectStage.pt1 = ct.Trans<CPoint2d>(rect.right, rect.bottom);
	}

	CCoordTrans ctStageToScreen(m_ctScreenToStage.GetInverse());

	CRect rect(ctStageToScreen(rectStage.pt0), ctStageToScreen(rectStage.pt1));
	rect.NormalizeRect();

	CRect rectA = rect & rectClient;

	if (rectA.IsRectEmpty() || rect.IsRectEmpty())
		return FALSE;

	if (bFillInside) {
		CImage img;
		img.Create(rect.Width(), rect.Height(), 24);
		CDC dcBox;
		dcBox.Attach(img.GetDC());
		CRect rectBox(0, 0, img.GetWidth(), img.GetHeight());
		CBrush brush(RGB(255, 255, 255));
		dcBox.SelectStockObject(BLACK_PEN);
		dcBox.SelectObject(&brush);
		dcBox.Rectangle(rectBox);
		CPen pen(PS_DOT, 1, RGB(255, 0, 0));
		dcBox.SelectObject(&pen);
		CPoint ptCenter = rectBox.CenterPoint();
		dcBox.MoveTo(rectBox.left, ptCenter.y);
		dcBox.LineTo(rectBox.right, ptCenter.y);
		dcBox.MoveTo(ptCenter.x, rectBox.top);
		dcBox.LineTo(ptCenter.x, rectBox.bottom);
		dcBox.Detach();
		img.ReleaseDC();

		img.AlphaBlend(dc, rect.left, rect.top, rect.Width(), rect.Height(), 0, 0, rect.Width(), rect.Height(), 100);
	} else {
		CPen pen(PS_DOT, 1, RGB(128, 128, 255));
		CPen* pOldPen = dc.SelectObject(&pen);

		dc.MoveTo(rect.left, rect.top);
		dc.LineTo(rect.left, rect.bottom);
		dc.LineTo(rect.right, rect.bottom);
		dc.LineTo(rect.right, rect.top);
		dc.LineTo(rect.left, rect.top);

		CPoint ptCenter = rect.CenterPoint();
		dc.MoveTo(rect.left, ptCenter.y);
		dc.LineTo(rect.right, ptCenter.y);
		dc.MoveTo(ptCenter.x, rect.top);
		dc.LineTo(ptCenter.x, rect.bottom);

		dc.SelectObject(pOldPen);
	}

	if (bShowSize) {
		int eOldBkMode = dc.SetBkMode(TRANSPARENT);
		COLORREF crOldText = dc.SetTextColor(RGB(0, 0, 255));
		UINT eOldAlign = dc.SetTextAlign(TA_LEFT|TA_TOP);

		CString str;

		str.Format(_T("(%.3f, %.3f)"), rectStage.left, rectStage.top);
		dc.SetTextAlign(TA_RIGHT|TA_BOTTOM);
		dc.TextOut(rect.left, rect.top, str);

		str.Format(_T("(%.3f, %.3f)"), rectStage.right, rectStage.bottom);
		dc.SetTextAlign(TA_LEFT|TA_TOP);
		dc.TextOut(rect.right, rect.bottom, str);

		str.Format(_T("W %.3f"), rectStage.Width());
		dc.SetTextAlign(TA_RIGHT|TA_TOP);
		dc.TextOut(rect.left, rect.bottom, str);

		str.Format(_T("H %.3f"), rectStage.Height());
		dc.SetTextAlign(TA_LEFT|TA_BOTTOM);
		dc.TextOut(rect.right, rect.top, str);

		dc.SetTextAlign(eOldAlign);
		dc.SetTextColor(crOldText);
		dc.SetBkMode(eOldBkMode);
	}

	return TRUE;
}
BOOL CCameraView::DrawGrid(CDC& dc, const CRect& rectClient) {
	cv::Size size = m_sizeCamera;

	CPoint2d ptCenterImage;
	CPoint2d ptStage;
	CCoordTrans ct;
	{
		CS cs(&m_csCameraToStage);
		m_ctCameraToStage[GetCurrentLens()].GetShift(ptCenterImage);
		ct = m_ctScreenToStage.GetInverse();
		ptStage = m_rStage->GetStageXY();
	}
	CPoint ptCenterScreen = m_ctCameraToScreen(ptCenterImage);

	// GRAY COLOR
	CPen pen(PS_SOLID, 1, m_view.crOffset);
	CPen* pOldPen = dc.SelectObject(&pen);
	CSize ptSize(m_view.sizeOffset);

	// DRAW CROSS LINE
	CPoint ptLeft(m_ctCameraToScreen(CPoint(0, ptCenterImage.y)));
	CPoint ptRight(m_ctCameraToScreen(CPoint(size.width, ptCenterImage.y)));
	CPoint ptTop(m_ctCameraToScreen(CPoint(ptCenterImage.x, 0)));
	CPoint ptBottom(m_ctCameraToScreen(CPoint(ptCenterImage.x, size.height)));

	dc.MoveTo(ptLeft);
	dc.LineTo(ptRight);
	dc.MoveTo(ptTop);
	dc.LineTo(ptBottom);

	// CALC GRID INTERVAL & SIZE
	CPoint2d ptGridInterval(ConvStageToCamera(CPoint2d(m_view.dGridIntervalX, m_view.dGridIntervalY)));
	CPoint2d ptGridSize(ConvStageToCamera(CPoint2d(m_view.dGridSizeX, m_view.dGridSizeY)));
	CPoint2d ptGridSub(ConvStageToCamera(CPoint2d(0.0,0.0)));
	
	double dGridIntervalX(abs(ptGridInterval.x - ptGridSub.x));
	double dGridIntervalY(abs(ptGridInterval.y - ptGridSub.y));
	double dGridSizeX(abs(ptGridSize.x - ptGridSub.x));
	double dGridSizeY(abs(ptGridSize.y - ptGridSub.y));
	
	// DRAW GRID X ( 15um )
	for(int i=0; i < (size.width - ptCenterImage.x) / dGridIntervalX ; i++)
	{
		CPoint2d ptGridStartR(m_ctCameraToScreen(CPoint2d(ptCenterImage.x + (dGridIntervalX * i), ptCenterImage.y - dGridSizeY)));
		CPoint2d ptGridEndR(m_ctCameraToScreen(CPoint2d(ptCenterImage.x + (dGridIntervalX * i), ptCenterImage.y + dGridSizeY)));

		CPoint2d ptGridStartL(m_ctCameraToScreen(CPoint2d(ptCenterImage.x - (dGridIntervalX * i), ptCenterImage.y - dGridSizeY)));
		CPoint2d ptGridEndL(m_ctCameraToScreen(CPoint2d(ptCenterImage.x - (dGridIntervalX * i), ptCenterImage.y + dGridSizeY)));

		dc.MoveTo(ptGridStartR);
		dc.LineTo(ptGridEndR);
		dc.MoveTo(ptGridStartL);
		dc.LineTo(ptGridEndL);
	}

	// DRAW GRID Y ( 15um )
	for(int i=0; i < (size.height - ptCenterImage.y) / dGridIntervalY ; i++)
	{
		CPoint2d ptGridStartT(m_ctCameraToScreen(CPoint2d(ptCenterImage.x - dGridSizeY, ptCenterImage.y + (dGridIntervalY * i))));
		CPoint2d ptGridEndT(m_ctCameraToScreen(CPoint2d(ptCenterImage.x + dGridSizeY, ptCenterImage.y + (dGridIntervalY * i))));

		CPoint2d ptGridStartB(m_ctCameraToScreen(CPoint2d(ptCenterImage.x - dGridSizeY, ptCenterImage.y - (dGridIntervalY * i))));
		CPoint2d ptGridEndB(m_ctCameraToScreen(CPoint2d(ptCenterImage.x + dGridSizeY, ptCenterImage.y - (dGridIntervalY * i))));

		dc.MoveTo(ptGridStartT);
		dc.LineTo(ptGridEndT);
		dc.MoveTo(ptGridStartB);
		dc.LineTo(ptGridEndB);
	}

	dc.SelectObject(pOldPen);
	return TRUE;
}
BOOL CCameraView::DrawSlit(CDC& dc, const CRect& rectClient) {
	CCoordTrans ct;
	{
		CS cs(&m_csCameraToStage);
		ct = m_ctCameraToScreen;
		ct.SetShift(-m_ptCenterOffset[GetCurrentLens()]);
	}

	if (!m_rStage || !m_rStage->HaveSlit())
		return FALSE;
	CPoint2d pulse = m_rStage->GetSlitPulseXY();
	IMeshTable& tblX = m_dlgCalibrationSlit.m_tblSlitP2CX;
	IMeshTable& tblY = m_dlgCalibrationSlit.m_tblSlitP2CY;
	CPoint2d vx, vy;
	ct.RotateM(-m_rStage->GetSlitPulseAngle() * M_PI/180.);	// CCW.
	if (!tblX.Trans(pulse.x, vx) || !tblY.Trans(pulse.y, vy))
		return FALSE;

	CRect2d rect(vx[0], vy[0], vx[1], vy[1]);
	CPoint2d pts[4];
	pts[0] = ct.Trans<CPoint2d>(rect.pt0);
	pts[1] = ct.Trans<CPoint2d>(rect.left, rect.bottom);
	pts[2] = ct.Trans<CPoint2d>(rect.right, rect.bottom);
	pts[3] = ct.Trans<CPoint2d>(rect.right, rect.top);

	CPen pen(PS_DOT, 1, RGB(128, 255, 128));
	CPen* pOldPen = dc.SelectObject(&pen);
	dc.MoveTo(pts[0]);
	for (int i = 0; i < countof(pts); i++)
		dc.LineTo(pts[(i+1)%countof(pts)]);
	dc.SelectObject(pOldPen);

	return TRUE;
}
BOOL CCameraView::DrawCross(CDC& dc, const CRect& rectClient) {
	cv::Size size = m_sizeCamera;

	CPoint2d ptCenterImage;
	{
		CS cs(&m_csCameraToStage);
		m_ctCameraToStage[GetCurrentLens()].GetShift(ptCenterImage);
	}
	CPoint ptCenterScreen = m_ctCameraToScreen(ptCenterImage);

	CSize ptSize(m_view.sizeOffset);
	CPen pen(PS_SOLID, 1, m_view.crOffset);
	CPen* pOldPen = dc.SelectObject(&pen);
	dc.MoveTo(ptCenterScreen.x-ptSize.cx/2, ptCenterScreen.y);
	dc.LineTo(ptCenterScreen.x+ptSize.cx/2, ptCenterScreen.y);
	dc.MoveTo(ptCenterScreen.x, ptCenterScreen.y-ptSize.cy/2);
	dc.LineTo(ptCenterScreen.x, ptCenterScreen.y+ptSize.cy/2);

	dc.SelectObject(pOldPen);
	return TRUE;
}
BOOL CCameraView::DrawMeasure(CDC& dc, const CRect& rectClient) {
	if (!m_mouse.ptsStage.N())
		return FALSE;

	CPen pen(PS_SOLID, 3, RGB(255,0,0));
	CPen* pOldPen = dc.SelectObject(&pen);

	CCoordTrans ct;
	ct = m_ctScreenToStage.GetInverse();
	CPoint pt0;
	pt0 = (POINT)ct.Trans(m_mouse.ptsStage[0]);
	CPoint pt1;
	if (m_mouse.ptsStage.N() >= 2)
		pt1 = (POINT)ct.Trans(m_mouse.ptsStage[1]);
	else
		pt1 = (POINT)ct.Trans(m_mouse.ptStage);

	dc.MoveTo(pt0);	
	dc.LineTo(pt1);

	dc.SelectObject(pOldPen);

	return TRUE;
}

BOOL CCameraView::DrawSearchCross(CDC& dc, const CRect& rectClient) {
	CPen	pen(PS_DOT, 1, m_mouse.crCross);
	dc.SelectObject( &pen );

	const int iLineLen = 30;
	CPoint2d ptS = m_ctScreenToStage.TransI(m_mouse.ptSearchCross);
	CPoint2l pt(_round(ptS.x), _round(ptS.y));
	dc.MoveTo( pt.x - iLineLen, pt.y );
	dc.LineTo( pt.x + iLineLen, pt.y );
	dc.MoveTo( pt.x,			pt.y - iLineLen );
	dc.LineTo( pt.x,			pt.y + iLineLen );

	return TRUE;
}

BOOL CCameraView::DrawText(CDC& dc, const CRect& rectClient) {
	if (m_mouse.strText.IsEmpty() || !m_mouse.font.m_hObject)
		return FALSE;

	CPoint2d ptS = m_ctScreenToStage.TransI(m_mouse.ptText);
	//ptS += CPoint2d(rectClient.CenterPoint());

	CFont* pOldFont = dc.SelectObject(&m_mouse.font);
	COLORREF crOldText = dc.SetTextColor(m_mouse.crText);
	CRect rect(0, 0, 0, 0);
	//dc.TextOut(ptS.x, ptS.y, m_mouse.strText);
	dc.SetBkMode(TRANSPARENT);
	dc.DrawText(m_mouse.strText, rect, DT_CENTER|DT_CALCRECT);
	rect.MoveToXY(ptS.x - rect.Width()/2, ptS.y - rect.Height()/2);
	dc.DrawText(m_mouse.strText, rect, DT_CENTER);

	dc.SetTextColor(crOldText);
	dc.SelectObject(pOldFont);
	return TRUE;
}

BOOL CCameraView::DrawObject(CDC& dc, const CCoordTrans& ct, CShapeObject* pObject, int nLineThickness) {
	if (!pObject)
		return FALSE;

	CPen penDefault(PS_SOLID, 1, RGB(0, 0, 0));
	CPen* pOldPen = dc.SelectObject(&penDefault);
	
	CPen pen(PS_DASH, nLineThickness,RGB(0, 255, 0));
	dc.SelectObject(pen);
	SelectObject(dc, GetStockObject(NULL_BRUSH));
	switch (pObject->GetShape())
	{
		case CShapeObject::S_LINE :
			{
				TLineD pts = ((CShapeLine*)pObject)->m_pts; 
				//if (m_draw.bCircle == FALSE)
				{
					if (pts.size()) {
						dc.MoveTo(ct(pts[0]));
						for (int i = 1; i < pts.size(); i++)
							dc.LineTo(ct(pts[i]));
						dc.LineTo(ct(pts[0]));
					}
				}
				//else
				//{
				//	CPoint2d pt0, pt1;
				//	pt0 = ct(pts[0]);
				//	pt1 = ct(pts[1]);
				//	dc.Ellipse(pt0.x, pt0.y, pt1.x, pt1.y);
				//}
			}	
			break;
		case CShapeObject::S_POLY_LINE :
			{
				TLineD pts = ((CShapePolyLine*)pObject)->m_pts;
				if (pts.size()) {
					dc.MoveTo(ct(pts[0]));
					for (int i = 1; i < pts.size(); i++)
						dc.LineTo(ct(pts[i]));
					dc.LineTo(ct(pts[0]));

					double dLaserThickness = pObject->GetHatchingDensity();
					if (dLaserThickness > 0) {
						std::vector<CPoint2d> ptsL, ptsR;
						AddLaserOffsetToLine(pts, ptsL, dLaserThickness/2., FALSE);
						AddLaserOffsetToLine(pts, ptsR, -dLaserThickness/2., FALSE);
						CPen pen(PS_DOT, 1, pObject->GetColor());
						dc.SelectObject(&pen);
						if (ptsL.size()) {
							dc.MoveTo(ct(ptsL[0]));
							for (int i = 1; i < ptsL.size(); i++)
								dc.LineTo(ct(ptsL[i]));
							//dc.LineTo(ct(ptsL[0]));
						}
						if (ptsR.size()) {
							dc.MoveTo(ct(ptsR[0]));
							for (int i = 1; i < ptsR.size(); i++)
								dc.LineTo(ct(ptsR[i]));
							//dc.LineTo(ct(ptsR[0]));
						}
					}
				}
			}
			break;
		case CShapeObject::S_CIRCLE :
			{
				CPoint2d ptCenter;
				CPoint2d ptS, ptE;
				double dRadius, dTLength, dT0;
				dRadius = ((CShapeCircle*)pObject)->m_dRadius;
				dTLength = ((CShapeCircle*)pObject)->m_dTLength;
				ptCenter = ((CShapeCircle*)pObject)->m_ptCenter;
				ptS.x = ptCenter.x - dRadius;
				ptS.y = ptCenter.y - dRadius;
				ptE.x = ptCenter.x + dRadius;
				ptE.y = ptCenter.y + dRadius;
				CPoint2d pt0, pt1;
				pt0 = ct(ptS);
				pt1 = ct(ptE);
				dc.Ellipse(pt0.x, pt0.y, pt1.x, pt1.y);

				//const double M_2PI = 2 * M_PI;
				//int nSamplingPointsPer2PI = 48;
				//if (nSamplingPointsPer2PI <= 0) {
				//	nSamplingPointsPer2PI = (int)_min(dRadius*24/M_2PI, 24);
				//}
				//double dT1 = dT0+dTLength;
				//int n = (int)(nSamplingPointsPer2PI * fabs(dTLength) / M_2PI);
				//if (n < 8)
				//	n = 8;
				//for (int i = 0; i < n; i++) {
				//	double dTheta = i * dTLength / (n-1) + dT0;
				//	if (fabs(dTheta) > M_2PI)
				//		dTheta = fmod(dTheta, M_2PI);
				//	double c = dRadius * cos(dTheta);
				//	double s = dRadius * sin(dTheta);
				//	CPoint2d pt(ptCenter.x + c, ptCenter.y+s);
				//	if (i == 0)
				//		dc.MoveTo(pt);
				//	else
				//		dc.LineTo(pt);
				//}
			}
			break;
	}
	dc.SelectObject(pOldPen);

	return TRUE;
}

BOOL CCameraView::DrawObject(CDC& dc, const CRect& rectClient) {
	CCoordTrans ct;
	{
		CS cs(&m_csCameraToStage);
		m_ctScreenToStage.GetInverse(ct);
	}

	if (!m_draw.rObjectCurrent && m_draw.group.m_objects.empty())
		return FALSE;

	CPen penDefault(PS_SOLID, 1, RGB(0, 0, 0));
	CPen* pOldPen = dc.SelectObject(&penDefault);

	if (m_draw.rObjectCurrent) {
		std::vector<CPoint2d> pts;
		pts = m_draw.ptsStage;
		pts.push_back(m_draw.ptStage);
		TRefPointer<CShapeObject> rObject = m_draw.rObjectCurrent->NewClone();
		rObject->SetFromPoints(pts);
		DrawObject(dc, ct, rObject, 3);
	}
	int n = m_draw.group.m_objects.size();
	for (int i = 0; i < m_draw.group.m_objects.size(); i++) {
		DrawObject(dc, ct, m_draw.group.m_objects.GetItem(i), 2);
	}

	dc.SelectObject(pOldPen);

	return TRUE;
}


BOOL CCameraView::OnEraseBkgnd(CDC* pDC) {
	//return CWnd::OnEraseBkgnd(pDC);
	return TRUE;
}

void CCameraView::OnSize(UINT nType, int cx, int cy) {
	ICameraView::OnSize(nType, cx, cy);

	if (cx < 100)
		return;

	if (m_wndMenu.IsVisible()) {
		CRect rect;
		m_wndMenu.GetWindowRect(rect);
		ScreenToClient(rect);
		rect.right = rect.left + cx;
		m_wndMenu.MoveWindow(rect);
	}
}

void CCameraView::OnShowWindow(BOOL bShow, UINT nStatus) {
	ICameraView::OnShowWindow(bShow, nStatus);
	// FrameGrabber는 계속 동작 해야 함. (화면이 나오지 않는중에도 Edge Detect 등 외부에서 IPCommand 등이 들어올 수 있음
	//if (m_rFG) {
	//	if (bShow)
	//		m_rFG->StartCapture();
	//	else
	//		m_rFG->StopCapture();
	//}
}

BOOL CCameraView::StartMoveDisplayRegion(CPoint point) {
	if (GetFocus() != this)
		SetFocus();

	if (GetCapture()) {
		m_moveDisplayRegion.bCaptured = FALSE;
	} else {
		SetCapture();
		m_moveDisplayRegion.bCaptured = TRUE;
	}
	m_moveDisplayRegion.bMoveDisplayRegion = TRUE;
	m_moveDisplayRegion.ptScreen = point;
	m_ctCameraToScreen.GetShift(m_moveDisplayRegion.ptShift);

	return TRUE;
}
BOOL CCameraView::EndMoveDisplayRegion() {
	if ( (GetCapture() == this) && m_moveDisplayRegion.bCaptured ) {
		ReleaseCapture();
	}
	m_moveDisplayRegion.bCaptured = FALSE;
	m_moveDisplayRegion.bMoveDisplayRegion = FALSE;

	return TRUE;
}

BOOL CCameraView::UpdateMenuBar() {
	if (!m_wndMenu.IsVisible())
		return FALSE;

	BOOL bLive = m_rFG && m_rFG->IsLive();
	if (!bLive)
		m_nFrames = 0;
	BOOL bFreezed = FALSE;

	// Update Frame Rate
	DWORD dwTick = GetTickCount();

	if (bLive && !m_nFrames) {
		if (dwTick - m_dwTick0 > 2000)
			bFreezed = TRUE;
		//m_dwTick0 = dwTick;
	} else if ( bLive && (dwTick - m_dwTick0 >= 1000) ) {
		m_video.dFramesPerSec = m_nFrames*1000./(dwTick-m_dwTick0);
		::CheckAndSetDlgItemText(&m_wndMenu, IDC_CB_PAUSE, bLive ? Format(_T("%4.1f fps"), m_nFrames*1000./(dwTick-m_dwTick0)) : _T("calc..."));
		m_nFrames = 0;
		if (dwTick - m_dwTick0 > 2000)
			m_dwTick0 = dwTick;
		else
			m_dwTick0 += (dwTick - m_dwTick0);
	}

	if (!CompareBoolean(bLive, !m_wndMenu.IsDlgButtonChecked(IDC_CB_PAUSE))) {
		m_wndMenu.CheckDlgButton(IDC_CB_PAUSE, bLive ? 0 : 1);
	}

	CMFCButton* pButton = dynamic_cast<CMFCButton*>(m_wndMenu.GetDlgItem(IDC_CB_PAUSE));
	if (pButton && pButton->IsKindOf(RUNTIME_CLASS(CMFCButton))) {
		BOOL bChangeButton = FALSE;
		if ( (bFreezed || !bLive) && m_bLiveBefore) {
			m_bLiveBefore = FALSE;
			::CheckAndSetDlgItemText(&m_wndMenu, IDC_CB_PAUSE, bFreezed ? _T("Freezed") : _T("Paused"));
			pButton->m_bDontUseWinXPTheme = TRUE;
			pButton->SetFaceColor(bFreezed ? RGB(255, 0, 0) : RGB(255, 255, 0), TRUE);
			pButton->RedrawWindow();
		} else if ( (!bFreezed && bLive) && !m_bLiveBefore) {
			m_bLiveBefore = TRUE;
			::CheckAndSetDlgItemText(&m_wndMenu, IDC_CB_PAUSE, _T("calc..."));
			pButton->SetFaceColor(GetSysColor(COLOR_3DFACE), TRUE);
			pButton->m_bDontUseWinXPTheme = FALSE;
			pButton->RedrawWindow();
		}
	}

	// Update Cursor Position (Stage Position,...)
	CString str;
	if (m_view.bDisplayFocusValue)
		str += Format(_T("focus(%5.3f)"), m_pre.dDiff);
	CPoint2d pt = m_mouse.ptStage;
	str += Format(_T("(%8.3f, %8.3f)"), pt.x, pt.y);

	if (m_mouse.ptsStage.N()) {
		const TList<CPoint2d>& pts = m_mouse.ptsStage;
		if (pts.N() <= 2) {
			CPoint2d pt0 = pts[0];
			CPoint2d pt1 = pts.N() >= 2 ? pts[1] : m_mouse.ptStage;
			CPoint2d pt = pt1-pt0;
			str += Format(_T(" L = %.3f mm, start(%.3f, %.3f) size(%.3f, %.3f)"), pt.GetLength(), pt0.x, pt0.y, pt.x, pt.y);
			CCoordTrans ct = m_ctCameraToStage[GetCurrentLens()].GetInverse();
			CSize size = ct(pt1) - ct(pt0);
			str += Format(_T(" Img W/D (%d, %d)"), size.cx, size.cy);
		} else {
			for (int i = 0; i < pts.N(); i++) {
				str += Format(_T(" [%d](%.3f, %.3f)"), i+1, pts[i].x, pts[i].y);
			}
		}
	}

	m_wndMenu.SetViewInfo(str);

	return TRUE;
}

#include "ViewDlg.h"
static HWND ghWnd = NULL;
BOOL CCameraView::DetachView() {
	CWnd* pWndParent = GetParent();

	//static CViewDlg dlg;
	//if (!dlg.m_hWnd)
	//	dlg.Create(pWndParent);
	//dlg.ShowWindow(SW_MAXIMIZE);
	//dlg.m_pView->InitCamera(GetUnitName(), m_rFG);
	//dlg.m_pView->SetUnitName(NULL);

	//ghWnd = UnsubclassWindow();
	//SubclassWindow(dlg.GetViewHandle());

	return TRUE;
}
BOOL CCameraView::AttachView() {
	SubclassWindow(ghWnd);

	return TRUE;
}

void CCameraView::OnLButtonDown(UINT nFlags, CPoint point) {
	ICameraView::OnLButtonDown(nFlags, point);

	if (m_draw.OnLButtonDown(m_ctScreenToStage, point))
		return;

	if (m_mouse.bRubberBand)
		return;

	if ( (GetCapture() != this) && m_wndMenu.OnMouseAction(point, GetCurrentMessage()->message))
		return;

	if (GetFocus() != this) {
		SetFocus();
	}

	if (GetKeyState(VK_CONTROL) < 0) {
		StartMoveDisplayRegion(point);
		return;
	}

	CCoordTrans ctScreenToCamera(m_ctCameraToScreen.GetInverse());

	CPoint2d ptStage;
	{
		CS cs(&m_csCameraToStage);
		ptStage = m_ctScreenToStage(CPoint2d(point));
	}

	switch (m_mouse.eMouseMode) {
	case MM_NONE :
		if (GetCapture())
			return;
		SetCapture();
		//m_mouse.Init();

		if (m_view.bMeasureMode) {
			m_mouse.eMouseMode = MM_MEASURE;
		} else {
			m_mouse.eMouseMode = MM_SELECT_REGION_ONECLICK;
		}
		m_mouse.ptStage = ptStage;
		m_mouse.ptsStage.Attach(new CPoint2d(m_mouse.ptStage));
		m_mouse.ptsImage.Attach(new CPoint2d(ctScreenToCamera(point)));
		break;

	case MM_MEASURE :
		switch (m_mouse.ptsStage.N()) {
		case 0 :
		case 1 :
			m_mouse.ptStage = ptStage;
			m_mouse.ptsStage.Attach(new CPoint2d(m_mouse.ptStage));
			break;
		case 2 :
		default :
			m_mouse.Init();
			break;
		}
		break;

	case MM_SELECT_REGION_ONECLICK :
		if (m_mouse.ptsStage.N()) {
			m_mouse.Init();	// Turn Off
		}
		//} else {
			// Restart
			SetCapture();
			m_mouse.eMouseMode = MM_SELECT_REGION_ONECLICK;
			m_mouse.ptStage = ptStage;
			m_mouse.ptsStage.Attach(new CPoint2d(m_mouse.ptStage));
			m_mouse.ptsImage.Attach(new CPoint2d(ctScreenToCamera(point)));
		//}
		break;

	case MM_SELECT_REGION_TWOCLICK :
		if (!m_mouse.ptsStage.N())
			SetCapture();
		if (m_mouse.ptsStage.N() <= 1) {
			m_mouse.ptStage = ptStage;
			m_mouse.ptsStage.Attach(new CPoint2d(m_mouse.ptStage));
			m_mouse.ptsImage.Attach(new CPoint2d(ctScreenToCamera(point)));
		}
		if (m_mouse.ptsStage.N() >= 2) {
			if (GetCapture() == this)
				ReleaseCapture();
			cv::Mat img;
			if (m_rFG) {
				m_rFG->GetImage(img);
			}
			m_mouse.NotifyIP(&img);
			//m_mouse.Init();
		}
		break;
	}

	RedrawImage();
}

void CCameraView::OnLButtonUp(UINT nFlags, CPoint point) {
	ICameraView::OnLButtonUp(nFlags, point);

	if (m_draw.OnLButtonUp(m_ctScreenToStage, point))
		return;

	if (m_mouse.bRubberBand)
		return;

	if ( (GetCapture() != this) && m_wndMenu.OnMouseAction(point, GetCurrentMessage()->message))
		return;

 	if (m_moveDisplayRegion.bMoveDisplayRegion) {
		EndMoveDisplayRegion();
		return;
	}

	CPoint2d ptStage;
	{
		CS cs(&m_csCameraToStage);
		ptStage = m_ctScreenToStage(CPoint2d(point));
	}

	switch (m_mouse.eMouseMode) {
	case MM_SELECT_REGION_ONECLICK :
		if (GetCapture() == this)
			ReleaseCapture();
		m_mouse.ptStage = ptStage;
		m_mouse.ptsStage.Attach(new CPoint2d(m_mouse.ptStage));
		m_mouse.ptsImage.Attach(new CPoint2d(m_ctCameraToScreen.GetInverse()(point)));

		if (m_rFG) {
			cv::Mat img;
			m_rFG->GetImage(img);
			m_mouse.NotifyIP(&img);
		}
		break;

	case MM_SELECT_REGION_TWOCLICK :
		break;
	}

	RedrawImage();
}

void CCameraView::OnMButtonDown(UINT nFlags, CPoint point) {
	ICameraView::OnMButtonDown(nFlags, point);

	StartMoveDisplayRegion(point);
}

void CCameraView::OnMButtonUp(UINT nFlags, CPoint point) {
	ICameraView::OnMButtonUp(nFlags, point);

	EndMoveDisplayRegion();
}

void CCameraView::OnRButtonDown(UINT nFlags, CPoint point) {
	ICameraView::OnRButtonDown(nFlags, point);

	if (m_wndMenu && m_wndMenu.OnMouseAction(point, GetCurrentMessage()->message))
		return;

	//m_view.dlgBoostImage.m_boost.bBoost = FALSE;
	//if (m_view.dlgBoostImage.m_hWnd) {
	//	m_view.dlgBoostImage.ShowWindow(SW_HIDE);
	//}

	MoveStageTo(point);
}

void CCameraView::OnRButtonUp(UINT nFlags, CPoint point) {
	if (m_wndMenu && m_wndMenu.OnMouseAction(point, GetCurrentMessage()->message))
		return;
	ICameraView::OnRButtonUp(nFlags, point);
}

BOOL CCameraView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt) {
	return ICameraView::OnMouseWheel(nFlags, zDelta, pt);
}

void CCameraView::OnMouseMove(UINT nFlags, CPoint point) {
	if (m_wndMenu)
		m_wndMenu.OnMouseMoveAction(point);

	ICameraView::OnMouseMove(nFlags, point);

	if (m_draw.OnMouseMove(m_ctScreenToStage, point))
		return;

	CPoint2d ptStage;
	{
		CS cs(&m_csCameraToStage);
		ptStage = m_ctScreenToStage(CPoint2d(point));
	}

	if (m_mouse.bRubberBand)
		return;

	m_mouse.ptStage = ptStage;

	if (GetCapture() != this)
		return;

	if (m_moveDisplayRegion.bMoveDisplayRegion) {
		CCoordTrans ct(m_ctCameraToScreen);
		ct.SetShift(m_moveDisplayRegion.ptShift);
		CCoordTrans ctI(ct.GetInverse());
		CPoint2d ptShift = ctI(CPoint2d(point)) - ctI(CPoint2d(m_moveDisplayRegion.ptScreen));
		m_ctCameraToScreen.SetShift(m_moveDisplayRegion.ptShift-ptShift);
		// Adjust shift (double -> int)
		{
			CCoordTrans& ct = m_ctCameraToScreen;
			CPoint ptShiftScreen = ct.Trans(m_moveDisplayRegion.ptShift-ptShift);
			CPoint ptShiftCamera = ct.TransI(ptShiftScreen);
			ct.SetShift(ptShiftCamera);
		}
		RedrawImage();
		return;
	}

	switch (m_mouse.eMouseMode) {
	case MM_SELECT_REGION_ONECLICK :
	case MM_SELECT_REGION_TWOCLICK :
		//if (m_mouse.ptsStage.N() == 1) {
		//}
		RedrawImage();
		break;
	}

}

BOOL CCameraView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) {
	//switch (m_mouse.eMouseMode) {
	//case MM_SELECT_REGION_ONECLICK :
	//case MM_SELECT_REGION_TWOCLICK :
	//	if ( (m_mouse.ptsStage.N() <= 1) && m_hCursorCross) {
	//		SetCursor(m_hCursorCross);
	//		return TRUE;
	//	}
	//}

	CPoint pt = GetCurrentMessage()->pt;

	HCURSOR hCursor = m_draw.OnSetCursor(m_ctScreenToStage, pt);

	if (!hCursor)
		hCursor = m_hCursorCross;

	if (hCursor) {
		CRect rect;
		GetClientRect(rect);
		ScreenToClient(&pt);
		if (rect.PtInRect(pt) && SetCursor(hCursor))
			return TRUE;
	}	
	return ICameraView::OnSetCursor(pWnd, nHitTest, message);
}

void CCameraView::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
	ICameraView::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CCameraView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) {
	ICameraView::OnVScroll(nSBCode, nPos, pScrollBar);
}

void CCameraView::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags) {
	ICameraView::OnSysKeyDown(nChar, nRepCnt, nFlags);
}

void CCameraView::OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags) {
	ICameraView::OnSysKeyUp(nChar, nRepCnt, nFlags);
}

void CCameraView::OnTimer(UINT_PTR nIDEvent) {
	switch (nIDEvent) {
	case T_POLLING :
		// UpdateCoordTrans
		UpdateCoordTrans();

		{
			CPoint pt = GetCurrentMessage()->pt;
			ScreenToClient(&pt);
			CRect rect;
			GetClientRect(rect);
			if (m_rStage) {
				if (rect.PtInRect(pt)) {
					CPoint2d ptStage;
					{
						CS cs(&m_csCameraToStage);
						ptStage = m_ctScreenToStage(CPoint2d(pt));
					}

					if (!m_mouse.bRubberBand)
						m_mouse.ptStage = ptStage;
				} else {
					m_mouse.ptStage = m_rStage->GetStageXY();
				}
			}
		}
		// Update Menu Bar
		UpdateMenuBar();
		break;

	case T_KILL_SEARCH_MODE :
		KillTimer(nIDEvent);
		m_mouse.bSearchMode = FALSE;
		m_mouse.bShowText = FALSE;
		break;

	case T_STOP_VIDEO_CAPTURE :
		KillTimer(nIDEvent);
		StopVideoCapture();
		break;

	}
	ICameraView::OnTimer(nIDEvent);
}

void CCameraView::GetClientRect(LPRECT lpRect) const {
	__super::GetClientRect(lpRect);
	if (m_wndMenu.IsVisible() && lpRect) {
		CRect rect;
		m_wndMenu.GetWindowRect(rect);
		ScreenToClient(rect);
		lpRect->top = rect.bottom;
	}
}

void CCameraView::RedrawImage(BOOL bForceIfPaused) {
	if (m_rFG && !m_rFG->IsLive() && bForceIfPaused) {
		m_evtRefresh.SetEvent();
	}

	CRect rect;
	GetClientRect(rect);
	InvalidateRect(rect, FALSE);
}

BOOL CCameraView::ShowToolMenu(BOOL bShow) {
	if (!m_wndMenu.m_hWnd)
		return FALSE;
	if (!CompareBoolean(bShow, m_wndMenu.IsVisible())) {
		m_wndMenu.ShowWindow(bShow ? SW_SHOW : SW_HIDE);
		SaveState();
		Invalidate();
	}
	return TRUE;
}
BOOL CCameraView::IsToolMenuVisible() const {
	return m_wndMenu.IsVisible();
}

BOOL CCameraView::GetZoomedSize(const cv::Size& sizeReal, cv::Rect& rectEffective, cv::Size& sizeImage, cv::Size& sizeView) const {
	// Mat img 의 가로 길이는 반드시 4의 배수가 되어야 속도 저하가 없음.
	// img 의 너비를 4의 배수로 맞추고, 실제 이미지 크기는 sizeView 에서 가지고 있음.
	//

	if (!sizeReal.area())
		return FALSE;

	CRect rectClient;
	GetClientRect(rectClient);
	if ( (rectClient.Width() <= 0) || (rectClient.Height() <= 0) )
		return FALSE;

	int cx = sizeReal.width;
	int cy = sizeReal.height;

	sizeImage = sizeReal;
	rectEffective.x = 0;
	rectEffective.y = 0;
	rectEffective.width = cx;
	rectEffective.height = cy;
	sizeView = rectEffective.size();

	// Zoom 배율 설정 값을 가져옴
	CMenuBar::eZOOM eZoom = CMenuBar::Z_FIT_TO_SCREEN;
	double dZoom = 1.0;
	m_wndMenu.GetZoom(eZoom, dZoom);

	BOOL bAdjust = FALSE;

	switch (eZoom) {
	default :
	//case Z_FIT_TO_IMAGE :
		dZoom = 1.0;
		bAdjust = FALSE;
		break;

	case CMenuBar::Z_FIT_TO_SCREEN :
		if (sizeView.width && sizeView.height) {
			double dScaleX = (double)rectClient.Width() / sizeView.width;
			double dScaleY = (double)rectClient.Height() / sizeView.height;
			dZoom = min2(dScaleX, dScaleY);
			bAdjust = TRUE;
		}
		break;

	case CMenuBar::Z_FIT_TO_SCREEN_ANISO :
		sizeView.width = rectClient.Width();
		sizeView.height = rectClient.Height();

		sizeImage.width = ADJUST_DWORD_ALIGN(sizeView.width);
		sizeImage.height = sizeView.height;

		dZoom = 0.0;

		bAdjust = FALSE;
		break;

	case CMenuBar::Z_FLEXIBLE :
		bAdjust = TRUE;
		break;
	}

	// Adjust
	if ( bAdjust && (dZoom != 1.0) ) {
		sizeView.width =  _round(cx*dZoom);
		sizeView.height = _round(cy*dZoom);

		// Adjust Image Width for Speed up
		sizeImage.width = ADJUST_DWORD_ALIGN(sizeView.width);
		// Adjust zoom ratio
		dZoom = (double)sizeView.width / cx;
		sizeImage.height = _round(cy*dZoom);
	}

	// Rect Effective
	if (1) {
		if (sizeView.width > rectClient.Width()) {
			sizeView.width = _round(rectClient.Width() / dZoom);
			CPoint2d ptShift;
			m_ctCameraToScreen.GetShift(ptShift);
			rectEffective.x = _round(ptShift.x - sizeView.width/2);
			rectEffective.width = sizeView.width;
			if (rectEffective.x < 0)
				rectEffective.x = 0;
			if (rectEffective.x + rectEffective.width > cx)
				rectEffective.x = cx - rectEffective.width;
			sizeView.width = rectClient.Width();
			sizeImage.width = ADJUST_DWORD_ALIGN(sizeView.width);
		}
		if (sizeView.height > rectClient.Height()) {
			sizeView.height = _round(rectClient.Height() / dZoom);
			CPoint2d ptShift;
			m_ctCameraToScreen.GetShift(ptShift);
			rectEffective.y = _round(ptShift.y - sizeView.height/2);
			rectEffective.height = sizeView.height;
			if (rectEffective.y < 0)
				rectEffective.y = 0;
			if (rectEffective.y + rectEffective.height > cy)
				rectEffective.y = cy - rectEffective.height;
			sizeView.height = rectClient.Height();
			sizeImage.height = sizeView.height;
		}
	}

	const int nMinWidth = 12;
	const int nMinHeight = 9;
	if (sizeImage.width < nMinWidth)
		sizeImage.width = ADJUST_DWORD_ALIGN(nMinWidth);
	if (sizeImage.height < nMinHeight)
		sizeImage.height = nMinHeight;

	if (sizeView.width < sizeImage.width)
		sizeView.width = sizeImage.width;
	if (sizeView.height < sizeImage.height)
		sizeView.height = sizeImage.height;

	return TRUE;
}

void CCameraView::UpdateCoordTrans() {
	if (m_imgBGR.empty()) {
		m_ctCameraToScreen.SetTransformMatrix();
		return;
	}

	CCoordTrans& ct = m_ctCameraToScreen;

	CRect rectClient;
	GetClientRect(rectClient);
	CPoint2d ptCenterScreen(rectClient.CenterPoint());

	int cx = m_sizeCamera.width;
	int cy = m_sizeCamera.height;

	// Zoom 배율 설정 값을 가져옴
	CMenuBar::eZOOM eZoom = CMenuBar::Z_FIT_TO_SCREEN;
	double dZoom = 1.0;
	m_wndMenu.GetZoom(eZoom, dZoom);

	switch (eZoom) {
	default :
	//case Z_FIT_TO_IMAGE :
		ct.SetScale(1.0);
		ct.SetMatrix(1, 0, 0, 1);
		break;

	case CMenuBar::Z_FIT_TO_SCREEN :
		if (cx && cy) {
			double dScaleX = (double)rectClient.Width() / cx;
			double dScaleY = (double)rectClient.Height() / cy;
			dZoom = min2(dScaleX, dScaleY);
			ct.SetTransformMatrix(dZoom,
				1, 0, 0, 1,
				cx/2, cy/2,
				ptCenterScreen.x, ptCenterScreen.y
				);
		}
		break;

	case CMenuBar::Z_FIT_TO_SCREEN_ANISO :
		if (cx && cy) {
			ct.SetTransformMatrix(1.0,
				(double)rectClient.Width()/cx, 0.0, 0.0, (double)rectClient.Height()/cy,
				cx/2, cy/2,
				ptCenterScreen.x, ptCenterScreen.y
				);
		}
		break;

	case CMenuBar::Z_FLEXIBLE :
		ct.SetScale(dZoom);
		ct.SetMatrix(1, 0, 0, 1);
		break;
	}
	ct.SetOffset(ptCenterScreen);

	CPoint2d ptShift;
	ct.GetShift(ptShift);
	if (ptShift.x < m_rectEffective.width/2)
		ptShift.x = m_rectEffective.width/2;
	if (ptShift.x > cx-m_rectEffective.width/2)
		ptShift.x = cx-m_rectEffective.width/2;
	if (ptShift.y < m_rectEffective.height/2)
		ptShift.y = m_rectEffective.height/2;
	if (ptShift.y > cy-m_rectEffective.height/2)
		ptShift.y = cy-m_rectEffective.height/2;
	ct.SetShift(ptShift);

	// Adjust shift (double -> int)
	{
		CPoint ptShiftScreen = ct.Trans(ptShift);
		CPoint ptShiftCamera = ct.TransI(ptShiftScreen);
		ct.SetShift(ptShiftCamera);
	}


	{
		CS cs(&m_csCameraToStage);
		//-------------------------------------------------------------------------
		//m_ctCameraToStage;
		const int iCurrentLens = GetCurrentLens();

		if (IsFixedCamera()) {
			CPoint2d ptOffset;
			m_ctCameraToStage[iCurrentLens].GetOffset(ptOffset);
			if (m_rStage)
				m_rStage->SetVirtualStageXY(ptOffset);
		} else {
			if (m_rStage) {
				CPoint2d ptStage = m_rStage->GetStageXY();
				CCoordTrans ct(m_ctCameraToStage[iCurrentLens]);
				ct.SetShift(0, 0);
				ct.SetOffset(0, 0);
				CPoint2d pt = m_ptCenterOffset[iCurrentLens];
				m_ctCameraToStage[iCurrentLens].SetShift(m_sizeCamera.width/2+pt.x, m_sizeCamera.height/2+pt.y);
				m_ctCameraToStage[iCurrentLens].SetOffset(ptStage);
			}
		}

		//-------------------------------------------------------------------------
		//m_ctScreenToStage;
		m_ctScreenToStage = m_ctCameraToStage[iCurrentLens] * m_ctCameraToScreen.GetInverse();
	}
}

BOOL CCameraView::SetCTCameraToStage(const CCoordTrans& ct) {
	CS cs(&m_csCameraToStage);
	m_ctCameraToStage[GetCurrentLens()] = ct;

	if (IsFixedCamera()) {
		CPoint2d ptOffset;
		m_ctCameraToStage[GetCurrentLens()].GetOffset(ptOffset);
		if (m_rStage)
			m_rStage->SetVirtualStageXY(ptOffset);
	}

	UpdateCoordTrans();

	SaveState();

	return TRUE;
}

CPoint2d CCameraView::ConvCameraToStage(const CPoint2d& ptCamera, int iLensNo) {
	if (iLensNo < 0)
		iLensNo = GetCurrentLens();
	if ( (iLensNo < 0) || (iLensNo >= countof(m_ctCameraToStage)) ) {
		ErrorLog(__TFMSG("Wrong iLensNo(%d)"), iLensNo);
		return CPoint2d(0, 0);
	}
	CS cs(&m_csCameraToStage);
	return m_ctCameraToStage[iLensNo].Trans(ptCamera);
}
CPoint2d CCameraView::ConvStageToCamera(const CPoint2d& ptStage, int iLensNo) {
	if (iLensNo < 0)
		iLensNo = GetCurrentLens();
	if ( (iLensNo < 0) || (iLensNo >= countof(m_ctCameraToStage)) ) {
		ErrorLog(__TFMSG("Wrong iLensNo(%d)"), iLensNo);
		return CPoint2d(0, 0);
	}
	CS cs(&m_csCameraToStage);
	return m_ctCameraToStage[iLensNo].TransI(ptStage);
}
CCoordTrans CCameraView::GetCTCameraToStage(int iLensNo) {
	if (iLensNo < 0)
		iLensNo = GetCurrentLens();
	if ( (iLensNo < 0) || (iLensNo >= countof(m_ctCameraToStage)) ) {
		ErrorLog(__TFMSG("Wrong iLensNo(%d)"), iLensNo);
		return CCoordTrans();
	}
	return m_ctCameraToStage[iLensNo];
}


BOOL CCameraView::ShowCross(BOOL bShow, const CPoint2d& ptStage, COLORREF crColor, DWORD dwDuration) {
	m_mouse.bSearchMode = bShow;
	m_mouse.ptSearchCross = ptStage;
	m_mouse.crCross = crColor;
	if ( bShow && (dwDuration != 0) && (dwDuration != INFINITE) )
		SetTimer(T_KILL_SEARCH_MODE, 10*1000, NULL);

	Invalidate(FALSE);

	return TRUE;
}

BOOL CCameraView::ShowText(BOOL bShow, const CPoint2d& ptStage, LPCTSTR pszText, COLORREF cr, LOGFONT* pLogFont, DWORD dwDuration) {
	if (bShow) {
		m_mouse.ptText = ptStage;
		if (pLogFont && pLogFont->lfFaceName[0])
			m_mouse.font.CreateFontIndirect(pLogFont);
		if (!m_mouse.font.m_hObject)
			m_mouse.font.CreatePointFont(200, _T("Lucida Console"));
		m_mouse.crText = cr;
		m_mouse.strText = pszText;
		if ( (dwDuration != 0) && (dwDuration != INFINITE) )
			SetTimer(T_KILL_SEARCH_MODE, dwDuration, NULL);
	}
	m_mouse.bShowText = bShow;

	Invalidate(FALSE);

	return TRUE;
}

BOOL CCameraView::PreTranslateMessage(MSG* pMsg) {
	//if ( (pMsg->message != WM_PAINT) && (pMsg->message != WM_MOUSEMOVE) )
	//	TRACE("msg : %d\n", pMsg->message);
	if (pMsg->message == WM_KEYDOWN) {
		if (!m_draw.OnKeyDown(pMsg->wParam)) {
			if (pMsg->wParam == VK_ESCAPE) {
				BOOL bConsume = FALSE;
				if (GetCapture() == this) {
					ReleaseCapture();
					bConsume = TRUE;
				}
				if (m_pIPClient->IsValidCmd(m_mouse.pIPCmd)) {
					bConsume = TRUE;
					ContinueIPStepCommand(m_mouse.pIPCmd, IPPS_CANCELED);
				}
				m_mouse.Init();
				m_view.bMeasureMode = FALSE;
				if (bConsume)
					return TRUE;
			}
		}
	}
	return ICameraView::PreTranslateMessage(pMsg);
}

BOOL CCameraView::CloseAll() {
	m_evtStopAll.SetEvent();

	if (m_hPostProcessor) {
		WaitForSingleObject(m_hPostProcessor, INFINITE);
		CloseHandle(m_hPostProcessor);
		m_hPostProcessor = NULL;
	}
	if (m_hPreProcessor) {
		WaitForSingleObject(m_hPreProcessor, INFINITE);
		CloseHandle(m_hPreProcessor);
		m_hPreProcessor = NULL;
	}

	if (m_rFG) {
		m_rFG->StopCapture();
		m_rFG->CloseFrameGrabber();
	}
	m_rFG.Release();

	return TRUE;
}


CProfileSection& CCameraView::GetProfileCameraSection() {
	if (!s_pProfile) {
		static CProfileSection sectionNULL;
		return sectionNULL;
	}
	CString strUnitName(GetUnitName());
	if (strUnitName.IsEmpty())
		strUnitName = _T("Default");
	return s_pProfile->GetSection(_T("CameraSettings")).GetSubSection(strUnitName);
}
CProfileSection& CCameraView::GetProfileStageSection() {
	if (!s_pProfile) {
		static CProfileSection sectionNULL;
		return sectionNULL;
	}
	CString strUnitName(GetUnitName());
	if (strUnitName.IsEmpty())
		strUnitName = _T("Default");
	return s_pProfile->GetSection(_T("StageSettings")).GetSubSection(strUnitName);
}

BOOL CCameraView::InitCamera(LPCSTR pszUnitName, LPCTSTR pszCameraType, const CProfileSection& settings) {
	CString strMessage;
	TRefPointer<CFrameGrabber> rFG;
	rFG = CFrameGrabber::GetFrameGrabber(pszCameraType, settings, strMessage);
	if (!rFG) {
		ErrorLog(_T("FrameGrabber(%s) FAILED.\r\n%s"), pszCameraType, strMessage);
		rFG = CFrameGrabber::GetFrameGrabber(NULL, settings, strMessage);
	}
	if (!strMessage.IsEmpty()) {
		Log(_T("FrameGrabber(%s) Initialized : %s"), pszCameraType, strMessage);
	}

	if (!rFG)
		return FALSE;

	if (!InitCamera(pszUnitName, rFG))
		return FALSE;

	return TRUE;
}
BOOL CCameraView::InitCamera(LPCSTR pszUnitName, TRefPointer<CFrameGrabber>& rFG) {
	CloseAll();
	if (pszUnitName && *pszUnitName) {
		SetUnitName(pszUnitName);
		m_view.dlgBoostImage.SetUnitName(pszUnitName);
		m_dlgFindPattern.SetUnitName(pszUnitName);
		m_dlgCalibrationCameraToStage.SetUnitName(pszUnitName);
		m_dlgCalibrationSlit.SetUnitName(pszUnitName);
		m_imgProcessor.SetUnitName(pszUnitName);
	}

	LoadState();
	m_bLiveBefore = FALSE;

	//m_rStage->Create(m_pIPClient);
	m_rStage = new CStageInterface;
	m_rStage->Init(GetProfileStageSection());

	m_dlgCalibrationCameraToStage.CheckDlgButton(IDC_CB_FIXED, IsFixedCamera() ? 1 : 0);
	m_dlgCalibrationSlit.m_pStage = m_rStage;

	CString strMessage;
	m_rFG = rFG;
	
	m_evtStopAll.ResetEvent();
	DWORD dwThreadID = 0;
	m_hPreProcessor = CreateThread(NULL, 0, PreProcessorT, this, 0, &dwThreadID);
	m_hPostProcessor = CreateThread(NULL, 0, PostProcessorT, this, 0, &dwThreadID);

	if (m_rFG)
		m_rFG->StartCapture();

	return TRUE;
}

BOOL CCameraView::LoadState(BOOL bReload) {
	if (!s_pProfile)
		return FALSE;
	if (bReload)
		s_pProfile->Load();
	CProfileSection& section = GetProfileCameraSection();
	return SyncState(FALSE, section);
}
BOOL CCameraView::SaveState() {
	if (!s_pProfile)
		return FALSE;
	CProfileSection& section = GetProfileCameraSection();
	return SyncState(TRUE, section) && s_pProfile->Save();
}
BOOL CCameraView::SyncState(BOOL bStore, CProfileSection& section) {
	{
		CS cs(&m_csCameraToStage);
		for (int i = 0; i < countof(m_ctCameraToStage); i++) {
			CProfileSection& sectionLens = section.GetSubSection(Format(_T("Lens_%d"), i));
			m_ctCameraToStage[i].SyncData(bStore, sectionLens.GetSubSection(_T("CTCameraToStage")));
			sectionLens.SyncItemValue(bStore, _T("CenterOffset.x"), m_ptCenterOffset[i].x);
			sectionLens.SyncItemValue(bStore, _T("CenterOffset.y"), m_ptCenterOffset[i].y);
		}
	}

	section.SyncItemValueBoolean(bStore, _T("view.bDisplayRegion"), m_view.bDisplayRegion);
	section.SyncItemValueBoolean(bStore, _T("view.bDisplayGrid"), m_view.bDisplayGrid);
	section.SyncItemValueBoolean(bStore, _T("view.bDisplaySlit"), m_view.bDisplaySlit);
	section.SyncItemValueBoolean(bStore, _T("view.bDisplayFocusValue"), m_view.bDisplayFocusValue);
	section.SyncItemValueBoolean(bStore, _T("view.bDisplaySelectedRegionSize"), m_view.bDisplaySelectedRegionSize);
	section.SyncItemValue(bStore, _T("view.dGridInterval.x"), m_view.dGridIntervalX);
	section.SyncItemValue(bStore, _T("view.dGridInterval.y"), m_view.dGridIntervalY);
	section.SyncItemValue(bStore, _T("view.dGridSize.x"), m_view.dGridSizeX);
	section.SyncItemValue(bStore, _T("view.dGridSize.y"), m_view.dGridSizeY);
	section.SyncItemValueBoolean(bStore, _T("view.bDisplayLaserCenterOffset"), m_view.bDisplayLaserCenterOffset);
	section.SyncItemValue(bStore, _T("view.crOffset"), (__uint32&)m_view.crOffset);
	section.SyncItemValue(bStore, _T("view.sizeOffset.cx"), (__uint32&)m_view.sizeOffset.cx);
	section.SyncItemValue(bStore, _T("view.sizeOffset.cy"), (__uint32&)m_view.sizeOffset.cy);
	//section.SyncItemValueBoolean(bStore, _T("view.bDisplayGeometryInfo"), m_view.bDisplayGeometryInfo);

	if (m_wndMenu.m_hWnd) {
		if (bStore) {
			section.SetItemValueBoolean(_T("view.ToolMenu"), m_wndMenu.IsVisible());
		} else {
			BOOL bVisible = section.GetItemValueBoolean(_T("view.ToolMenu"), TRUE);
			m_wndMenu.ShowWindow(bVisible ? SW_SHOW : SW_HIDE);
		}
	}

	return TRUE;
}

BOOL CCameraView::UpdateImage() {
	{
		CS cs(&m_pre.csImgBGR);
		m_sizeEffective = m_pre.sizeView;
		m_rectEffective = m_pre.rectEffective;
		m_sizeCamera = m_pre.sizeCamera;
		m_pre.imgBGR.copyTo(m_imgBGR);
	}
	UpdateCoordTrans();
	return TRUE;
}

DWORD CCameraView::PreProcessorT(LPVOID pParam) {
	CCameraView* pThis = (CCameraView*)pParam;
	if (!pThis)
		return -1;
	return pThis->PreProcessor();
}
DWORD CCameraView::PreProcessor() {
	if (!m_rFG)
		return -1;
	TRefPointer<CEvent> evtNewImage(new CEvent);
	m_rFG->InsertWaitingQueue(evtNewImage);

	int iCount = 0;

	HANDLE handles[] = { m_evtStopAll, *(evtNewImage.GetPointer()), m_evtRefresh };
	cv::Mat imgBGR, imgBGRResized;
	while (TRUE) {
		DWORD dwResult = WaitForMultipleObjects(countof(handles), handles, FALSE, INFINITE);
		m_nFrames++;
		if (dwResult == WAIT_OBJECT_0)
			break;
		if (!m_rFG)
			break;

		if (!IsWindowVisible())
			continue;

		if (m_bInterlockedMode) {
			CFrameGrabberImageLock fgImage(m_rFG);
			cv::Mat& imgBGR = fgImage.GetImage();

			if (m_video.bUseSourceSize) {
				m_video.WriteImage(imgBGR);
			}

			// Prepare Image (Resizing)
			cv::Rect rectEffective;
			cv::Size sizeImage, sizeView;
			if (!GetZoomedSize(imgBGR.size(), rectEffective, sizeImage, sizeView))
				continue;
			if ( (rectEffective.x < 0) || (rectEffective.y < 0) || (rectEffective.width <= 0) || (rectEffective.height <= 0) )
				continue;
			CRect rectClient;
			GetClientRect(rectClient);

			if (sizeImage != imgBGR.size()) {
				if (CFrameGrabber::IsGPUEnabled()) {
					cv::gpu::GpuMat imgBGR2;
					cv::gpu::resize(cv::gpu::GpuMat(imgBGR(rectEffective)), imgBGR2, sizeImage, 0.0, 0.0, cv::INTER_LINEAR);
					if (!imgBGR2.empty()) {
						// Set Pre.
						CS cs(&m_pre.csImgBGR);
						m_pre.sizeView = sizeView;
						m_pre.rectEffective = rectEffective;
						m_pre.sizeCamera = imgBGR.size();
						imgBGR2.download(m_pre.imgBGR);
					}
				} else {
					CS cs(&m_pre.csImgBGR);
					m_pre.sizeView = sizeView;
					m_pre.rectEffective = rectEffective;
					m_pre.sizeCamera = imgBGR.size();
					cv::resize(imgBGR(rectEffective), m_pre.imgBGR, sizeImage, 0.0, 0.0, cv::INTER_LINEAR);
				}
			} else {
				CS cs(&m_pre.csImgBGR);
				m_pre.sizeView = sizeView;
				m_pre.rectEffective = rectEffective;
				m_pre.sizeCamera = imgBGR.size();
				imgBGR(rectEffective).copyTo(m_pre.imgBGR);
			}
		} else {

			m_rFG->GetImage(imgBGR);
			if (imgBGR.empty())
				continue;

			if (m_video.bUseSourceSize) {
				m_video.WriteImage(imgBGR);
			}

			// Prepare Image (Resizing)
			cv::Rect rectEffective;
			cv::Size sizeImage, sizeView;
			if (!GetZoomedSize(imgBGR.size(), rectEffective, sizeImage, sizeView))
				continue;
			CRect rectClient;
			GetClientRect(rectClient);

			if (sizeImage != imgBGR.size()) {
				if (CFrameGrabber::IsGPUEnabled()) {
					cv::gpu::GpuMat imgBGR2;
					cv::gpu::resize(cv::gpu::GpuMat(imgBGR(rectEffective)), imgBGR2, sizeImage, 0.0, 0.0, cv::INTER_LINEAR);
					if (!imgBGR2.empty())
						imgBGR2.download(imgBGRResized);
				} else {
					cv::resize(imgBGR(rectEffective), imgBGRResized, sizeImage, 0.0, 0.0, cv::INTER_LINEAR);
				}
			} else {
				imgBGRResized = imgBGR(rectEffective);
			}

			{
				CS cs(&m_pre.csImgBGR);
				m_pre.sizeView = sizeView;
				m_pre.rectEffective = rectEffective;
				m_pre.sizeCamera = imgBGR.size();
				imgBGRResized.copyTo(m_pre.imgBGR);
			}
		}

		if (m_view.bDisplayFocusValue) {
			try {
				cv::Mat img = m_pre.imgBGR;
				cv::Rect rect1(0, 0, _min(200, img.cols-1), _min(200, img.rows-1));
				rect1.x = img.cols/2 - rect1.width/2;
				rect1.y = img.rows/2 - rect1.height/2;
				cv::Rect rect2(rect1);
				rect2.x ++;
				rect2.y ++;
				cv::Mat matDiff = cv::abs(img(rect1) - img(rect2));
				cv::Scalar sum = cv::sum(matDiff);
				double dSum = 0.0;
				for (int i = 0; i < img.channels(); i++)
					dSum += sum[i];
				m_pre.dDiff = dSum / rect1.area() / img.channels();
				//TRACE("dSum : %g\n", dSum / rect1.area() / img.channels());
			} catch (...) {
			}
		} else {
			m_pre.dDiff = 0.0;
		}

		if (!m_video.bUseSourceSize) {
			m_video.WriteImage(m_pre.imgBGR);
		}

		RedrawImage(FALSE);
	}

	if (m_rFG)
		m_rFG->DeleteWaitingQueue(evtNewImage);

	return 0;
}

DWORD CCameraView::PostProcessorT(LPVOID pParam) {
	CCameraView* pThis = (CCameraView*)pParam;
	if (!pThis)
		return -1;
	return pThis->PostProcessor();
}
DWORD CCameraView::PostProcessor() {
	return 0;
}

//-----------------------------------------------------------------------------
// Implementation
BOOL CCameraView::SaveImage(LPCTSTR pszFileName, LPCTSTR pszText, const CPoint2d& ptText, int iFont, double dFontScale, int iQuality, CString& strPathSaved) {
	if (!m_rFG)
		return FALSE;
	cv::Mat imgBGR;
	m_rFG->GetImage(imgBGR);
	if (imgBGR.empty())
		return FALSE;
	return SaveImage(imgBGR, pszFileName, pszText, ptText, iFont, dFontScale, iQuality, strPathSaved);
}
BOOL CCameraView::SaveImage(const cv::Rect& _rect, LPCTSTR pszFileName, LPCTSTR pszText, const CPoint2d& ptText, int iFont, double dFontScale, int iQuality, CString& strPathSaved) {
	if (!m_rFG)
		return FALSE;
	cv::Mat imgBGR;
	cv::Mat img;
	cv::Rect rect(_rect);
	m_rFG->GetImage(img);
	if (img.empty())
		return FALSE;
	if (rect.width && rect.height) {
		if (rect.x < 0)
			rect.x = 0;
		if (rect.y < 0)
			rect.y = 0;
		if ( (rect.x >= img.cols) || (rect.y >= img.rows) )
			return FALSE;
		if (rect.x+rect.width > img.cols)
			rect.width = img.cols-rect.x;
		if (rect.y+rect.height> img.rows)
			rect.height = img.rows-rect.y;
		imgBGR = img(rect);
	} else {
		imgBGR = img;
	}
	return SaveImage(imgBGR, pszFileName, pszText, ptText, iFont, dFontScale, iQuality, strPathSaved);
}
BOOL CCameraView::SaveImage(const cv::Mat& imgBGR, LPCTSTR pszFileName, LPCTSTR pszText, const CPoint2d& ptText, int iFont, double dFontScale, int iQuality, CString& strPathSaved) {
	if (imgBGR.empty())
		return FALSE;

	cv::Mat img;
	imgBGR.copyTo(img);

	CStringA strText(pszText);
	if (!strText.IsEmpty()) {
		CRect rect(0, 0, img.cols, img.rows);
		//cv::Point pt(rect.left + 10, rect.top + 40);

		////<< LBH, 2013.12.17.
		//cv::Point pt(rect.left + 10, rect.top + 20);
		//double dGradient = 9.; //dFontScale이 1.4, 10일때 대략적인 글자 크기 측정 후 기울기 구함.
		//double dOffset = 7.; //dFontScale이 1.4, 10일때 대략적인 글자 크기 측정 후 Offset값을 구함.
		//pt += cv::Point(0, _round(dFontScale*dGradient+dOffset));
		////>>

		cv::Size size = cv::getTextSize("A", iFont, dFontScale, 1, NULL);
		cv::Point pt(ptText);
		pt.y += size.height;

		CStringsA strs;
		SplitString(strText, '\n', strs);
		for (int i = 0; i < strs.size(); i++) {
			strs[i].TrimRight();
			std::string str((LPCSTR)strs[i]);
			// Draw Outlines
			cv::putText(img, str, pt + cv::Point(-1, 0), iFont, dFontScale, cv::Scalar(0, 0, 0), 1, 8, false);
			cv::putText(img, str, pt + cv::Point( 1, 0), iFont, dFontScale, cv::Scalar(0, 0, 0), 1, 8, false);
			cv::putText(img, str, pt + cv::Point( 0, 1), iFont, dFontScale, cv::Scalar(0, 0, 0), 1, 8, false);
			cv::putText(img, str, pt + cv::Point( 0,-1), iFont, dFontScale, cv::Scalar(0, 0, 0), 1, 8, false);
			// Draw Text
			cv::putText(img, str, pt, iFont, dFontScale, cv::Scalar(255, 255, 255), 1, 8, false);
			pt.y += _round(size.height * 1.2);
		}
	}
	std::vector<int> params;
	params.push_back(CV_IMWRITE_JPEG_QUALITY);
	params.push_back(iQuality);

	CString strPath, strName;
	CString strFullPath(pszFileName);
	if (strFullPath.Find(_T('%')) >= 0) {
		COleDateTime t(COleDateTime::GetCurrentTime());
		strFullPath = t.Format(pszFileName);
	}

	SplitPath(strFullPath, strPath, strName);
	if (strName.IsEmpty())
		return FALSE;

	if (!strPath.IsEmpty())
		CreateIntermediateDirectory(strPath);
	if (strName.Find(_T('?')) >= 0) {
		if (strPath.IsEmpty()) {
			strPath = _T(".\\");
		}
		strFullPath = strPath + GetUniqueFileName(strPath, strName, FALSE);
	}

	strPathSaved = strFullPath;
	try {
		imwrite((LPCSTR)CStringA(strFullPath), img, params);
	} catch (...) {
		return FALSE;
	}

	return TRUE;
}

BOOL CCameraView::StartVideoCapture(LPCTSTR pszFileName, CString& strPathSaved, int fourcc, BOOL bUseSourceSize) {
	StopVideoCapture();

	// Get File Name
	CString strPath, strName;
	CString strFullPath(pszFileName);
	if (strFullPath.Find(_T('%')) >= 0) {
		COleDateTime t(COleDateTime::GetCurrentTime());
		strFullPath = t.Format(pszFileName);
	}

	SplitPath(strFullPath, strPath, strName);
	if (strName.IsEmpty())
		return FALSE;

	if (!strPath.IsEmpty())
		CreateIntermediateDirectory(strPath);
	if (strName.Find(_T('?')) >= 0) {
		if (strPath.IsEmpty()) {
			strPath = _T(".\\");
		}
		strFullPath = strPath + GetUniqueFileName(strPath, strName, FALSE);
	}

	strPathSaved = strFullPath;

	double dFramesPerSec = m_video.dFramesPerSec;
	m_video.nFramesSaved = 0;
	m_video.bUseSourceSize = bUseSourceSize;
	if (dFramesPerSec <= 0)
		dFramesPerSec = 15.8;
	if (!fourcc)
		fourcc = CV_FOURCC('M', 'J', 'P', 'G');
	cv::Mat img;
	if (m_video.bUseSourceSize)
		m_rFG->GetImage(img);
	else
		img = m_imgBGR;
	cv::Size size = img.size();
	bool bColor = img.channels() == 3;
	try {
		CS cs(&m_video.cs);
		if (!m_video.writer.open((LPCSTR)CStringA(strPathSaved), fourcc, dFramesPerSec, size, bColor)) {
			DeleteFile(strPathSaved);
			return FALSE;
		}
	} catch (...) {
		return FALSE;
	}
	return TRUE;
}
BOOL CCameraView::StopVideoCapture() {
	try {
		CS cs(&m_video.cs);
		if (m_video.writer.isOpened())
			m_video.writer.release();
	} catch (...) {
	}
	m_video.nFramesSaved = 0;
	m_video.bUseSourceSize = FALSE;
	return TRUE;
}

//-----------------------------------------------------------------------------
// Menu
void CCameraView::OnImagePause() {
	if (m_rFG) {
		m_rFG->SetLive(!m_rFG->IsLive());
	}
	//SaveState();

	UpdateMenuBar();
}

void CCameraView::OnImageLoad() {
	CFileDialog dlg(TRUE, _T(".bmp"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR, _T("Image Files(*.bmp;*.jpg;*.tiff;*.png|*.bmp;*.jpg;*.tiff;*.png|All File(*.*)|*.*||"));
	if (dlg.DoModal() != IDOK)
		return;

	cv::Mat imgStatic;
	try {
		imgStatic = cv::imread((const char*)CStringA(dlg.GetPathName()));
	} catch (cv::Exception&) {
		return;
	}

	if (m_rFG)
		m_rFG->SetStaticImage(imgStatic);
}
void CCameraView::OnImageSave() {
	PostIPCommand("Image", "Save");
}
void CCameraView::OnImageSaveRegion() {
	PostIPCommand("Image", "SaveRegion");
}
void CCameraView::OnImageSaveEx() {
	m_dlgSaveImage.ShowWindow(SW_SHOW);
	m_dlgSaveImage.SetFocus();
}
void CCameraView::OnVideoCapture() {
	m_dlgVideoCapture.ShowWindow(SW_SHOW);
	m_dlgVideoCapture.SetFocus();
}

void CCameraView::OnImagePatternMatching() {
	m_dlgFindPattern.ShowWindow(SW_SHOW);
	m_dlgFindPattern.SetFocus();
}
void CCameraView::OnImageFindEdge() {
	m_imgProcessor.PostIPCommand("Image", "FindEdge");
}
void CCameraView::OnImageFindCorner() {
	m_imgProcessor.PostIPCommand("Image", "FindCorner");
}
void CCameraView::OnImageFindLine() {
	m_imgProcessor.PostIPCommand("Image", "FindControlLine");
}
void CCameraView::OnImageFindDot() {
}
void CCameraView::OnImageFindSimpleObject() {
	m_dlgFindSimpleObject.ShowWindow(SW_SHOW);
	m_dlgFindSimpleObject.SetFocus();
}

void CCameraView::OnViewDisplayRegion() {
	m_view.bDisplayRegion = !m_view.bDisplayRegion;
	SaveState();
}
//void CCameraView::OnUpdateViewDisplayRegion(CCmdUI* pCmdUI) {
//	pCmdUI->SetCheck(m_view.bDisplayRegion);
//}
void CCameraView::OnViewGrid() {
	m_view.bDisplayGrid = !m_view.bDisplayGrid;
	SaveState();
}
void CCameraView::OnViewSlit() {
	m_view.bDisplaySlit = !m_view.bDisplaySlit;
	SaveState();
}
void CCameraView::OnViewFocusValue() {
	m_view.bDisplayFocusValue = !m_view.bDisplayFocusValue;
	SaveState();
}
void CCameraView::OnViewSelectedRegionSize() {
	m_view.bDisplaySelectedRegionSize = !m_view.bDisplaySelectedRegionSize;
	SaveState();
}
void CCameraView::OnViewLaserCenter() {
	m_view.bDisplayLaserCenterOffset = !m_view.bDisplayLaserCenterOffset;
	SaveState();
}
//void CCameraView::OnViewGeometryInfo() {
//	m_view.bDisplayGeometryInfo = !m_view.bDisplayGeometryInfo;
//	SaveState();
//}
void CCameraView::OnViewMeasure() {
	m_mouse.Init();
	m_view.bMeasureMode = !m_view.bMeasureMode;
}
//void CCameraView::OnViewToggleMainMenu() {
//	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
//	if (!pFrame)
//		return;
//	pFrame->ShowMenuBar(!pFrame->IsMenuVisible());
//}
void CCameraView::OnViewBoostImage() {
	m_view.dlgBoostImage.ShowWindow(SW_SHOW);
	m_view.dlgBoostImage.SetFocus();
	m_view.dlgBoostImage.m_boost.bBoost = TRUE;
}
void CCameraView::OnViewPopup() {
	extern HWND ghWnd;

	if (ghWnd == NULL)
		DetachView();
	else
		AttachView();
}

//void CCameraView::OnDrawInit() {
//	m_draw.Init();
//}
//void CCameraView::OnDrawAddLine() {
//	m_mouse.Init();
//	m_draw.Init();
//	m_draw.eDrawMode = DM_SINGLE;
//	m_draw.eShape = CShapeObject::S_LINE;
//}
//void CCameraView::OnDrawAddPolyline() {
//	m_mouse.Init();
//	m_draw.Init();
//	m_draw.eDrawMode = DM_SINGLE;
//	m_draw.eShape = CShapeObject::S_POLY_LINE;
//}

void CCameraView::OnCalibrateCameraToStage() {
	m_dlgCalibrationCameraToStage.Init(m_ctCameraToStage[GetCurrentLens()]);
	m_dlgCalibrationCameraToStage.ShowWindow(SW_SHOW);
}

BOOL CCameraView::OnIPCalibrateScreenToMachine(CIPCommand& cmd) {
	OnCalibrateCameraToStage();
	return TRUE;
}

void CCameraView::OnCalibrateLaserCenterOffset() {
	PostIPCommand("Calibrate", "LaserCenterOffset");
}

void CCameraView::OnCalibrateResetLaserCenterOffset() {
	if (MessageBox(_T("Calibrate Laser Offset"), _T("Are you sure, to Reset Laser Offset?"), MB_YESNOCANCEL|MB_ICONEXCLAMATION) != IDYES)
		return ;
	PostIPCommand("Calibrate", "ResetLaserCenterOffset");
}

BOOL CCameraView::OnIPCalibrateLaserCenterOffset(CIPCommand& cmd) {
	CIPVar& varCookie = cmd.m_varCookie;
	switch (cmd.GetCurrentStep()) {
	case 0 :
		{
			CIPVar varSR;
			varSR.SetChildItem("TwoClick", FALSE);
			varSR.SetChildItem("MessageBox", TRUE);
			varSR.SetChildItem("Title", _T("Laser Center Offset"));
			varSR.SetChildItem("Message", _T("Select Center Position of Laser"));
			SendIPStepCommand("Image", "SelectRegion", varSR);
		}
		cmd.EnqueueNextStep();
		break;

	case 1 :
		if (cmd.IsChildOK()) {
			const CIPVar& varRegion = cmd.GetChildResult(NULL, "Image", "SelectRegion");
			CPoint2d pt(varRegion(F_IMAGE_X, 0.0), varRegion(F_IMAGE_Y, 0.0));
			m_ptCenterOffset[GetCurrentLens()] = pt - CPoint2d(m_sizeCamera.width/2.0, m_sizeCamera.height/2.0);
		}

		UpdateCoordTrans();
		SaveState();

		m_mouse.Init();
		break;
	}

	return TRUE;
}

BOOL CCameraView::OnIPCalibrateResetLaserCenterOffset(CIPCommand& cmd) {
	m_ptCenterOffset[GetCurrentLens()].SetPoint(0, 0);
	UpdateCoordTrans();
	SaveState();

	return TRUE;
}

BOOL CCameraView::OnIPCalibrateGetLaserCenterOffset(CIPCommand& cmd) {
	CCoordTrans ct(m_ctCameraToStage[GetCurrentLens()]);
	ct.SetShift(0, 0);
	ct.SetOffset(0, 0);
	CPoint2d ptOffset = ct(m_ptCenterOffset[GetCurrentLens()]);
	cmd.m_varResult.SetChildItemUserType("Offset", &ptOffset);
	for (int i = 0; i < countof(m_ptCenterOffset); i++) {
		CCoordTrans ct(m_ctCameraToStage[i]);
		ct.SetShift(0, 0);
		ct.SetOffset(0, 0);
		CPoint2d ptOffset = ct(m_ptCenterOffset[i]);
		cmd.m_varResult.SetChildItemUserType(FormatA("Offset%d", i), &ptOffset);
	}

	return TRUE;
}

void CCameraView::OnCalibrateSlit() {
	ActivateView();
	m_dlgCalibrationSlit.SelectCurrentLens();
	m_dlgCalibrationSlit.ShowWindow(SW_SHOW);
}

void CCameraView::OnSettingsStage() {
	if (!s_pProfile)
		return;
	CProfileSection& section = GetProfileStageSection();
	CStageInterfaceDlg dlg;
	dlg.m_section = section;
	if (dlg.DoModal() != IDOK)
		return;
	section = dlg.m_section;
	s_pProfile->Save();
	if (m_rStage)
		m_rStage->Init(dlg.m_section);
}

void CCameraView::OnSettingsCamera() {
	if (m_rFG)
		m_rFG->OpenCameraSetting(this);
}

//-----------------------------------------------------------------------------
// IP Command Handler

BOOL CCameraView::OnIPImageSelectRegion(CIPCommand& cmd) {
	if (!cmd.IsOK() || !cmd.IsChildOK()) {

		CMessageDlg* pDlgMessage = (CMessageDlg*)cmd.m_pCookie;
		if (pDlgMessage)
			delete pDlgMessage;
		cmd.m_pCookie = NULL;
		m_mouse.pIPCmd = NULL;
		m_mouse.Init();
		return FALSE;
	}

	if (m_mouse.pIPCmd && (m_mouse.pIPCmd != &cmd))
		return FALSE;

	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	switch (cmd.GetCurrentStep()) {
	case 0 :
		{
			ActivateView();
			SetFocus();
			m_mouse.Init();
			m_mouse.pIPCmd = &cmd;
			m_mouse.eMouseMode = var("TwoClick", TRUE) ? MM_SELECT_REGION_TWOCLICK : MM_SELECT_REGION_ONECLICK;
			m_mouse.bHideAttr = var("HideAttr", FALSE);
			m_mouse.bRubberBand = var("RubberBand", FALSE);

			if (m_mouse.bRubberBand) {
				CRect rect;
				GetClientRect(rect);
				rect.DeflateRect(rect.Width()/3, rect.Height()/3);
				m_mouse.wndTrack.MoveWindow(rect);
				m_mouse.wndTrack.ShowWindow(SW_SHOW);
				m_mouse.eMouseMode = MM_NONE;

				CMessageDlg* pDlgMessage = new CMessageDlg(m_pIPClient);
				cmd.m_pCookie = pDlgMessage;
				pDlgMessage->Create(this);
				pDlgMessage->Show(&cmd, var("Title", _T("Select Region")), var("Message", _T("Select Region Using Mouse...")), IDOK, _T("OK"), IDCANCEL, _T("CANCEL"));
			} else {
				if (var("MessageBox", FALSE)) {
					CMessageDlg* pDlgMessage = new CMessageDlg(m_pIPClient);
					cmd.m_pCookie = pDlgMessage;
					pDlgMessage->Create(this);
					pDlgMessage->Show(&cmd, var("Title", _T("Select Region")), var("Message", _T("Select Region Using Mouse...")), IDCANCEL, _T("CANCEL"));
				}
			}
		}
		cmd.PauseStep(INFINITE);
		break;

	case 1 :
		{
			BOOL bRubberBand = m_mouse.bRubberBand;
			if (bRubberBand && m_mouse.wndTrack.m_hWnd) {
				CRect rect;
				m_mouse.wndTrack.GetWindowRect(rect);
				ScreenToClient(rect);
				TList<CPoint2d> ptsStage, ptsImage;
				ptsStage.Push(new CPoint2d(m_ctScreenToStage.Trans<CPoint2d>(rect.left, rect.top)));
				ptsStage.Push(new CPoint2d(m_ctScreenToStage.Trans<CPoint2d>(rect.right, rect.bottom)));
				ptsImage.Push(new CPoint2d(m_ctCameraToScreen.TransI<CPoint2d>(rect.left, rect.top)));
				ptsImage.Push(new CPoint2d(m_ctCameraToScreen.TransI<CPoint2d>(rect.right, rect.bottom)));

				cv::Mat img;
				m_rFG->GetImage(img);

				SavePositionToVar(varResult, &ptsStage, &ptsImage, &img);
			}
			m_mouse.pIPCmd = NULL;	// to prevent ContinueIPStepCommand
			m_mouse.Init();

			{
				CMessageDlg* pDlgMessage = (CMessageDlg*)cmd.m_pCookie;
				if (pDlgMessage)
					delete pDlgMessage;
				cmd.m_pCookie = NULL;
			}

			if (cmd.m_varCookie("eButton", -1) == IDCANCEL)
				return FALSE;
		}
		break;
	}
	return TRUE;
}

BOOL CCameraView::OnIPImageLoad(CIPCommand& cmd) {
	if (!m_rFG)
		return FALSE;

	const CIPVar& var = cmd.m_var;

	CString strFileName = var("FileName", _T(""));
	if (strFileName.IsEmpty()) {
		ActivateView();
		CFileDialog dlg(TRUE, _T(".bmp"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR, _T("Bitmap(*.bmp)|*.bmp|JPEG File(*.jpg)|*.jpg|All Files(*.*)|*.*||"));
		if (dlg.DoModal() != IDOK) {
			m_rFG->SetLive();
			return FALSE;
		}
		strFileName = dlg.GetPathName();
	}

	cv::Mat imgStatic;
	try {
		imgStatic = cv::imread((const char*)CStringA(strFileName));
	} catch (cv::Exception&) {
		ErrorLog(__TFUNCTION__ _T(" : FAILED... loading file %s"), strFileName);
		return FALSE;
	}

	if (m_rFG)
		m_rFG->SetStaticImage(imgStatic);

	return TRUE;
}

BOOL CCameraView::OnIPImageSave(CIPCommand& cmd) {
	if (!m_rFG)
		return FALSE;

	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	cv::Mat imgBGR;
	m_rFG->GetImage(imgBGR);

	CString strFileName = var("FileName", _T(""));
	if (strFileName.IsEmpty()) {
		ActivateView();
		CFileDialog dlg(FALSE, _T(".bmp"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR, _T("Bitmap(*.bmp)|*.bmp|JPEG File(*.jpg)|*.jpg|All Files(*.*)|*.*||"));
		if (dlg.DoModal() != IDOK) {
			m_rFG->SetLive();
			return FALSE;
		}
		strFileName = dlg.GetPathName();
	}

	CString strText = var("Text", _T(""));
	CPoint2d ptText(var("TextX", 0.0), var("TextY", 0.0));
	int iFont = var("FontFace", cv::FONT_HERSHEY_PLAIN);

	int iQuality = var("Quality", 95);
	double dFontScale = var("FontScale", 10.0);

	CString strPathSaved;
	BOOL bResult = SaveImage(imgBGR, strFileName, strText, ptText, iFont, dFontScale, iQuality, strPathSaved);
	varResult.SetChildItem("PathSaved", strPathSaved);

	if (!bResult)
		ErrorLog(__TFUNCTION__ _T(" : FAILED... saving as %s"), strFileName);

	if (var("MessageBox", FALSE)) {
		ActivateView();
		if (bResult)
			MessageBox(Format(_T("Image Saved as %s"), strFileName));
		else
			MessageBox(Format(_T("FAILED... saving as %s"), strFileName));
	}

	return bResult;
}

BOOL CCameraView::OnIPImageSaveRegion(CIPCommand& cmd) {
	if (!m_rFG)
		return FALSE;

	if (!cmd.IsOK() || !cmd.IsChildOK()) {
		m_rFG->SetLive(TRUE);
		if (cmd.m_pCookie) {
			cv::Mat* pImg = (cv::Mat*)cmd.m_pCookie;
			delete pImg;
			cmd.m_pCookie = NULL;
		}
		return FALSE;
	}
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;
	CIPVar& varCookie = cmd.m_varCookie;

	switch (cmd.GetCurrentStep()) {
	case 0 :
		{
			m_rFG->SetLive(FALSE);
			cv::Mat* pImg = new cv::Mat;
			cmd.m_pCookie = pImg;
			m_rFG->GetImage(*pImg);

			CIPVar varSR;
			varSR.SetChildItem("TwoClick", var("TwoClick", FALSE));
			if (var("MessageBox", FALSE)) {
				varSR.SetChildItem("MessageBox", TRUE);
				varSR.SetChildItem("Title", _T("Save Image"));
				varSR.SetChildItem("Message", _T("Select Region to Save"));
			} 
			SendIPStepCommand("Image", "SelectRegion", varSR);
		}
		cmd.EnqueueNextStep();
		break;

	case 1 :
		{
			if (!cmd.m_pCookie)
				return FALSE;

			cv::Mat img;
			img = *(cv::Mat*)cmd.m_pCookie;
			delete (cv::Mat*)cmd.m_pCookie;
			if (img.empty())
				return FALSE;

			// Get Region
			cv::Mat imgBGR;
			cv::Rect rect;
			{
				const CIPVar& varRegion = cmd.GetChildResult(NULL, "Image", "SelectRegion");
				cv::Point pt1;
				cv::Point pt2;
				pt1 = cv::Point(_round(varRegion(F_IMAGE_X"0", 0.0)), _round(varRegion(F_IMAGE_Y"0", 0.0)));
				pt2 = cv::Point(_round(varRegion(F_IMAGE_X"1", 0.0)), _round(varRegion(F_IMAGE_Y"1", 0.0)));
				rect = cv::Rect(pt1, pt2);
			}
			if (rect.width && rect.height) {
				if (rect.x < 0)
					rect.x = 0;
				if (rect.y < 0)
					rect.y = 0;
				if ( (rect.x >= img.cols) || (rect.y >= img.rows) ) {
					return FALSE;
				}
				if (rect.x+rect.width > img.cols)
					rect.width = img.cols-rect.x;
				if (rect.y+rect.height> img.rows)
					rect.height = img.rows-rect.y;
				if ( (rect.width <= 0) || (rect.height <= 0) )
					imgBGR = img;
				else
					imgBGR = img(rect);

				imgBGR = img(rect);
			} else {
				imgBGR = img;
			}

			// File Name
			CString strFileName = var("FileName", _T(""));
			if (strFileName.IsEmpty()) {
				CFileDialog dlg(FALSE, _T(".bmp"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR, _T("Bitmap(*.bmp)|*.bmp|JPEG File(*.jpg)|*.jpg|All Files(*.*)|*.*||"));
				if (dlg.DoModal() != IDOK) {
					m_rFG->SetLive(TRUE);
					return FALSE;
				}
				strFileName = dlg.GetPathName();
			}

			// Text
			CString strText = var("Text", _T(""));
			CPoint2d ptText(var("TextX", 0.0), var("TextY", 0.0));
			int iFont = var("FontFace", cv::FONT_HERSHEY_PLAIN);

			// JPG Quality
			int iQuality = var("Quality", 95);
			double dFontScale = var("FontScale", 10.0);
			CString strPathSaved;
			BOOL bResult = SaveImage(imgBGR, strFileName, strText, ptText, iFont, dFontScale, iQuality, strPathSaved);
			varResult.SetChildItem("PathSaved", strPathSaved);

			if (!bResult)
				ErrorLog(__TFUNCTION__ _T(" : FAILED... saving as %s"), strFileName);

			if (var("MessageBox", FALSE)) {
				if (bResult)
					MessageBox(Format(_T("Image Saved as %s"), strFileName));
				else
					MessageBox(Format(_T("FAILED... saving as %s"), strFileName));
			}

			m_rFG->SetLive(TRUE);
		}
		break;
	}

	return TRUE;
}

BOOL CCameraView::OnIPImageSaveHardcopy(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	CRect rectClient;
	GetClientRect(rectClient);
	CRect rectClient2;
	__super::GetClientRect(rectClient2);
	BOOL bResult = FALSE;

	try {
		if (rectClient.IsRectEmpty())
			return FALSE;

		CImage img;
		img.Create(rectClient2.Width(), rectClient2.Height(), 24);
		CDC dc;
		dc.Attach(img.GetDC());

		// Background.
		CBrush brush(GetSysColor(CTLCOLOR_DLG));
		dc.FillRect(rectClient, &brush);

		OnDraw(&dc, rectClient);

		dc.Detach();
		img.ReleaseDC();

		// Image to Mat
		cv::Mat imgBGR = cv::Mat::zeros(rectClient2.Height(), rectClient2.Width(), CV_8UC3);
		const BYTE* pImage = (const BYTE*)(const void*)img.GetBits();
		int nBufferPitch = img.GetPitch();	// nBufferPitch < 0.
		// Copy
		const BYTE* pos = NULL;
		pos = (const BYTE*)pImage;
		const size_t nSize = _min((size_t)imgBGR.step, abs(nBufferPitch));
		for (int y = 0; y < imgBGR.rows; y++, pos += nBufferPitch)
			memcpy(imgBGR.ptr(y), pos, nSize);
		if ( (imgBGR.cols >= m_imgBGR.cols) && (imgBGR.rows >= m_imgBGR.rows) ) {
			cv::Rect rc;
			rc.x = (imgBGR.cols - m_imgBGR.cols) / 2;
			rc.y = (imgBGR.rows - m_imgBGR.rows + (rectClient.top - rectClient2.top)) / 2;
			rc.width = m_imgBGR.cols;
			rc.height = m_imgBGR.rows;
			imgBGR = imgBGR(rc);
		}
		//cv::cvtColor(imgBGR, imgBGR, CV_RGB2BGR);

		CString strFileName = var("FileName", _T(""));
		if (strFileName.IsEmpty()) {
			ActivateView();
			CFileDialog dlg(FALSE, _T(".bmp"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR, _T("Bitmap(*.bmp)|*.bmp|JPEG File(*.jpg)|*.jpg|All Files(*.*)|*.*||"));
			if (dlg.DoModal() != IDOK) {
				m_rFG->SetLive();
				return FALSE;
			}
			strFileName = dlg.GetPathName();
		}

		CString strText = var("Text", _T(""));
		CPoint2d ptText(var("TextX", 0.0), var("TextY", 0.0));
		int iFont = var("FontFace", cv::FONT_HERSHEY_PLAIN);

		int iQuality = var("Quality", 95);
		double dFontScale = var("FontScale", 10.0);

		CString strPathSaved;
		bResult = SaveImage(imgBGR, strFileName, strText, ptText, iFont, dFontScale, iQuality, strPathSaved);
		varResult.SetChildItem("PathSaved", strPathSaved);

		if (!bResult)
			ErrorLog(__TFUNCTION__ _T(" : FAILED... saving as %s"), strFileName);

		if (var("MessageBox", FALSE)) {
			ActivateView();
			if (bResult)
				MessageBox(Format(_T("Image Saved as %s"), strFileName));
			else
				MessageBox(Format(_T("FAILED... saving as %s"), strFileName));
		}
	} catch (...) {
		return FALSE;
	}
	return bResult;
}

BOOL CCameraView::OnIPImageStartVideoCapture(CIPCommand& cmd) {
	if (!m_rFG)
		return FALSE;

	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	switch (cmd.GetStep()) {
	case 0 :
		{
			CString strFileName = var("FileName", _T(""));
			if (strFileName.IsEmpty()) {
				ActivateView();
				CFileDialog dlg(FALSE, _T(".avi"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR, _T("AVI File(*.avi)|*.avi|All Files(*.*)|*.*||"));
				if (dlg.DoModal() != IDOK) {
					m_rFG->SetLive();
					return FALSE;
				}
				strFileName = dlg.GetPathName();
			}
			CString strFourCC = var("FourCC", _T("MJPG"));
			int fourcc = 0;
			if (strFourCC.GetLength() >= 4)
				fourcc = CV_FOURCC(strFourCC[0], strFourCC[1], strFourCC[2], strFourCC[3]);
			else
				fourcc = var("FourCC", CV_FOURCC('M', 'J', 'P', 'G'));

			BOOL bUseSourceSize = var("UseSourceSize", TRUE);
			CString strPathSaved;
			if (!StartVideoCapture(strFileName, strPathSaved, fourcc, bUseSourceSize))
				return FALSE;
			varResult.SetChildItem("PathSaved", strPathSaved);

			DWORD dwLength = var("Length", INFINITE);
			if (dwLength == INFINITE)
				return TRUE;

			SetTimer(T_STOP_VIDEO_CAPTURE, dwLength, NULL);
			break;
		}
	}

	return TRUE;
}

BOOL CCameraView::OnIPImageStopVideoCapture(CIPCommand& cmd) {
	StopVideoCapture();
	ContinueIPStepCommandByName("Image", "StartVideoCapture");
	return TRUE;
}


BOOL CCameraView::OnIPConvM2C(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;
	int iLensNo = var("LensNo", GetCurrentLens());
	if ( (iLensNo < 0) || (iLensNo >= countof(m_ctCameraToStage)) )
		return FALSE;

	CPoint2d pt = ConvStageToCamera(CPoint2d(var("X", 0.0), var("Y", 0.0)), iLensNo);
	varResult.SetChildItem("X", pt.x);
	varResult.SetChildItem("Y", pt.y);

	return TRUE;
}
BOOL CCameraView::OnIPConvC2M(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;
	int iLensNo = var("LensNo", GetCurrentLens());
	if ( (iLensNo < 0) || (iLensNo >= countof(m_ctCameraToStage)) )
		return FALSE;

	CPoint2d pt = ConvCameraToStage(CPoint2d(var("X", 0.0), var("Y", 0.0)), iLensNo);
	varResult.SetChildItem("X", pt.x);
	varResult.SetChildItem("Y", pt.y);

	return TRUE;
}
BOOL CCameraView::OnIPConvM2S(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	CPoint2d pt = m_ctScreenToStage.TransI<CPoint2d>(var("X", 0.0), var("Y", 0.0));
	varResult.SetChildItem("X", pt.x);
	varResult.SetChildItem("Y", pt.y);

	return TRUE;
}
BOOL CCameraView::OnIPConvS2M(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	CPoint2d pt = m_ctScreenToStage.Trans<CPoint2d>(var("X", 0.0), var("Y", 0.0));
	varResult.SetChildItem("X", pt.x);
	varResult.SetChildItem("Y", pt.y);

	return TRUE;
}
BOOL CCameraView::OnIPConvC2S(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	CPoint2d pt = m_ctCameraToScreen.Trans<CPoint2d>(var("X", 0.0), var("Y", 0.0));
	varResult.SetChildItem("X", pt.x);
	varResult.SetChildItem("Y", pt.y);

	return TRUE;
}
BOOL CCameraView::OnIPConvS2C(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	CPoint2d pt = m_ctCameraToScreen.TransI<CPoint2d>(var("X", 0.0), var("Y", 0.0));
	varResult.SetChildItem("X", pt.x);
	varResult.SetChildItem("Y", pt.y);

	return TRUE;
}
BOOL CCameraView::OnIPConvGetCT(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	CS cs(&m_csCameraToStage);

	CCoordTrans ct;

	int iLensNo = var("LensNo", GetCurrentLens());
	if ( (iLensNo >= 0) && (iLensNo < countof(m_ptCenterOffset)) ) {
		ct = m_ctCameraToStage[iLensNo];
		varResult.SetChildItemUserType("C2M", &ct);
		ct = m_ctCameraToStage[iLensNo].GetInverse();
		varResult.SetChildItemUserType("M2C", &ct);
	}
	ct = m_ctScreenToStage;
	varResult.SetChildItemUserType("S2M", &ct);
	ct = m_ctScreenToStage.GetInverse();
	varResult.SetChildItemUserType("M2S", &ct);

	ct = m_ctCameraToScreen;
	varResult.SetChildItemUserType("C2S", &ct);
	ct = m_ctCameraToScreen.GetInverse();
	varResult.SetChildItemUserType("S2C", &ct);

	return TRUE;
}
BOOL CCameraView::OnIPConvGetLaserOffset(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	int iLensNo = var("LensNo", GetCurrentLens());
	if ( (iLensNo < 0) || (iLensNo >= countof(m_ptCenterOffset)) )
		return FALSE;

	CPoint2d pt;
	// in Stage Coordinate System
	CCoordTrans ct(m_ctCameraToStage[iLensNo]);
	ct.SetShift(0, 0);
	ct.SetOffset(0, 0);
	pt = ct(m_ptCenterOffset[iLensNo]);
	varResult.SetChildItem(F_MACHINE_X, pt.x);
	varResult.SetChildItem(F_MACHINE_Y, pt.y);

	// in Image Coordinate System
	varResult.SetChildItem(F_IMAGE_X, m_ptCenterOffset[iLensNo].x);
	varResult.SetChildItem(F_IMAGE_Y, m_ptCenterOffset[iLensNo].y);

	return TRUE;
}
BOOL CCameraView::OnIPConvSlitP2M(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;
	int iLensNo = var("LensNo", GetCurrentLens());
	if ( (iLensNo < 0) || (iLensNo >= countof(m_ctCameraToStage)) )
		return FALSE;

	CPoint2d w;
	m_dlgCalibrationSlit.m_tblSlitP2CX.Trans(var("PulseX", 0.0), w);
	CPoint2d h;
	m_dlgCalibrationSlit.m_tblSlitP2CY.Trans(var("PulseY", 0.0), h);

	CPoint2d ptLT, ptRB;
	{
		CS cs(&m_csCameraToStage);
		ptLT = m_ctCameraToStage[iLensNo].Trans(CPoint2d(w.val[0], h.val[0]));
		ptRB = m_ctCameraToStage[iLensNo].Trans(CPoint2d(w.val[1], h.val[1]));
	}

	varResult.SetChildItem("Width", fabs(ptRB.x-ptLT.x));
	varResult.SetChildItem("Height", fabs(ptRB.y-ptLT.y));

	return TRUE;
}
BOOL CCameraView::OnIPConvSlitM2P(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;
	int iLensNo = var("LensNo", GetCurrentLens());
	if ( (iLensNo < 0) || (iLensNo >= countof(m_ctCameraToStage)) )
		return FALSE;

	double dWidth = 0, dHeight = 0;
	{
		CS cs(&m_csCameraToStage);
		dWidth  = m_ctCameraToStage[iLensNo].TransI(var("Width", 0.0));
		dHeight = m_ctCameraToStage[iLensNo].TransI(var("Height", 0.0));
	}

	CPoint2d pulse;
	m_dlgCalibrationSlit.m_tblSlitC2PX.Trans(dWidth, pulse.x);
	m_dlgCalibrationSlit.m_tblSlitC2PY.Trans(dHeight, pulse.y);

	varResult.SetChildItem("PulseX", pulse.x);
	varResult.SetChildItem("PulseY", pulse.y);

	return TRUE;
}

//-----------------------------------------------------------------------------
// Camera Setting
BOOL CCameraView::OnIPCameraSetting(CIPCommand& cmd) {
	if (!m_rFG)
		return FALSE;
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	if (!m_rFG->IsCommPortOpen() && !m_rFG->OpenCommPort())
		return FALSE;

	BOOL bOpen = FALSE;
	if (!m_rFG->IsCommPortOpen())
		bOpen = m_rFG->OpenCommPort();
	if (!m_rFG->IsCommPortOpen())
		return FALSE;

	IFGCamera* pFGCamera = m_rFG->GetCamera();
	if (!pFGCamera) {
		if (bOpen)
			m_rFG->CloseCommPort();
		return FALSE;
	}

	TBuffer<BYTE> snd, recv;
	CStringA strCommand = var("Command", "");
	int dwTimeout = var("Timeout", 1000);
	pFGCamera->StringToBuffer(strCommand, snd);
	if (!pFGCamera->DoCommand(snd, &recv, dwTimeout))
		return FALSE;

	varResult.SetChildItem("Result", recv, recv.GetSize());

	if (bOpen)
		m_rFG->CloseCommPort();
	return TRUE;
}


//-----------------------------------------------------------------------------
// Display Option
BOOL CCameraView::OnIPViewToolMenu(CIPCommand& cmd) {
	return ShowToolMenu(cmd.m_var("View", TRUE));
}
BOOL CCameraView::OnIPViewActivate(CIPCommand& ) {
	return ActivateView();
}
BOOL CCameraView::OnIPViewDisplayRegion(CIPCommand& cmd) {
	m_view.bDisplayRegion = cmd.m_var("View", TRUE);
	SaveState();
	return TRUE;
}
BOOL CCameraView::OnIPViewCrossMark(CIPCommand& cmd) {
	m_view.bDisplayLaserCenterOffset = cmd.m_var("View", m_view.bDisplayLaserCenterOffset);
	m_view.crOffset = cmd.m_var("Color", (__uint32)m_view.crOffset);
	m_view.sizeOffset.cx = cmd.m_var("Width", (__uint32)m_view.sizeOffset.cx);
	m_view.sizeOffset.cy = cmd.m_var("Height", (__uint32)m_view.sizeOffset.cy);
	SaveState();
	return TRUE;
}
BOOL CCameraView::OnIPViewGrid(CIPCommand& cmd) {
	m_view.bDisplayGrid = cmd.m_var("View", TRUE);
	SaveState();
	return TRUE;
}
BOOL CCameraView::OnIPViewSlit(CIPCommand& cmd) {
	m_view.bDisplaySlit = cmd.m_var("View", TRUE);
	SaveState();
	return TRUE;
}
BOOL CCameraView::OnIPViewFocusValue(CIPCommand& cmd) {
	m_view.bDisplayFocusValue = cmd.m_var("View", TRUE);
	SaveState();
	return TRUE;
}
BOOL CCameraView::OnIPViewSelectedRegionSize(CIPCommand& cmd) {
	m_view.bDisplaySelectedRegionSize = cmd.m_var("View", TRUE);
	SaveState();
	return TRUE;
}
BOOL CCameraView::OnIPViewMeasure(CIPCommand& cmd) {
	m_mouse.Init();
	m_view.bMeasureMode = cmd.m_var("View", TRUE);
	SaveState();
	return TRUE;
}
//BOOL CCameraView::OnIPViewMainMenu(CIPCommand& cmd) {
//	CMainFrame* pFrame = (CMainFrame*)AfxGetMainWnd();
//	if (!pFrame)
//		return FALSE;
//	pFrame->ShowMenuBar(cmd.m_var("View", TRUE));
//	return TRUE;
//}
BOOL CCameraView::OnIPViewBoostImage(CIPCommand& cmd) {
	if (cmd.m_var("View", TRUE)) {
		ActivateView();
		m_view.dlgBoostImage.ShowWindow(SW_SHOW);
		m_view.dlgBoostImage.SetFocus();
		m_view.dlgBoostImage.m_boost.bBoost = TRUE;
	} else {
		m_view.dlgBoostImage.ShowWindow(SW_HIDE);
		m_view.dlgBoostImage.m_boost.bBoost = FALSE;
	}
	return TRUE;
}
BOOL CCameraView::OnIPViewZoom(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	CMenuBar::eZOOM eZoom = (CMenuBar::eZOOM)var("Zoom", CMenuBar::Z_FIT_TO_SCREEN);
	double dZoom = var("Scale", 1.0);
	m_wndMenu.SetZoom(eZoom, dZoom);

	return TRUE;
}
BOOL CCameraView::OnIPViewShowText(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	if (var("View", TRUE)) {
		COLORREF crText = var("Color", (__uint32)RGB(255, 255, 255));
		CPoint2d ptText;
		ptText.x = var("MX", 0.0);
		ptText.y = var("MY", 0.0);
		LOGFONT logFont;
		ZeroVar(logFont);
		var.GetChildItemUserType("Font", &logFont);
		CString strText = var("Text", _T(""));
		DWORD dwDuration = var("Duration", 10*1000);

		ShowText(TRUE, ptText, strText, crText, &logFont, dwDuration);
	} else
		ShowText(FALSE, CPoint2d(), NULL, 0, NULL, 0);
	return TRUE;
}
BOOL CCameraView::OnIPViewPopup(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	BOOL bPopup = var("Popup", TRUE);

	if (bPopup)
		DetachView();
	else
		AttachView();

	return TRUE;
}

//-----------------------------------------------------------------------------
// Draw Functions
BOOL CCameraView::OnIPDrawInit(CIPCommand& cmd) {
	m_draw.Init();
	return TRUE;
}

BOOL CCameraView::OnIPDrawStart(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	m_mouse.Init();
	//m_draw.Init();
	m_draw.pptStageCurrent = NULL;
	m_draw.rObjectCurrent.Release();
	m_draw.rObjectEdit.Release();
	m_draw.eDrawMode = (eDRAW_MODE)var("Mode", DM_SINGLE);
	m_draw.eShape = (CShapeObject::eSHAPE)var("Shape", CShapeObject::S_LINE);
	m_draw.bRectangle = var("Rectangle", FALSE);
	m_draw.bCircle = var("Circle", FALSE);
	m_draw.dThickness = var("LaserThickness", 0.030);
	m_draw.crObject = var("Color", (__uint32)RGB(255, 0, 0));
	
	return TRUE;
}

BOOL CCameraView::OnIPDrawGetObject(CIPCommand& cmd) {
	const CIPVar& var = cmd.m_var;
	CIPVar& varResult = cmd.m_varResult;

	if (!m_draw.group.m_objects.size())
		return FALSE;

	CMemFile f;
	CArchive ar(&f, CArchive::store);
	m_draw.group.Serialize(ar);
	ar.Flush();
	ar.Close();
	int nLength = f.GetLength();
	BYTE* pBuffer = f.Detach();

	varResult.SetChildItem("CShapeGroup", pBuffer, nLength);

	if (nLength && pBuffer) {
		free(pBuffer);
		pBuffer = NULL;
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// Stage Functions
int CCameraView::GetCurrentLens() {
	int iLensNo = 0;
	if (!m_rStage)
		return iLensNo;

	iLensNo = m_rStage->GetCurrentLens();
	if (iLensNo < 0)
		iLensNo = 0;
	if (iLensNo >= countof(m_ctCameraToStage))
		iLensNo = countof(m_ctCameraToStage)-1;
	return iLensNo;
}
CPoint2d CCameraView::GetStageXY() {
	if (m_rStage)
		return m_rStage->GetStageXY();
	CPoint2d ptOffset;
	CS cs(&m_csCameraToStage);
	m_ctCameraToStage[GetCurrentLens()].GetOffset(ptOffset);
	return ptOffset;
}
double CCameraView::GetStageZ() {
	if (m_rStage)
		return m_rStage->GetStageZ();
	return 0.0;
}
BOOL CCameraView::MoveStageTo(double dStageX, double dStageY, double dSpeed, BOOL bPost) {
	if (m_rStage)
		return m_rStage->MoveStageXY(CPoint2d(dStageX, dStageY), dSpeed, bPost);
	return FALSE;
}

BOOL CCameraView::MoveStageTo(const CPoint& ptScreen, double dSpeed, BOOL bPost) {
	CPoint2d ptStage = m_ctScreenToStage(CPoint2d(ptScreen));
	return MoveStageTo(ptStage.x, ptStage.y, dSpeed, bPost);
}

BOOL CCameraView::MoveStageTo(const CPoint2d& ptCamera, double dSpeed, BOOL bPost) {
	CS cs(&m_csCameraToStage);;
	CPoint2d ptStage = m_ctCameraToStage[GetCurrentLens()](ptCamera);
	return MoveStageTo(ptStage.x, ptStage.y, dSpeed, bPost);
}

BOOL CCameraView::MoveStageToStep(double dStageX, double dStageY, double dSpeed) {
	if (m_rStage)
		return m_rStage->MoveStageXY(CPoint2d(dStageX, dStageY), dSpeed, FALSE, m_rCMDCurrent);
	return FALSE;
}

BOOL CCameraView::MoveStageZTo(double dStageZ, double dSpeed, BOOL bPost) {
	if (m_rStage)
		return m_rStage->MoveStageZ(dStageZ, dSpeed, bPost);
	return FALSE;
}
BOOL CCameraView::MoveStageZToStep(double dStageZ, double dSpeed) {
	if (m_rStage)
		return m_rStage->MoveStageZ(dStageZ, dSpeed, FALSE, m_rCMDCurrent);
	return FALSE;
}

BOOL CCameraView::MoveSlitTo(const CPoint2d& ptPulse, BOOL bPost) {
	if (m_rStage)
		return m_rStage->MoveSlitPulseXY(ptPulse, bPost);
	return TRUE;
}
BOOL CCameraView::MoveSlitToStep(const CPoint2d& ptPulse) {
	if (m_rStage)
		return m_rStage->MoveSlitPulseXY(ptPulse, FALSE, m_rCMDCurrent);
	return TRUE;
}
BOOL CCameraView::MoveSlitAngleTo(double dTheta, BOOL bPost) {
	if (m_rStage)
		return m_rStage->MoveSlitAngle(dTheta, bPost);
	return TRUE;
}
BOOL CCameraView::MoveSlitAngleToStep(double dTheta) {
	if (m_rStage)
		return m_rStage->MoveSlitAngle(dTheta, FALSE, m_rCMDCurrent);
	return TRUE;
}
