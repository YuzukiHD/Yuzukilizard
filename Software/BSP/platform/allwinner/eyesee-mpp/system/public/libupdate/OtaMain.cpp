#include "OtaUpdate.h"
//#include "app_log.h"
#include "ota_private.h"
#define OTA_TIMER  1001
#define COST_TIME  120

#define db_msg printf
HWND     hMainWnd;

static int screen_w = 0;
static int screen_h = 0;
static int y_pos;

void drawProgress(HWND hwnd,int p,bool flag)
{
    HDC hdc = 0;
    // hdc = GetDC(hwnd);
    hdc = BeginPaint(hwnd);
    SetPenColor(HDC_SCREEN, RGBA2Pixel(HDC_SCREEN, 0xff, 0x00, 0x00, 0xff));
    db_msg("****p:%d******\n",p);
    if(p<230){
        TextOut(HDC_SCREEN , 80, y_pos-30, "OTA UPDATE NOW!");
        EndPaint(hwnd, hdc);
    }else{
        UpdateWindow(hwnd,1);
        if(flag )
            TextOut(HDC_SCREEN, 80, y_pos-30, "OTA UPDATE SUCCESSED!");
        else {
            TextOut(HDC_SCREEN, 80, y_pos-30, "OTA UPDATE FAILED!");
        }
    }
    Rectangle(HDC_SCREEN, 38, y_pos, screen_w-38, y_pos+40);
    SetPenWidth (HDC_SCREEN, 36);
    LineEx (HDC_SCREEN, 40, y_pos+20, 40+p, y_pos+20);

}

static int OTAPro(HWND hWnd,int  message, WPARAM wParam, LPARAM lParam)
{
    static int pos=0;
    switch(message)
    {
        case MSG_CREATE:
        {
            pos=0;
            SetTimer(hWnd,OTA_TIMER,100);
        }
            break;
        case MSG_TIMER:
            if(wParam == OTA_TIMER){
                pos+=1;
                //db_msg("****pos:%d******",pos);
                if(pos<(screen_w-90)){
                    drawProgress(hWnd, pos, false);
                }else{
                    drawProgress(hWnd, screen_w-90, false);
                }
            }
            break;
        case MSG_UPDATE_OVER:
            {
                db_msg("******MSG_UPDATE_OVER****wParam:%ud*****\n",wParam);
                if(wParam){
                    drawProgress(hWnd, screen_w-80, true);
                }else{
                    drawProgress(hWnd, screen_w-85, false);
                }
                sleep(2);
                system("reboot");
            }
            break;
        case MSG_CLOSE:
            KillTimer(hWnd,OTA_TIMER);
            DestroyMainWindow(hWnd);
            PostQuitMessage(hWnd);
            break;
        default:
            return DefaultMainWinProc(hWnd,message,wParam,lParam);
    }
    return 0;
}


static void getScreenInfo(int *w, int *h)
{
/*
    static int swidth = 0;
    static int sheight =0;
    if(swidth == 0 ){
        char *env = NULL;
        char mode[32]={0};
        if ((env=getenv("MG_DEFAULTMODE")) != NULL) {
            strncpy(mode, env, strlen(env)+1);
        } else {
            return;
        }
        char *pW = NULL;
        char *pH = NULL;
        pW = strstr(mode, "x");
        *pW = '\0';
        swidth = atoi(mode);
        *pW = 'x';
        pH = strstr(mode, "-");
        *pW = '\0';
        sheight = atoi(pW+1);
        *pW = '-';
    }
    *w = swidth;
    *h = sheight;
*/
    *w = 1280;
    *h = 720;
}

int initWindow()
{
    getScreenInfo(&screen_w, &screen_h);
    MAINWINCREATE CreateInfo;
    CreateInfo.dwStyle=WS_VISIBLE ;
    CreateInfo.dwExStyle = WS_EX_NONE | WS_EX_TROUNDCNS | WS_EX_BROUNDCNS;
    CreateInfo.spCaption = "OTA Update";
    CreateInfo.hMenu = 0;
    CreateInfo.hCursor = GetSystemCursor(0);
    CreateInfo.hIcon = 0;
    CreateInfo.MainWindowProc = OTAPro;
    CreateInfo.lx = 0;
    CreateInfo.ty = 0;
    CreateInfo.rx = screen_w;
    CreateInfo.by = screen_h;
    CreateInfo.iBkColor = COLOR_lightwhite;
    CreateInfo.dwAddData = 0;
    CreateInfo.hHosting = HWND_DESKTOP;

    y_pos = (int)(3*screen_h/8);
    //db_msg("y_pos:%d; screen_h:%d; screen_w:%d", y_pos, screen_h, screen_w);

    hMainWnd = CreateMainWindow(&CreateInfo);
    if(hMainWnd==HWND_INVALID)
        return 0;
    return 1;
}


int MiniGUIMain(int argc, const char* argv[])
{
    MSG msg;
    if(!initWindow()){
        fprintf(stderr,"create ota window failed");
    }
    ShowWindow(hMainWnd,SW_SHOWNORMAL);
    OtaUpdate *ota = new OtaUpdate(hMainWnd);
    ota->startUpdatePkg();
    while(GetMessage(&msg, hMainWnd)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    MainWindowThreadCleanup(hMainWnd);
    return 0;
}

int main_nodisplay(int argc, const char* argv[])
{

    ota_update_part_flg ota_part_flg;
    memset(&ota_part_flg, 0, sizeof(ota_update_part_flg));
    ota_part_flg.boot_logo_flag = 1;
    ota_part_flg.data_flag      = 1;
    ota_part_flg.env_flag       = 1;
    ota_part_flg.kernel_flag    = 1;
    ota_part_flg.misc_flag      = 1;
    ota_part_flg.rootfs_flag    = 1;
    ota_part_flg.uboot_flag     = 1;
    ota_part_flg.nor_flag       = 1;//nor flash
    ota_part_flg.nand_flag      = 1;//nor flash

    //ota_main(UPDATE_FILE_PATH_NAND, ota_part_flg);
    ota_main((char *)"/home/sun8iw12p1_linux_dvb_uart0.img", ota_part_flg);
    return 0;
}
