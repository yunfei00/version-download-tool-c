#include "app.h"
#include "version.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <direct.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

typedef struct { AppContext* ctx; char repo[256]; BOOL self_update; } RefreshParam;
typedef struct { AppContext* ctx; int row; char final_path[MAX_PATH]; char part_path[MAX_PATH]; } DownloadParam;
typedef struct { BOOL success; int count; char error[256]; ReleaseAsset assets[MAX_RELEASE_ASSETS]; } RefreshResult;
static AppContext g_app;

static BOOL semver_newer(const char* latest, const char* current){ int a=0,b=0,c=0,x=0,y=0,z=0; sscanf(latest,"v%d.%d.%d",&a,&b,&c); sscanf(current,"%d.%d.%d",&x,&y,&z); if(a!=x) return a>x; if(b!=y) return b>y; return c>z; }
static BOOL is_cancelled(void* ud) { return InterlockedCompareExchange((LONG*)ud, 0, 0) != 0; }
static void app_create_controls(AppContext* c); static void app_layout_controls(AppContext* c, int width, int height); static void app_init_list_columns(AppContext* c); static void app_set_busy_state(AppContext* c); static LRESULT CALLBACK app_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static DWORD WINAPI refresh_thread(LPVOID p){ RefreshParam* rp=(RefreshParam*)p; RefreshResult* rr=(RefreshResult*)malloc(sizeof(RefreshResult)); rr->success=github_fetch_releases(rp->repo,rr->assets,MAX_RELEASE_ASSETS,&rr->count,rr->error,sizeof(rr->error)); PostMessage(rp->ctx->hMainWnd,rp->self_update?WM_APP_SELF_UPDATE_COMPLETE:WM_APP_REFRESH_COMPLETE,0,(LPARAM)rr); free(rp); return 0; }
static void progress_cb(int prog,DWORD total,DWORD expected,void* ud){ DownloadParam* dp=(DownloadParam*)ud; PostMessage(dp->ctx->hMainWnd,WM_APP_DOWNLOAD_PROGRESS,(WPARAM)prog,(LPARAM)dp->row); app_log(dp->ctx,"%s => %lu/%lu (%d%%)",dp->ctx->assets[dp->row].file_name,total,expected,prog); }
static void make_unique_path(const char* dir,const char* file,char* out,size_t out_sz){ char base[260],ext[64]=""; strncpy(base,file,sizeof(base)-1); base[sizeof(base)-1]='\0'; char* dot=strrchr(base,'.'); if(dot){strncpy(ext,dot,sizeof(ext)-1);*dot='\0';} snprintf(out,out_sz,"%s\\%s%s",dir,base,ext); for(int i=1; GetFileAttributes(out)!=INVALID_FILE_ATTRIBUTES; ++i) snprintf(out,out_sz,"%s\\%s_%d%s",dir,base,i,ext);} 
static DWORD WINAPI download_thread(LPVOID p){ DownloadParam* dp=(DownloadParam*)p; AppContext* ctx=dp->ctx; ReleaseAsset* a=&ctx->assets[dp->row]; BOOL cancelled=FALSE; char err[256]={0}; BOOL ok=github_download_file(a->download_url,dp->part_path,a->size_bytes,progress_cb,is_cancelled,(void*)&ctx->cancel_download,&cancelled,err,sizeof(err)); if(ok){MoveFileEx(dp->part_path,dp->final_path,MOVEFILE_REPLACE_EXISTING); app_log(ctx,"Completed: %s",dp->final_path);} else app_log(ctx,"%s",err); PostMessage(ctx->hMainWnd,WM_APP_DOWNLOAD_COMPLETE,(WPARAM)(ok?1:0),(LPARAM)(cancelled?-(dp->row+1):(dp->row+1))); free(dp); return 0; }

BOOL app_init(HINSTANCE hInstance,int nCmdShow){ INITCOMMONCONTROLSEX icex={sizeof(icex),ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS}; InitCommonControlsEx(&icex);
    WNDCLASS wc={0}; wc.lpfnWndProc=app_wnd_proc; wc.hInstance=hInstance; wc.lpszClassName="VersionDownloadToolWnd"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1); if(!RegisterClass(&wc)) return FALSE;
    ZeroMemory(&g_app,sizeof(g_app)); g_app.hInstance=hInstance; config_load("app_config.ini",&g_app.config); strncpy(g_app.download_dir,g_app.config.download_dir,sizeof(g_app.download_dir)-1);
    char title[256]; snprintf(title,sizeof(title),"%s v%s",APP_NAME,APP_VERSION);
    HWND hWnd=CreateWindowEx(0,wc.lpszClassName,title,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,g_app.config.window_width,g_app.config.window_height,NULL,NULL,hInstance,&g_app); if(!hWnd) return FALSE;
    ShowWindow(hWnd,nCmdShow); UpdateWindow(hWnd); return TRUE; }

void app_log(AppContext* ctx,const char* fmt,...){ if(!ctx||!ctx->hLogEdit) return; char m[1024]; va_list a; va_start(a,fmt); vsnprintf(m,sizeof(m),fmt,a); va_end(a); SYSTEMTIME st; GetLocalTime(&st); char l[1200]; snprintf(l,sizeof(l),"[%02d:%02d:%02d] %s\r\n",st.wHour,st.wMinute,st.wSecond,m); int len=GetWindowTextLength(ctx->hLogEdit); SendMessage(ctx->hLogEdit,EM_SETSEL,len,len); SendMessage(ctx->hLogEdit,EM_REPLACESEL,0,(LPARAM)l);} 
void app_clear_releases(AppContext* ctx){ ListView_DeleteAllItems(ctx->hListView); ctx->asset_count=0; }
void app_add_release_row(AppContext* ctx,int no,const ReleaseAsset* a){ LVITEM item={0}; char n[16]; snprintf(n,sizeof(n),"%d",no); item.mask=LVIF_TEXT; item.iItem=ListView_GetItemCount(ctx->hListView); item.pszText=n; int row=ListView_InsertItem(ctx->hListView,&item); ListView_SetItemText(ctx->hListView,row,1,(LPSTR)a->version); ListView_SetItemText(ctx->hListView,row,2,(LPSTR)a->file_name); ListView_SetItemText(ctx->hListView,row,3,(LPSTR)a->size_text); ListView_SetItemText(ctx->hListView,row,4,(LPSTR)a->published_at); ListView_SetItemText(ctx->hListView,row,5,(LPSTR)a->status); }

static void app_create_controls(AppContext* c){ HWND h=c->hMainWnd; c->hRepoCombo=CreateWindowEx(WS_EX_CLIENTEDGE,"COMBOBOX","",WS_CHILD|WS_VISIBLE|CBS_DROPDOWN|WS_VSCROLL,0,0,0,0,h,(HMENU)IDC_COMBO_REPO,c->hInstance,NULL);
for(int i=0;i<c->config.recent_repo_count;i++) SendMessage(c->hRepoCombo,CB_ADDSTRING,0,(LPARAM)c->config.recent_repos[i]); SetWindowText(c->hRepoCombo,c->config.repo);
c->hRefreshBtn=CreateWindow("BUTTON","Refresh Releases",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)IDC_BTN_REFRESH,c->hInstance,NULL); c->hSelfUpdateBtn=CreateWindow("BUTTON","Check Self Update",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)IDC_BTN_SELF_UPDATE,c->hInstance,NULL); c->hDownloadBtn=CreateWindow("BUTTON","Download Selected",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)IDC_BTN_DOWNLOAD,c->hInstance,NULL); c->hSelectFolderBtn=CreateWindow("BUTTON","Select Folder",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)IDC_BTN_SELECT_FOLDER,c->hInstance,NULL); c->hCancelBtn=CreateWindow("BUTTON","Cancel Download",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)IDC_BTN_CANCEL_DOWNLOAD,c->hInstance,NULL); c->hFolderEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",c->download_dir,WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|ES_READONLY,0,0,0,0,h,(HMENU)IDC_EDIT_FOLDER,c->hInstance,NULL); c->hListView=CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,"",WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL,0,0,0,0,h,(HMENU)IDC_LIST_RELEASES,c->hInstance,NULL); ListView_SetExtendedListViewStyle(c->hListView,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES); c->hProgress=CreateWindowEx(0,PROGRESS_CLASS,"",WS_CHILD|WS_VISIBLE,0,0,0,0,h,(HMENU)IDC_PROGRESS_DOWNLOAD,c->hInstance,NULL); c->hLogEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,0,0,0,0,h,(HMENU)IDC_EDIT_LOG,c->hInstance,NULL); app_init_list_columns(c);
HMENU menu=CreateMenu(),help=CreatePopupMenu(); AppendMenu(help,MF_STRING,IDC_MENU_ABOUT,"About"); AppendMenu(menu,MF_POPUP,(UINT_PTR)help,"Help"); SetMenu(h,menu); }
static void app_init_list_columns(AppContext* c){ LVCOLUMN col={0}; col.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; const char*t[]={"No","Version","File Name","Size","Published At","Status"}; int w[]={60,120,250,100,180,120}; for(int i=0;i<6;++i){col.iSubItem=i; col.pszText=(LPSTR)t[i]; col.cx=w[i]; ListView_InsertColumn(c->hListView,i,&col);} }
static void app_layout_controls(AppContext* c,int width,int height){ const int m=10,topH=28,btnW=120,g=6,progressH=22,logH=170; int repoW=width-m*2-btnW*5-g*5; MoveWindow(c->hRepoCombo,m,m,repoW,250,TRUE); int x=m+repoW+g; MoveWindow(c->hRefreshBtn,x,m,btnW,topH,TRUE); x+=btnW+g; MoveWindow(c->hSelfUpdateBtn,x,m,btnW,topH,TRUE); x+=btnW+g; MoveWindow(c->hDownloadBtn,x,m,btnW,topH,TRUE); x+=btnW+g; MoveWindow(c->hSelectFolderBtn,x,m,btnW,topH,TRUE); x+=btnW+g; MoveWindow(c->hCancelBtn,x,m,btnW,topH,TRUE); int folderY=m+topH+6; MoveWindow(c->hFolderEdit,m,folderY,width-m*2,topH,TRUE); int listTop=folderY+topH+8; int listH=height-listTop-m-progressH-8-logH-8; MoveWindow(c->hListView,m,listTop,width-m*2,listH,TRUE); int pTop=listTop+listH+8; MoveWindow(c->hProgress,m,pTop,width-m*2,progressH,TRUE); MoveWindow(c->hLogEdit,m,pTop+progressH+8,width-m*2,logH,TRUE); }
static void app_set_busy_state(AppContext* c){ BOOL busy=c->downloading!=0; EnableWindow(c->hRefreshBtn,!busy&&!c->refreshing); EnableWindow(c->hSelfUpdateBtn,!busy&&!c->checking_update); }

static LRESULT CALLBACK app_wnd_proc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam){ AppContext* c=(AppContext*)GetWindowLongPtr(hWnd,GWLP_USERDATA); switch(msg){
case WM_CREATE:{CREATESTRUCT*cs=(CREATESTRUCT*)lParam; c=(AppContext*)cs->lpCreateParams; c->hMainWnd=hWnd; SetWindowLongPtr(hWnd,GWLP_USERDATA,(LONG_PTR)c); app_create_controls(c); SendMessage(c->hProgress,PBM_SETRANGE,0,MAKELPARAM(0,100)); return 0;}
case WM_SIZE: if(c) app_layout_controls(c,LOWORD(lParam),HIWORD(lParam)); return 0;
case WM_COMMAND: if(!c) return 0; switch(LOWORD(wParam)){
case IDC_MENU_ABOUT:{ char info[512]; snprintf(info,sizeof(info),"App Name: %s\nApp Version: %s\nBuild Version: %s\nTech Stack: C / Win32 API / WinHTTP\nGitHub Repo: %s",APP_NAME,APP_VERSION,BUILD_VERSION,APP_GITHUB_REPO); MessageBox(hWnd,info,"About",MB_OK|MB_ICONINFORMATION); break; }
case IDC_BTN_SELF_UPDATE:{ if(c->checking_update) break; c->checking_update=TRUE; app_set_busy_state(c); RefreshParam* rp=(RefreshParam*)malloc(sizeof(RefreshParam)); rp->ctx=c; rp->self_update=TRUE; strncpy(rp->repo,APP_GITHUB_REPO,sizeof(rp->repo)-1); rp->repo[sizeof(rp->repo)-1]='\0'; HANDLE t=CreateThread(NULL,0,refresh_thread,rp,0,NULL); if(t) CloseHandle(t); break; }
case IDC_BTN_SELECT_FOLDER:{ BROWSEINFO bi={0}; bi.hwndOwner=hWnd; bi.lpszTitle="Select download folder"; PIDLIST_ABSOLUTE pidl=SHBrowseForFolder(&bi); if(pidl){ SHGetPathFromIDList(pidl,c->download_dir); CoTaskMemFree(pidl); SetWindowText(c->hFolderEdit,c->download_dir);} break;}
case IDC_BTN_REFRESH:{ char repo[256]; GetWindowText(c->hRepoCombo,repo,sizeof(repo)); app_clear_releases(c); c->refreshing=TRUE; app_set_busy_state(c); RefreshParam* rp=(RefreshParam*)malloc(sizeof(RefreshParam)); rp->ctx=c; rp->self_update=FALSE; strncpy(rp->repo,repo,sizeof(rp->repo)-1); rp->repo[sizeof(rp->repo)-1]='\0'; HANDLE t=CreateThread(NULL,0,refresh_thread,rp,0,NULL); if(t) CloseHandle(t); break;}
} return 0;
case WM_APP_REFRESH_COMPLETE: if(c){ c->refreshing=FALSE; app_set_busy_state(c); RefreshResult* rr=(RefreshResult*)lParam; if(rr->success){ char repo[256]; GetWindowText(c->hRepoCombo,repo,sizeof(repo)); strncpy(c->config.repo,repo,sizeof(c->config.repo)-1); config_add_recent_repo(&c->config,repo); SendMessage(c->hRepoCombo,CB_RESETCONTENT,0,0); for(int i=0;i<c->config.recent_repo_count;i++) SendMessage(c->hRepoCombo,CB_ADDSTRING,0,(LPARAM)c->config.recent_repos[i]); SetWindowText(c->hRepoCombo,repo); c->asset_count=rr->count; for(int i=0;i<rr->count;i++){ c->assets[i]=rr->assets[i]; app_add_release_row(c,i+1,&c->assets[i]); } } else app_log(c,"%s",rr->error); free(rr);} return 0;
case WM_APP_SELF_UPDATE_COMPLETE: if(c){ c->checking_update=FALSE; app_set_busy_state(c); RefreshResult* rr=(RefreshResult*)lParam; if(rr->success && rr->count>0){ if(semver_newer(rr->assets[0].version,APP_VERSION)) app_log(c,"New version available: %s",rr->assets[0].version); else app_log(c,"You are using the latest version."); } else app_log(c,"Self update check failed."); free(rr);} return 0;
case WM_CLOSE: if(c){ GetWindowText(c->hRepoCombo,c->config.repo,sizeof(c->config.repo)); strncpy(c->config.download_dir,c->download_dir,sizeof(c->config.download_dir)-1); RECT r; GetWindowRect(hWnd,&r); c->config.window_width=r.right-r.left; c->config.window_height=r.bottom-r.top; config_save("app_config.ini",&c->config);} DestroyWindow(hWnd); return 0;
case WM_DESTROY: PostQuitMessage(0); return 0; }
return DefWindowProc(hWnd,msg,wParam,lParam); }
