/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "mplayerc.h"
#include "PPageFileInfoDetails.h"
#include <atlbase.h>
#include "DSUtil.h"
#include "text.h"
#include <d3d9.h>
#include <vmr9.h>
#include "moreuuids.h"


// CPPageFileInfoDetails dialog

IMPLEMENT_DYNAMIC(CPPageFileInfoDetails, CPropertyPage)
CPPageFileInfoDetails::CPPageFileInfoDetails(CString path, IFilterGraph* pFG, ISubPicAllocatorPresenter* pCAP, IFileSourceFilter* pFSF)
    : CPropertyPage(CPPageFileInfoDetails::IDD, CPPageFileInfoDetails::IDD)
    , m_fn(path)
    , m_path(path)
    , m_pFG(pFG)
    , m_pCAP(pCAP)
    , m_pFSF(pFSF)
    , m_hIcon(nullptr)
    , m_type(ResStr(IDS_AG_NOT_KNOWN))
    , m_size(ResStr(IDS_AG_NOT_KNOWN))
    , m_time(ResStr(IDS_AG_NOT_KNOWN))
    , m_res(ResStr(IDS_AG_NOT_KNOWN))
    , m_created(ResStr(IDS_AG_NOT_KNOWN))
{
}

CPPageFileInfoDetails::~CPPageFileInfoDetails()
{
    if (m_hIcon) {
        DestroyIcon(m_hIcon);
    }
}

void CPPageFileInfoDetails::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_DEFAULTICON, m_icon);
    DDX_Text(pDX, IDC_EDIT1, m_fn);
    DDX_Text(pDX, IDC_EDIT4, m_type);
    DDX_Text(pDX, IDC_EDIT3, m_size);
    DDX_Text(pDX, IDC_EDIT2, m_time);
    DDX_Text(pDX, IDC_EDIT5, m_res);
    DDX_Text(pDX, IDC_EDIT6, m_created);
    DDX_Control(pDX, IDC_EDIT7, m_encoding);
}

#define SETPAGEFOCUS (WM_APP + 252) // arbitrary number, can be changed if necessary

BEGIN_MESSAGE_MAP(CPPageFileInfoDetails, CPropertyPage)
    ON_MESSAGE(SETPAGEFOCUS, OnSetPageFocus)
END_MESSAGE_MAP()

// CPPageFileInfoDetails message handlers

static bool GetProperty(IFilterGraph* pFG, LPCOLESTR propName, VARIANT* vt)
{
    BeginEnumFilters(pFG, pEF, pBF) {
        if (CComQIPtr<IPropertyBag> pPB = pBF)
            if (SUCCEEDED(pPB->Read(propName, vt, nullptr))) {
                return true;
            }
    }
    EndEnumFilters;

    return false;
}

static CString FormatDateTime(FILETIME tm)
{
    SYSTEMTIME st;
    FileTimeToSystemTime(&tm, &st);
    TCHAR buff[256];
    GetDateFormat(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, nullptr, buff, _countof(buff));
    CString ret(buff);
    ret += _T(" ");
    GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, nullptr, buff, _countof(buff));
    ret += buff;
    return ret;
}

BOOL CPPageFileInfoDetails::OnInitDialog()
{
    __super::OnInitDialog();

    if (m_pFSF) {
        LPOLESTR pFN;
        if (SUCCEEDED(m_pFSF->GetCurFile(&pFN, nullptr))) {
            m_fn = pFN;
            CoTaskMemFree(pFN);
        }
    }

    if (m_path.IsEmpty()) {
        m_path = m_fn;
    }

    m_fn.TrimRight('/');
    m_fn.Replace('\\', '/');
    m_fn = m_fn.Mid(m_fn.ReverseFind('/') + 1);

    CString ext = m_fn.Left(m_fn.Find(_T("://")) + 1).TrimRight(':');
    if (ext.IsEmpty() || !ext.CompareNoCase(_T("file"))) {
        ext = _T(".") + m_fn.Mid(m_fn.ReverseFind('.') + 1);
    }

    m_hIcon = LoadIcon(m_fn, false);
    if (m_hIcon) {
        m_icon.SetIcon(m_hIcon);
    }

    if (!LoadType(ext, m_type)) {
        m_type.LoadString(IDS_AG_NOT_KNOWN);
    }

    CString created;
    CComVariant vt;
    if (::GetProperty(m_pFG, L"CurFile.TimeCreated", &vt)) {
        if (V_VT(&vt) == VT_UI8) {
            ULARGE_INTEGER uli;
            uli.QuadPart = V_UI8(&vt);

            FILETIME ft;
            ft.dwLowDateTime = uli.LowPart;
            ft.dwHighDateTime = uli.HighPart;

            created = FormatDateTime(ft);
        }
    }

    __int64 size = 0;
    if (CComQIPtr<IBaseFilter> pBF = m_pFSF) {
        BeginEnumPins(pBF, pEP, pPin) {
            if (CComQIPtr<IAsyncReader> pAR = pPin) {
                LONGLONG total, available;
                if (SUCCEEDED(pAR->Length(&total, &available))) {
                    size = total;
                    break;
                }
            }
        }
        EndEnumPins;
    }

    WIN32_FIND_DATA wfd;
    HANDLE hFind = FindFirstFile(m_path, &wfd);
    if (hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);

        if (size == 0) {
            size = (__int64(wfd.nFileSizeHigh) << 32) | wfd.nFileSizeLow;
        }

        if (created.IsEmpty()) {
            created = FormatDateTime(wfd.ftCreationTime);
        }
    }

    if (size > 0) {
        const int MAX_FILE_SIZE_BUFFER = 65;
        WCHAR szFileSize[MAX_FILE_SIZE_BUFFER];
        StrFormatByteSizeW(size, szFileSize, MAX_FILE_SIZE_BUFFER);
        CString szByteSize;
        szByteSize.Format(_T("%I64d"), size);
        m_size.Format(_T("%s (%s bytes)"), szFileSize, FormatNumber(szByteSize));
    }

    if (!created.IsEmpty()) {
        m_created = created;
    }

    REFERENCE_TIME rtDur = 0;
    CComQIPtr<IMediaSeeking> pMS = m_pFG;
    if (pMS && SUCCEEDED(pMS->GetDuration(&rtDur)) && rtDur > 0) {
        m_time = ReftimeToString2(rtDur);
    }

    CSize wh(0, 0), arxy(0, 0);

    if (m_pCAP) {
        wh = m_pCAP->GetVideoSize(false);
        arxy = m_pCAP->GetVideoSize(true);
    } else {
        if (CComQIPtr<IBasicVideo> pBV = m_pFG) {
            if (SUCCEEDED(pBV->GetVideoSize(&wh.cx, &wh.cy))) {
                if (CComQIPtr<IBasicVideo2> pBV2 = m_pFG) {
                    pBV2->GetPreferredAspectRatio(&arxy.cx, &arxy.cy);
                }
            } else {
                wh.SetSize(0, 0);
            }
        }

        if (wh.cx == 0 && wh.cy == 0) {
            BeginEnumFilters(m_pFG, pEF, pBF) {
                if (CComQIPtr<IBasicVideo> pBV = pBF) {
                    pBV->GetVideoSize(&wh.cx, &wh.cy);
                    if (CComQIPtr<IBasicVideo2> pBV2 = pBF) {
                        pBV2->GetPreferredAspectRatio(&arxy.cx, &arxy.cy);
                    }
                    break;
                } else if (CComQIPtr<IVMRWindowlessControl> pWC = pBF) {
                    pWC->GetNativeVideoSize(&wh.cx, &wh.cy, &arxy.cx, &arxy.cy);
                    break;
                } else if (CComQIPtr<IVMRWindowlessControl9> pWC9 = pBF) {
                    pWC9->GetNativeVideoSize(&wh.cx, &wh.cy, &arxy.cx, &arxy.cy);
                    break;
                }
            }
            EndEnumFilters;
        }
    }

    if (wh.cx > 0 && wh.cy > 0) {
        m_res.Format(_T("%dx%d"), wh.cx, wh.cy);

        int gcd = GCD(arxy.cx, arxy.cy);
        if (gcd > 1) {
            arxy.cx /= gcd;
            arxy.cy /= gcd;
        }

        if (arxy.cx > 0 && arxy.cy > 0 && arxy.cx * wh.cy != arxy.cy * wh.cx) {
            CString ar;
            ar.Format(_T(" (AR %d:%d)"), arxy.cx, arxy.cy);
            m_res += ar;
        }
    }

    UpdateData(FALSE);

    InitEncoding();

    m_pFG = nullptr;
    m_pCAP = nullptr;

    return TRUE;  // return TRUE unless you set the focus to a control
    // EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CPPageFileInfoDetails::OnSetActive()
{
    BOOL ret = __super::OnSetActive();
    PostMessage(SETPAGEFOCUS, 0, 0L);
    return ret;
}

LRESULT CPPageFileInfoDetails::OnSetPageFocus(WPARAM wParam, LPARAM lParam)
{
    CPropertySheet* psheet = (CPropertySheet*) GetParent();
    psheet->GetTabControl()->SetFocus();
    return 0;
}

void CPPageFileInfoDetails::InitEncoding()
{
    CAtlList<CString> sl;

    BeginEnumFilters(m_pFG, pEF, pBF) {
        CComPtr<IBaseFilter> pUSBF = GetUpStreamFilter(pBF);

        if (GetCLSID(pBF) == CLSID_NetShowSource) {
            continue;
        } else if (GetCLSID(pUSBF) != CLSID_NetShowSource) {
            if (IPin* pPin = GetFirstPin(pBF, PINDIR_INPUT)) {
                CMediaType mt;
                if (FAILED(pPin->ConnectionMediaType(&mt)) || mt.majortype != MEDIATYPE_Stream) {
                    continue;
                }
            }

            if (IPin* pPin = GetFirstPin(pBF, PINDIR_OUTPUT)) {
                CMediaType mt;
                if (SUCCEEDED(pPin->ConnectionMediaType(&mt)) && mt.majortype == MEDIATYPE_Stream) {
                    continue;
                }
            }
        }

        BeginEnumPins(pBF, pEP, pPin) {
            CMediaTypeEx mt;
            PIN_DIRECTION dir;
            if (FAILED(pPin->QueryDirection(&dir)) || dir != PINDIR_OUTPUT
                    || FAILED(pPin->ConnectionMediaType(&mt))) {
                continue;
            }

            CString str = mt.ToString();

            if (!str.IsEmpty()) {
                if (mt.majortype == MEDIATYPE_Video) { // Sort streams, set Video streams at head
                    bool found_video = false;
                    for (POSITION pos = sl.GetTailPosition(); pos; sl.GetPrev(pos)) {
                        CString Item = sl.GetAt(pos);
                        if (!Item.Find(_T("Video:"))) {
                            sl.InsertAfter(pos, str + CString(L" [" + GetPinName(pPin) + L"]"));
                            found_video = true;
                            break;
                        }
                    }
                    if (!found_video) {
                        sl.AddHead(str + CString(L" [" + GetPinName(pPin) + L"]"));
                    }
                } else {
                    sl.AddTail(str + CString(L" [" + GetPinName(pPin) + L"]"));
                }
            }
        }
        EndEnumPins;
    }
    EndEnumFilters;

    CString text = Implode(sl, '\n');
    text.Replace(_T("\n"), _T("\r\n"));
    m_encoding.SetWindowText(text);
}
