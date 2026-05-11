#include "app.h"
#include "version.h"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

typedef struct { AppContext* c; int idx; ULONGLONG start_tick; } DownloadParam;
typedef struct { AppContext* c; char url[2048]; } ScanParam;
static AppContext g;

static void logm(AppContext* c,const char* f,...){char b[1024];va_list a;va_start(a,f);vsnprintf(b,sizeof(b),f,a);va_end(a);int l=GetWindowTextLength(c->hLogEdit);SendMessage(c->hLogEdit,EM_SETSEL,l,l);SendMessage(c->hLogEdit,EM_REPLACESEL,0,(LPARAM)b);SendMessage(c->hLogEdit,EM_REPLACESEL,0,(LPARAM)"\r\n");}
static void refresh_row(AppContext* c,int i){char n[16],sz[64],pr[32];snprintf(n,16,"%d",i+1);snprintf(sz,64,"%llu",c->items[i].size_bytes);snprintf(pr,32,"%d%%",c->items[i].progress);ListView_SetItemText(c->hListView,i,0,n);ListView_SetItemText(c->hListView,i,1,c->items[i].file_name);ListView_SetItemText(c->hListView,i,2,c->items[i].url);ListView_SetItemText(c->hListView,i,3,sz);ListView_SetItemText(c->hListView,i,4,pr);ListView_SetItemText(c->hListView,i,5,c->items[i].status);} 
static void add_item(AppContext* c,const char* url){if(c->item_count>=MAX_DOWNLOAD_ITEMS)return;DownloadItem*it=&c->items[c->item_count];ZeroMemory(it,sizeof(*it));strncpy(it->url,url,sizeof(it->url)-1);parse_file_name_from_url(url,it->file_name,sizeof(it->file_name));strcpy(it->status,"Pending");it->size_bytes=http_get_file_size(url);LVITEM v={0};v.mask=LVIF_TEXT;v.iItem=c->item_count;v.pszText="";ListView_InsertItem(c->hListView,&v);refresh_row(c,c->item_count);c->item_count++;}
static BOOL cancelled(void* ud){return InterlockedCompareExchange((LONG*)ud,0,0)!=0;}
static void dcb(unsigned long long d,unsigned long long t,void* ud){DownloadParam*p=(DownloadParam*)ud;DownloadItem*it=&p->c->items[p->idx];it->downloaded_bytes=d;it->size_bytes=t;it->progress=t? (int)(d*100/t):0;ULONGLONG ms=GetTickCount64()-p->start_tick;double speed=ms? (d/1024.0)/(ms/1000.0):0;double eta=(speed>0&&t>d)?((t-d)/1024.0)/speed:0;char st[64];snprintf(st,64,"Downloading %.1fKB/s ETA %.0fs",speed,eta);strncpy(it->status,st,sizeof(it->status)-1);PostMessage(p->c->hMainWnd,WM_APP_DOWNLOAD_PROGRESS,p->idx,0);} 
static void unique(char*out,size_t n,const char*dir,const char*name){char b[260],e[64]="";strncpy(b,name,259);char*dot=strrchr(b,'.');if(dot){strncpy(e,dot,63);*dot='\0';}snprintf(out,n,"%s\\%s",dir,name);for(int i=1;GetFileAttributes(out)!=INVALID_FILE_ATTRIBUTES;i++)snprintf(out,n,"%s\\%s_%d%s",dir,b,i,e);} 
static DWORD WINAPI download_thread(LPVOID lp){DownloadParam*p=(DownloadParam*)lp;AppContext*c=p->c;DownloadItem*it=&c->items[p->idx];char final[MAX_PATH],part[MAX_PATH];unique(final,sizeof(final),c->download_dir,it->file_name);snprintf(part,sizeof(part),"%s.part",final);strncpy(it->save_path,final,sizeof(it->save_path)-1);strcpy(it->status,"Downloading");BOOL ok=http_download_file(it->url,part,dcb,cancelled,(void*)&c->cancel_download);if(ok){MoveFileEx(part,final,MOVEFILE_REPLACE_EXISTING);strcpy(it->status,"Completed");it->progress=100;} else {DeleteFile(part);strcpy(it->status,c->cancel_download?"Cancelled":"Failed");}PostMessage(c->hMainWnd,WM_APP_DOWNLOAD_COMPLETE,p->idx,0);free(p);return 0;}
static DWORD WINAPI scan_thread(LPVOID lp){ScanParam*p=(ScanParam*)lp;DownloadItem tmp[512];int n=http_scan_directory(p->url,tmp,512);for(int i=0;i<n;i++) add_item(p->c,tmp[i].url);PostMessage(p->c->hMainWnd,WM_APP_SCAN_COMPLETE,n,0);free(p);return 0;}

static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){AppContext*c=(AppContext*)GetWindowLongPtr(h,GWLP_USERDATA);switch(m){
case WM_CREATE:{CREATESTRUCT*cs=(CREATESTRUCT*)l;c=(AppContext*)cs->lpCreateParams;SetWindowLongPtr(h,GWLP_USERDATA,(LONG_PTR)c);c->hMainWnd=h;
 c->hUrlEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_MULTILINE|WS_VSCROLL,10,10,640,70,h,(HMENU)IDC_EDIT_URL,c->hInstance,NULL);
 c->hAddBtn=CreateWindow("BUTTON","Add URL",WS_CHILD|WS_VISIBLE,660,10,120,28,h,(HMENU)IDC_BTN_ADD_URL,c->hInstance,NULL);
 c->hScanBtn=CreateWindow("BUTTON","Scan HTTP Path",WS_CHILD|WS_VISIBLE,660,42,120,28,h,(HMENU)IDC_BTN_SCAN_PATH,c->hInstance,NULL);
 c->hDownloadSelectedBtn=CreateWindow("BUTTON","Download Selected",WS_CHILD|WS_VISIBLE,790,10,140,28,h,(HMENU)IDC_BTN_DOWNLOAD_SELECTED,c->hInstance,NULL);
 c->hDownloadAllBtn=CreateWindow("BUTTON","Download All",WS_CHILD|WS_VISIBLE,790,42,140,28,h,(HMENU)IDC_BTN_DOWNLOAD_ALL,c->hInstance,NULL);
 c->hSelectFolderBtn=CreateWindow("BUTTON","Select Folder",WS_CHILD|WS_VISIBLE,940,10,120,28,h,(HMENU)IDC_BTN_SELECT_FOLDER,c->hInstance,NULL);
 c->hFolderEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT",c->download_dir,WS_CHILD|WS_VISIBLE|ES_READONLY,940,42,120,28,h,(HMENU)IDC_EDIT_FOLDER,c->hInstance,NULL);
 c->hListView=CreateWindowEx(WS_EX_CLIENTEDGE,WC_LISTVIEW,"",WS_CHILD|WS_VISIBLE|LVS_REPORT,10,90,1050,360,h,(HMENU)IDC_LIST_ITEMS,c->hInstance,NULL);ListView_SetExtendedListViewStyle(c->hListView,LVS_EX_FULLROWSELECT|LVS_EX_GRIDLINES);
 const char*tt[]={"No","File Name","URL","Size","Progress","Status"};int ww[]={50,180,470,90,90,160};for(int i=0;i<6;i++){LVCOLUMN col={.mask=LVCF_TEXT|LVCF_WIDTH,.pszText=(LPSTR)tt[i],.cx=ww[i]};ListView_InsertColumn(c->hListView,i,&col);} 
 c->hProgress=CreateWindowEx(0,PROGRESS_CLASS,"",WS_CHILD|WS_VISIBLE,10,460,1050,24,h,(HMENU)IDC_PROGRESS_TOTAL,c->hInstance,NULL);
 c->hLogEdit=CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,10,490,1050,140,h,(HMENU)IDC_EDIT_LOG,c->hInstance,NULL);
 break;}
case WM_COMMAND: switch(LOWORD(w)){
 case IDC_BTN_SELECT_FOLDER:{BROWSEINFO bi={0};bi.hwndOwner=h;PIDLIST_ABSOLUTE id=SHBrowseForFolder(&bi);if(id){SHGetPathFromIDList(id,c->download_dir);CoTaskMemFree(id);SetWindowText(c->hFolderEdit,c->download_dir);}break;}
 case IDC_BTN_ADD_URL:{char text[8192];GetWindowText(c->hUrlEdit,text,sizeof(text));char*ctx=NULL;char*line=strtok_s(text,"\r\n",&ctx);while(line){if(*line) add_item(c,line);line=strtok_s(NULL,"\r\n",&ctx);}break;}
 case IDC_BTN_SCAN_PATH:{char url[2048];GetWindowText(c->hUrlEdit,url,sizeof(url));ScanParam*p=malloc(sizeof(ScanParam));p->c=c;strncpy(p->url,url,sizeof(p->url)-1);CreateThread(NULL,0,scan_thread,p,0,NULL);break;}
 case IDC_BTN_DOWNLOAD_SELECTED:{int i=ListView_GetNextItem(c->hListView,-1,LVNI_SELECTED);if(i>=0){InterlockedExchange(&c->cancel_download,0);DownloadParam*p=malloc(sizeof(DownloadParam));p->c=c;p->idx=i;p->start_tick=GetTickCount64();CreateThread(NULL,0,download_thread,p,0,NULL);}break;}
 case IDC_BTN_DOWNLOAD_ALL:{for(int i=0;i<c->item_count;i++){if(strcmp(c->items[i].status,"Pending")==0||strcmp(c->items[i].status,"Failed")==0){InterlockedExchange(&c->cancel_download,0);DownloadParam*p=malloc(sizeof(DownloadParam));p->c=c;p->idx=i;p->start_tick=GetTickCount64();CreateThread(NULL,0,download_thread,p,0,NULL);Sleep(20);}}break;}
 } break;
case WM_APP_DOWNLOAD_PROGRESS: refresh_row(c,(int)w); break;
case WM_APP_DOWNLOAD_COMPLETE: refresh_row(c,(int)w); break;
case WM_APP_SCAN_COMPLETE: logm(c,"Scan complete: %d urls added",(int)w); break;
case WM_CLOSE: strncpy(c->config.download_dir,c->download_dir,sizeof(c->config.download_dir)-1);config_save("app_config.ini",&c->config);DestroyWindow(h);break;
case WM_DESTROY: PostQuitMessage(0); break;
}return DefWindowProc(h,m,w,l);} 

BOOL app_init(HINSTANCE hInstance,int nCmdShow){INITCOMMONCONTROLSEX i={sizeof(i),ICC_LISTVIEW_CLASSES|ICC_PROGRESS_CLASS};InitCommonControlsEx(&i);ZeroMemory(&g,sizeof(g));g.hInstance=hInstance;config_load("app_config.ini",&g.config);strncpy(g.download_dir,g.config.download_dir,sizeof(g.download_dir)-1);
WNDCLASS wc={0};wc.lpfnWndProc=proc;wc.hInstance=hInstance;wc.lpszClassName="HttpPathDownloader";wc.hCursor=LoadCursor(NULL,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);RegisterClass(&wc);
HWND h=CreateWindow(wc.lpszClassName,APP_TITLE,WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,g.config.window_width,g.config.window_height,NULL,NULL,hInstance,&g);if(!h)return FALSE;ShowWindow(h,nCmdShow);UpdateWindow(h);return TRUE;}
