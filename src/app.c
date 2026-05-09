#include "app.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <direct.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

typedef struct { AppContext* ctx; char repo[256]; } RefreshParam;
typedef struct { AppContext* ctx; int row; char final_path[MAX_PATH]; char part_path[MAX_PATH]; } DownloadParam;
typedef struct { BOOL success; int count; char error[256]; ReleaseAsset assets[MAX_RELEASE_ASSETS]; } RefreshResult;

static AppContext g_app;
static void app_create_controls(AppContext* ctx);
static void app_layout_controls(AppContext* ctx, int width, int height);
static void app_init_list_columns(AppContext* ctx);
static void app_set_busy_state(AppContext* c);
static LRESULT CALLBACK app_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static BOOL is_cancelled(void* ud) { return InterlockedCompareExchange((LONG*)ud, 0, 0) != 0; }

static DWORD WINAPI refresh_thread(LPVOID p) {
    RefreshParam* rp = (RefreshParam*)p;
    RefreshResult* rr = (RefreshResult*)malloc(sizeof(RefreshResult));
    rr->success = github_fetch_releases(rp->repo, rr->assets, MAX_RELEASE_ASSETS, &rr->count, rr->error, sizeof(rr->error));
    PostMessage(rp->ctx->hMainWnd, WM_APP_REFRESH_COMPLETE, 0, (LPARAM)rr);
    free(rp);
    return 0;
}

static void progress_cb(int prog, DWORD total, DWORD expected, void* ud) {
    DownloadParam* dp = (DownloadParam*)ud;
    PostMessage(dp->ctx->hMainWnd, WM_APP_DOWNLOAD_PROGRESS, (WPARAM)prog, (LPARAM)dp->row);
    app_log(dp->ctx, "%s => %lu/%lu (%d%%)", dp->ctx->assets[dp->row].file_name, total, expected, prog);
}

static void make_unique_path(const char* dir, const char* file, char* out, size_t out_sz) {
    char base[260], ext[64] = ""; strncpy(base, file, sizeof(base)-1); base[sizeof(base)-1]='\0';
    char* dot = strrchr(base, '.'); if (dot) { strncpy(ext, dot, sizeof(ext)-1); *dot='\0'; }
    snprintf(out, out_sz, "%s\\%s%s", dir, base, ext);
    for (int i=1; GetFileAttributes(out) != INVALID_FILE_ATTRIBUTES; ++i) snprintf(out, out_sz, "%s\\%s_%d%s", dir, base, i, ext);
}

static DWORD WINAPI download_thread(LPVOID p) {
    DownloadParam* dp = (DownloadParam*)p; AppContext* ctx = dp->ctx; ReleaseAsset* a = &ctx->assets[dp->row];
    BOOL cancelled = FALSE; char err[256] = {0};
    app_log(ctx, "Downloading file: %s", a->file_name);
    app_log(ctx, "Save path: %s", dp->final_path);
    app_log(ctx, "Total size: %lu bytes", a->size_bytes);
    BOOL ok = github_download_file(a->download_url, dp->part_path, a->size_bytes, progress_cb, is_cancelled, (void*)&ctx->cancel_download, &cancelled, err, sizeof(err));
    if (ok) {
        MoveFileEx(dp->part_path, dp->final_path, MOVEFILE_REPLACE_EXISTING);
        app_log(ctx, "Completed: %s", dp->final_path);
    } else {
        app_log(ctx, "%s", err);
        app_log(ctx, "Temporary file kept: %s", dp->part_path);
    }
    PostMessage(ctx->hMainWnd, WM_APP_DOWNLOAD_COMPLETE, (WPARAM)(ok?1:0), (LPARAM)(cancelled?-(dp->row+1):(dp->row+1)));
    free(dp); return 0;
}

BOOL app_init(HINSTANCE hInstance, int nCmdShow) { INITCOMMONCONTROLSEX icex={sizeof(icex),ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS}; InitCommonControlsEx(&icex);
    WNDCLASS wc={0}; wc.lpfnWndProc=app_wnd_proc; wc.hInstance=hInstance; wc.lpszClassName="VersionDownloadToolWnd"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); if(!RegisterClass(&wc)) return FALSE;
    ZeroMemory(&g_app,sizeof(g_app)); g_app.hInstance=hInstance; GetCurrentDirectory(sizeof(g_app.download_dir), g_app.download_dir); strcat(g_app.download_dir, "\\downloads");
    HWND hWnd=CreateWindowEx(0,wc.lpszClassName,APP_TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,APP_WIDTH,APP_HEIGHT,NULL,NULL,hInstance,&g_app); if(!hWnd) return FALSE;
    ShowWindow(hWnd,nCmdShow); UpdateWindow(hWnd); return TRUE; }

void app_log(AppContext* ctx, const char* fmt, ...) { if(!ctx||!ctx->hLogEdit) return; char m[1024]; va_list a; va_start(a,fmt); vsnprintf(m,sizeof(m),fmt,a); va_end(a); SYSTEMTIME st; GetLocalTime(&st); char l[1200]; snprintf(l,sizeof(l),"[%02d:%02d:%02d] %s\r\n",st.wHour,st.wMinute,st.wSecond,m); int len=GetWindowTextLength(ctx->hLogEdit); SendMessage(ctx->hLogEdit,EM_SETSEL,len,len); SendMessage(ctx->hLogEdit,EM_REPLACESEL,0,(LPARAM)l); }
void app_clear_releases(AppContext* ctx){ ListView_DeleteAllItems(ctx->hListView); ctx->asset_count=0; }
void app_add_release_row(AppContext* ctx,int no,const ReleaseAsset* a){ LVITEM item={0}; char noStr[16]; snprintf(noStr,sizeof(noStr),"%d",no); item.mask=LVIF_TEXT; item.iItem=ListView_GetItemCount(ctx->hListView); item.pszText=noStr; int row=ListView_InsertItem(ctx->hListView,&item); ListView_SetItemText(ctx->hListView,row,1,(LPSTR)a->version); ListView_SetItemText(ctx->hListView,row,2,(LPSTR)a->file_name); ListView_SetItemText(ctx->hListView,row,3,(LPSTR)a->size_text); ListView_SetItemText(ctx->hListView,row,4,(LPSTR)a->published_at); ListView_SetItemText(ctx->hListView,row,5,(LPSTR)a->status);} 

static void app_create_controls(AppContext* c){ HWND h=c->hMainWnd; c->hRepoEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","yunfei00/version-download-tool-c",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,0,0,0,0,h,(HMENU)IDC_EDIT_REPO,c->hInstance,NULL); c->hRefreshBtn=CreateWindow("BUTTON","Refresh Releases",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,0,0,0,h,(HMENU)IDC_BTN_REFRESH,c->hInstance,NULL); c->hDownloadBtn=CreateWindow("BUTTON","Download Selected",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,0,0,0,h,(HMENU)IDC_BTN_DOWNLOAD,c->hInstance,NULL); c->hSelectFolderBtn=CreateWindow("BUTTON","Select Folder",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,0,0,0,h,(HMENU)IDC_BTN_SELECT_FOLDER,c->hInstance,NULL); c->hCancelBtn=CreateWindow("BUTTON","Cancel Download",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,0,0,0,0,h,(HMENU)IDC_BTN_CANCEL_DOWNLOAD,c->hInstance,NULL); c->hFolderEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",c->download_dir,WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_READONLY,0,0,0,0,h,(HMENU)IDC_EDIT_FOLDER,c->hInstance,NULL); c->hListView=CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL,0,0,0,0,h,(HMENU)IDC_LIST_RELEASES,c->hInstance,NULL); ListView_SetExtendedListViewStyle(c->hListView,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES); c->hProgress=CreateWindowEx(0,PROGRESS_CLASS,"",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)IDC_PROGRESS_DOWNLOAD,c->hInstance,NULL); c->hLogEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,0,0,0,0,h,(HMENU)IDC_EDIT_LOG,c->hInstance,NULL); app_init_list_columns(c);} 
static void app_init_list_columns(AppContext* c){ LVCOLUMN col={0}; col.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; const char*t[]={"No","Version","File Name","Size","Published At","Status"}; int w[]={60,120,260,100,180,120}; for(int i=0;i<6;++i){col.iSubItem=i; col.pszText=(LPSTR)t[i]; col.cx=w[i]; ListView_InsertColumn(c->hListView,i,&col);} }
static void app_layout_controls(AppContext* c,int width,int height){ const int m=10,topH=28,btnW=130,g=6,progressH=22,logH=170; int repoW=width-m*2-btnW*4-g*4; MoveWindow(c->hRepoEdit,m,m,repoW,topH,TRUE); int x=m+repoW+g; MoveWindow(c->hRefreshBtn,x,m,btnW,topH,TRUE); x+=btnW+g; MoveWindow(c->hDownloadBtn,x,m,btnW,topH,TRUE); x+=btnW+g; MoveWindow(c->hSelectFolderBtn,x,m,btnW,topH,TRUE); x+=btnW+g; MoveWindow(c->hCancelBtn,x,m,btnW,topH,TRUE); int folderY=m+topH+6; MoveWindow(c->hFolderEdit,m,folderY,width-m*2,topH,TRUE); int listTop=folderY+topH+8; int listH=height-listTop-m-progressH-8-logH-8; MoveWindow(c->hListView,m,listTop,width-m*2,listH,TRUE); int pTop=listTop+listH+8; MoveWindow(c->hProgress,m,pTop,width-m*2,progressH,TRUE); MoveWindow(c->hLogEdit,m,pTop+progressH+8,width-m*2,logH,TRUE);} 
static void app_set_busy_state(AppContext* c){ BOOL busy = c->downloading!=0; EnableWindow(c->hRefreshBtn, !busy && !c->refreshing); EnableWindow(c->hDownloadBtn, !busy); EnableWindow(c->hSelectFolderBtn, !busy); }

static LRESULT CALLBACK app_wnd_proc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){ AppContext* c=(AppContext*)GetWindowLongPtr(hWnd,GWLP_USERDATA); switch(msg){ case WM_CREATE:{CREATESTRUCT*cs=(CREATESTRUCT*)lParam; c=(AppContext*)cs->lpCreateParams; c->hMainWnd=hWnd; SetWindowLongPtr(hWnd,GWLP_USERDATA,(LONG_PTR)c); app_create_controls(c); SendMessage(c->hProgress,PBM_SETRANGE,0,MAKELPARAM(0,100)); app_log(c,"Application initialized."); return 0;} case WM_SIZE: if(c) app_layout_controls(c,LOWORD(lParam),HIWORD(lParam)); return 0; case WM_COMMAND: if(!c) return 0; switch(LOWORD(wParam)){ case IDC_BTN_SELECT_FOLDER:{ BROWSEINFO bi={0}; bi.hwndOwner=hWnd; bi.lpszTitle="Select download folder"; PIDLIST_ABSOLUTE pidl=SHBrowseForFolder(&bi); if(pidl){ SHGetPathFromIDList(pidl,c->download_dir); CoTaskMemFree(pidl); SetWindowText(c->hFolderEdit,c->download_dir); app_log(c,"Download folder set: %s", c->download_dir);} break;} case IDC_BTN_CANCEL_DOWNLOAD:{ if(!c->downloading){ app_log(c,"No active download."); break;} InterlockedExchange(&c->cancel_download,1); app_log(c,"Download cancellation requested."); break;} case IDC_BTN_REFRESH:{ if(c->refreshing||c->downloading){app_log(c,"Refresh already in progress or downloading."); break;} char repo[256]; GetWindowText(c->hRepoEdit,repo,sizeof(repo)); app_clear_releases(c); c->refreshing=TRUE; app_set_busy_state(c); app_log(c,"Refreshing releases from GitHub..."); RefreshParam* rp=(RefreshParam*)malloc(sizeof(RefreshParam)); rp->ctx=c; strncpy(rp->repo,repo,sizeof(rp->repo)-1); rp->repo[sizeof(rp->repo)-1]='\0'; HANDLE t=CreateThread(NULL,0,refresh_thread,rp,0,NULL); if(t) CloseHandle(t); else { c->refreshing=FALSE; app_log(c,"Failed to create refresh worker thread."); } break;} case IDC_BTN_DOWNLOAD:{ if(c->downloading){app_log(c,"Download already in progress."); break;} int sel=ListView_GetNextItem(c->hListView,-1,LVNI_SELECTED); if(sel<0){app_log(c,"Please select a release asset first."); break;} _mkdir(c->download_dir); c->downloading=TRUE; c->cancel_download=0; app_set_busy_state(c); SendMessage(c->hProgress,PBM_SETPOS,0,0); ListView_SetItemText(c->hListView,sel,5,"Downloading"); DownloadParam* dp=(DownloadParam*)malloc(sizeof(DownloadParam)); dp->ctx=c; dp->row=sel; make_unique_path(c->download_dir, c->assets[sel].file_name, dp->final_path, sizeof(dp->final_path)); snprintf(dp->part_path, sizeof(dp->part_path), "%s.part", dp->final_path); HANDLE t=CreateThread(NULL,0,download_thread,dp,0,NULL); if(t) CloseHandle(t); else { c->downloading=FALSE; app_set_busy_state(c); app_log(c,"Failed to create download worker thread."); } break;} } return 0; case WM_APP_REFRESH_COMPLETE: if(c){ c->refreshing=FALSE; app_set_busy_state(c); RefreshResult* rr=(RefreshResult*)lParam; if(rr->success){ c->asset_count=rr->count; for(int i=0;i<rr->count;i++){ c->assets[i]=rr->assets[i]; app_add_release_row(c,i+1,&c->assets[i]); } app_log(c,"Loaded %d release assets.",rr->count);} else app_log(c,"%s",rr->error); free(rr);} return 0; case WM_APP_DOWNLOAD_PROGRESS: if(c){ SendMessage(c->hProgress,PBM_SETPOS,wParam,0); } return 0; case WM_APP_DOWNLOAD_COMPLETE: if(c){ c->downloading=FALSE; app_set_busy_state(c); int tag=(int)lParam; int row=abs(tag)-1; BOOL cancelled=tag<0; BOOL ok=(BOOL)wParam; ListView_SetItemText(c->hListView,row,5, cancelled?"Cancelled":(ok?"Done":"Failed")); app_log(c, cancelled?"Download cancelled.":(ok?"Download completed successfully.":"Download failed.")); } return 0; case WM_CLOSE: if(c && c->downloading){ InterlockedExchange(&c->cancel_download,1); app_log(c,"Window closing: waiting for download cancellation."); Sleep(100); } DestroyWindow(hWnd); return 0; case WM_DESTROY: PostQuitMessage(0); return 0;} return DefWindowProc(hWnd,msg,wParam,lParam);} 
