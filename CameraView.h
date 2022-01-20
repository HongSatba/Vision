
// CameraView.h : interface of the CCameraView class
//

#pragma once

#include "StageInterface/StageInterface.h"
#include "StageInterface/StageInterfaceDlg.h"
#include "StageInterface/CalibrationCameraToStageDlg.h"
#include "StageInterface/CalibrationSlitDlg.h"

#include "ImageProcessor.h"

#include "FrameGrabber/ICameraView.h"
#include "xMathUtil/CoordTrans.h"
#include "xMathUtil/MeshTable.h"
#include "FrameGrabber/FrameGrabber.h"

#include "MenuBar.h"
//#include "CameraLayout.h"
#include "MessageDlg.h"
#include "SaveImageDlg.h"
#include "VideoCaptureDlg.h"
#include "BoostImageDlg.h"
#include "FindPatternDlg.h"
#include "FindSimpleObjectDlg.h"
#include "TrackWnd.h"

#include "Shape/Shape.h"

class IStageInterface;

// CCameraView window
class CCameraView : public ICameraView {
//class CCameraView : public CWnd {

public:
	DECLARE_DYNAMIC_CREATE(CCameraView, _T("CameraView"))

	friend class CMenuBar;
	friend class CCalibrationCameraToStageDlg;

protected:
	enum eTIMER {
		T_POLLING = 1034,
		T_KILL_SEARCH_MODE,
		T_STOP_VIDEO_CAPTURE,
	};
public:
	CCameraView();

// Attributes
public:

// Operations
public:

// Overrides
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	virtual void OnDraw(CDC* pDC, const CRect& rectClient);
	BOOL DrawRectEffective(CDC& dc, const CRect& rectClient);
	BOOL DrawSelRegion(CDC& dc, const CRect& rectClient, BOOL bFillInside, BOOL bShowSize);
	BOOL DrawGrid(CDC& dc, const CRect& rectClient);
	BOOL DrawSlit(CDC& dc, const CRect& rectClient);
	BOOL DrawCross(CDC& dc, const CRect& rectClient);
	BOOL DrawMeasure(CDC& dc, const CRect& rectClient);
	BOOL DrawSearchCross (CDC& dc, const CRect& rectClient);
	BOOL DrawText(CDC& dc, const CRect& rectClient);
	BOOL DrawObject(CDC& dc, const CRect& rectClient);
	BOOL DrawObject(CDC& dc, const CCoordTrans& ct, CShapeObject* pObject, int nLineThickness);

// Implementation
public:
	virtual ~CCameraView();

	BOOL ActivateView();

public:
	CImageProcessor m_imgProcessor;
public:
	CProfileSection& GetProfileCameraSection();
	CProfileSection& GetProfileStageSection();
	virtual BOOL InitCamera(LPCSTR pszUnitName, LPCTSTR pszCameraType, const CProfileSection& settings);
	virtual BOOL InitCamera(LPCSTR pszUnitName, TRefPointer<CFrameGrabber>& rFG);
protected:
	virtual BOOL LoadState(BOOL bReload = FALSE);
	virtual BOOL SaveState();
	virtual BOOL SyncState(BOOL bStore, CProfileSection& section);

protected:
	CMenuBar m_wndMenu;
public:
	void GetClientRect(LPRECT lpRect) const;
	virtual BOOL ShowToolMenu(BOOL bShow = TRUE);
	virtual BOOL IsToolMenuVisible() const;
	virtual BOOL GetZoomedSize(const cv::Size& sizeReal, cv::Rect& rectEffective, cv::Size& sizeImage, cv::Size& sizeView) const;

public:
	void RedrawImage(BOOL bForceIfPaused = TRUE);

	// Image
protected:
	BOOL m_bInterlockedMode;
	cv::Size m_sizeEffective;
	cv::Rect m_rectEffective;
	cv::Size m_sizeCamera;	// Camera Pixel Size.
	double m_dZoom;
	cv::Mat m_imgBGR;
	BOOL UpdateImage();
#ifdef _DEBUG
	mutable int m_iIndex;
#endif

protected:
	CPoint2d m_ptCenterOffset[MAX_LENS];	// Laser Offset
	CCriticalSection m_csCameraToStage;
	CCoordTrans m_ctCameraToScreen;
	CCoordTrans m_ctCameraToStage[MAX_LENS];
	CCoordTrans m_ctScreenToStage;
	//CCoordTrans m_ctObjectToStage;
	void UpdateCoordTrans();
	BOOL SetCTCameraToStage(const CCoordTrans& ct);
public:
	virtual BOOL ShowCross(BOOL bShow, const CPoint2d& ptStage, COLORREF crColor, DWORD dwDuration = 10*1000);
	virtual BOOL ShowText(BOOL bShow, const CPoint2d& ptStage = CPoint2d(), LPCTSTR pszText = NULL, COLORREF cr = RGB(0, 0, 0), LOGFONT* pLogFont = NULL, DWORD dwDuration = 10*1000);
	virtual CPoint2d ConvCameraToStage(const CPoint2d& ptCamera, int iLensNo = -1);
	virtual CPoint2d ConvStageToCamera(const CPoint2d& ptStage, int iLensNo = -1);
	virtual CCoordTrans GetCTCameraToStage(int iLensNo = -1);

protected:
	// mouse
	enum eMOUSE_MODE { MM_NONE, MM_SELECT_REGION_ONECLICK, MM_SELECT_REGION_TWOCLICK, MM_DRAW_OBJECT, MM_MEASURE };
	struct CMouseAction {
		CCameraView* const pThis;
		eMOUSE_MODE eMouseMode;
		CTrackWnd wndTrack;
		BOOL bRubberBand;
		BOOL bHideAttr;
		CPoint2d pt;
		CPoint2d ptStage;
		TList<CPoint2d> ptsStage;
		TList<CPoint2d> ptsImage;
		//CMessageDlg dlgMessage;

		CIPCommand* pIPCmd;

		CMouseAction(CCameraView* _pThis) : pThis(_pThis), pIPCmd(NULL) { }
		void Init();
		BOOL NotifyIP(cv::Mat* pImg = NULL);

		// ADD BY LSW
		BOOL bSearchMode;
		CPoint2d ptSearchCross;	// Machine Coordinate
		COLORREF crCross;

		// Text
		BOOL bShowText;
		COLORREF crText;
		CPoint2d ptText;
		CFont font;
		CString strText;
	} m_mouse;

	enum eDRAW_MODE { DM_NONE, DM_SINGLE, DM_MULTI };
	struct CDrawObject {
		eDRAW_MODE eDrawMode;
		std::vector<CPoint2d> ptsStage;
		CShapeGroup group;
		CShapeObject::eSHAPE eShape;
		BOOL bRectangle;
		BOOL bCircle;
		TRefPointer<CShapeObject> rObjectCurrent;
		TRefPointer<CShapeObject> rObjectEdit;
		CPoint2d* pptStageCurrent;
		CPoint2d ptStage;
		double dThickness;	// laser thickness
		COLORREF crObject;
		static HCURSOR hCursor;

		CDrawObject() {
			Init();
		}

		void Init();
		BOOL OnMouse(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen, double& dMinDinstance, TRefPointer<CShapeObject>& rObjectEdit, CPoint2d** ppPoints);
		BOOL OnLButtonDown(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen);
		BOOL OnLButtonUp(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen);
		//BOOL OnRButtonDown(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen);
		//BOOL OnRButtonUp(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen);
		BOOL OnMouseMove(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen);
		HCURSOR OnSetCursor(const CCoordTrans& ctScreenToStage, const CPoint& ptScreen);
		BOOL OnKeyDown(int eKey);
		BOOL AddCurrentObject();

	} m_draw;

	struct {
		BOOL bMoveDisplayRegion;
		BOOL bCaptured;
		CPoint2d ptShift;
		CPoint ptScreen;
	} m_moveDisplayRegion;

	struct {
		BOOL bDisplayRegion;
		BOOL bDisplayGrid;
		BOOL bDisplaySlit;
		BOOL bDisplayFocusValue;
		BOOL bDisplaySelectedRegionSize;
		double dGridIntervalX;
		double dGridIntervalY;
		double dGridSizeX;
		double dGridSizeY;
		BOOL bDisplayLaserCenterOffset;
		COLORREF crOffset;
		CSize sizeOffset;
		//BOOL bDisplayGeometryInfo;
		BOOL bMeasureMode;
		CBoostImageDlg dlgBoostImage;
	} m_view;

	CString m_strPathMeshTable;

protected:
	CCalibrationCameraToStageDlg m_dlgCalibrationCameraToStage;
	CSaveImageDlg m_dlgSaveImage;
	CVideoCaptureDlg m_dlgVideoCapture;

	CFindPatternDlg m_dlgFindPattern;
	CFindSimpleObjectDlg m_dlgFindSimpleObject;

protected:
	// Debug Output
	BOOL m_bLiveBefore;
	DWORD m_dwTick0;
	volatile int m_nFrames;

protected:
	HCURSOR m_hCursorCross;

protected:
	CEvent m_evtStopAll;
	BOOL CloseAll();

	CEvent m_evtRefresh;
	HANDLE m_hPreProcessor;
	static DWORD WINAPI PreProcessorT(LPVOID pParam);
	DWORD PreProcessor();

	struct {
		CCriticalSection csImgBGR;
		cv::Size sizeView;			// 화면 Display 영역 크기
		cv::Rect rectEffective;		// 전체 이미지 중에서 화면에 표시되는 Image 영역
		cv::Mat imgBGR;				// 전체 이미지 중에서 화면에 표시되는 부분만 가지고 있음
		cv::Size sizeCamera;		// Camera 에서 들어온 Image Size
		double dDiff;
	} m_pre;
	HANDLE m_hPostProcessor;
	static DWORD WINAPI PostProcessorT(LPVOID pParam);
	DWORD PostProcessor();

	// Implement
public:
	virtual BOOL SaveImage(LPCTSTR pszFileName, LPCTSTR pszText = NULL, const CPoint2d& ptText = CPoint2d(0, 0), int iFont = cv::FONT_HERSHEY_PLAIN, double dFontScale = 10., int iQuality = 95, CString& strPathSaved = CString());
	virtual BOOL SaveImage(const cv::Rect& rect, LPCTSTR pszFileName, LPCTSTR pszText = NULL, const CPoint2d& ptText = CPoint2d(0, 0), int iFont = cv::FONT_HERSHEY_PLAIN, double dFontScale = 10., int iQuality = 95, CString& strPathSaved = CString());
	virtual BOOL SaveImage(const cv::Mat& imgBGR, LPCTSTR pszFileName, LPCTSTR pszText = NULL, const CPoint2d& ptText = CPoint2d(0, 0), int iFont = cv::FONT_HERSHEY_PLAIN, double dFontScale = 10., int iQuality = 95, CString& strPathSaved = CString());

protected:
	struct {
		CCriticalSection cs;
		double dFramesPerSec;
		BOOL bUseSourceSize;
		int nFramesSaved;
		cv::VideoWriter writer;
		void WriteImage(const cv::Mat& img) {
			CS cs(&cs);
			if (writer.isOpened()) {
				try { writer << img; }
				catch(...) { }
				nFramesSaved ++;
			}
		}
	} m_video;
public:
	virtual BOOL StartVideoCapture(LPCTSTR pszFileName, CString& strPathSaved, int fourcc = CV_FOURCC('M', 'J', 'P', 'G'), BOOL bUseSourceSize = FALSE);
	virtual BOOL StopVideoCapture();
	virtual BOOL GetVideoCapturedFrames(int &nFrames) const { nFrames = m_video.nFramesSaved; return m_video.writer.isOpened(); }

public:
	virtual BOOL GetImage(cv::Mat& img) { return m_rFG->GetImage(img); }

public:
	virtual BOOL StartMoveDisplayRegion(CPoint point);
	virtual BOOL EndMoveDisplayRegion();
	virtual BOOL UpdateMenuBar();
	virtual BOOL DetachView();
	virtual BOOL AttachView();

protected:
	DECLARE_IP()
	ip_handler BOOL OnIPImageSelectRegion(CIPCommand& cmd);
	ip_handler BOOL OnIPImageLoad(CIPCommand& cmd);
	ip_handler BOOL OnIPImageSave(CIPCommand& cmd);
	ip_handler BOOL OnIPImageSaveRegion(CIPCommand& cmd);
	ip_handler BOOL OnIPImageSaveHardcopy(CIPCommand& cmd);
	ip_handler BOOL OnIPImageStartVideoCapture(CIPCommand& cmd);
	ip_handler BOOL OnIPImageStopVideoCapture(CIPCommand& cmd);

	ip_handler BOOL OnIPCalibrateScreenToMachine(CIPCommand& cmd);
	ip_handler BOOL OnIPCalibrateLaserCenterOffset(CIPCommand& cmd);
	ip_handler BOOL OnIPCalibrateResetLaserCenterOffset(CIPCommand& cmd);
	ip_handler BOOL OnIPCalibrateGetLaserCenterOffset(CIPCommand& cmd);

	ip_handler BOOL OnIPConvM2C(CIPCommand& cmd);
	ip_handler BOOL OnIPConvC2M(CIPCommand& cmd);
	ip_handler BOOL OnIPConvM2S(CIPCommand& cmd);
	ip_handler BOOL OnIPConvS2M(CIPCommand& cmd);
	ip_handler BOOL OnIPConvC2S(CIPCommand& cmd);
	ip_handler BOOL OnIPConvS2C(CIPCommand& cmd);
	ip_handler BOOL OnIPConvGetLaserOffset(CIPCommand& cmd);

	ip_handler BOOL OnIPConvGetCT(CIPCommand& cmd);

	ip_handler BOOL OnIPConvSlitP2M(CIPCommand& cmd);
	ip_handler BOOL OnIPConvSlitM2P(CIPCommand& cmd);

	ip_handler BOOL OnIPCameraSetting(CIPCommand& cmd);

	ip_handler BOOL OnIPViewToolMenu(CIPCommand& cmd);
	ip_handler BOOL OnIPViewActivate(CIPCommand& cmd);
	ip_handler BOOL OnIPViewDisplayRegion(CIPCommand& cmd);
	ip_handler BOOL OnIPViewCrossMark(CIPCommand& cmd);
	ip_handler BOOL OnIPViewGrid(CIPCommand& cmd);
	ip_handler BOOL OnIPViewSlit(CIPCommand& cmd);
	ip_handler BOOL OnIPViewFocusValue(CIPCommand& cmd);
	ip_handler BOOL OnIPViewSelectedRegionSize(CIPCommand& cmd);
	ip_handler BOOL OnIPViewMeasure(CIPCommand& cmd);
	//ip_handler BOOL OnIPViewMainMenu(CIPCommand& cmd);
	ip_handler BOOL OnIPViewBoostImage(CIPCommand& cmd);
	ip_handler BOOL OnIPViewZoom(CIPCommand& cmd);
	ip_handler BOOL OnIPViewShowText(CIPCommand& cmd);
	ip_handler BOOL OnIPViewPopup(CIPCommand& cmd);

	ip_handler BOOL OnIPDrawInit(CIPCommand& cmd);
	ip_handler BOOL OnIPDrawStart(CIPCommand& cmd);
	ip_handler BOOL OnIPDrawGetObject(CIPCommand& cmd);

	// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnPaint();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);

	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);

	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

	afx_msg void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);

	afx_msg void OnTimer(UINT_PTR nIDEvent);

	//----------------
	// Menu
	afx_msg void OnImagePause();

	afx_msg void OnImageLoad();

	afx_msg void OnImageSave();
	afx_msg void OnImageSaveEx();
	afx_msg void OnImageSaveRegion();
	afx_msg void OnVideoCapture();

	afx_msg void OnImagePatternMatching();
	afx_msg void OnImageFindEdge();
	afx_msg void OnImageFindCorner();
	afx_msg void OnImageFindLine();
	afx_msg void OnImageFindDot();
	afx_msg void OnImageFindSimpleObject();

	afx_msg void OnViewDisplayRegion();
	afx_msg void OnViewGrid();
	afx_msg void OnViewSlit();
	afx_msg void OnViewFocusValue();
	afx_msg void OnViewSelectedRegionSize();
	afx_msg void OnViewLaserCenter();
	afx_msg void OnViewMeasure();
	//afx_msg void OnViewToggleMainMenu();
	afx_msg void OnViewBoostImage();
	//afx_msg void OnUpdateViewDisplayRegion(CCmdUI* pCmdUI);
	//afx_msg void OnUpdateViewGrid(CCmdUI* pCmdUI);
	//afx_msg void OnUpdateViewLaserCenter(CCmdUI* pCmdUI);
	//afx_msg void OnUpdateViewGeometryInfo(CCmdUI* pCmdUI);
	afx_msg void OnViewPopup();

	//afx_msg void OnDrawInit();
	//afx_msg void OnDrawAddLine();
	//afx_msg void OnDrawAddPolyline();

	afx_msg void OnCalibrateCameraToStage();
	afx_msg void OnCalibrateLaserCenterOffset();
	afx_msg void OnCalibrateResetLaserCenterOffset();
	afx_msg void OnCalibrateSlit();

	afx_msg void OnSettingsStage();
	afx_msg void OnSettingsCamera();

	// Stage Functions (Lens, StageXY, ...)
protected:
	TRefPointer<CStageInterface> m_rStage;
	CCalibrationSlitDlg m_dlgCalibrationSlit;
public:
	BOOL IsFixedCamera() const { return m_rStage ? m_rStage->IsFixedXY() : TRUE; }
	int GetCurrentLens();
	BOOL SetCurrentLens(int iLensNo);
	CPoint2d GetStageXY();
	double GetStageZ();
	BOOL MoveStageTo(const CPoint& ptScreen, double dSpeed = -1, BOOL bPost = TRUE);
	BOOL MoveStageTo(const CPoint2d& pt, double dSpeed = -1, BOOL bPost = TRUE);
	BOOL MoveStageTo(double dStageX, double dStageY, double dSpeed = -1, BOOL bPost = TRUE);
	BOOL MoveStageToStep(double dStageX, double dStageY, double dSpeed = -1);
	BOOL MoveStageZTo(double dStageZ, double dSpeed = -1, BOOL bPost = TRUE);
	BOOL MoveStageZToStep(double dStageZ, double dSpeed = -1);
	BOOL MoveSlitTo(const CPoint2d& ptPulse, BOOL bPost = TRUE);
	BOOL MoveSlitToStep(const CPoint2d& ptPulse);
	BOOL MoveSlitAngleTo(double dTheta, BOOL bPost = TRUE);
	BOOL MoveSlitAngleToStep(double dTheta);

	BOOL SavePositionToVar(CIPVar& var, const TList<CPoint2d>* pptsStage = NULL, const TList<CPoint2d>* pptsImage = NULL, cv::Mat* pImg = NULL);
	BOOL SavePositionToVar(CIPVar& var, const CPoint2d* pptStage = NULL, const CPoint2d* pptImage = NULL, cv::Mat* pImg = NULL);
};
