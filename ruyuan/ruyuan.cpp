#include<iostream>
#include<conio.h>
#include<windows.h>
#include<io.h>
#include<fcntl.h>
using namespace std;

bool pk(int vk){return (GetAsyncKeyState(vk)&0x8000);}
void gotoxy(int x,int y){COORD pos={(short)x,(short)y};HANDLE hOut=GetStdHandle(STD_OUTPUT_HANDLE);SetConsoleCursorPosition(hOut,pos);}
void hidecur(){CONSOLE_CURSOR_INFO cursor_info={1,0};SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE),&cursor_info);}
void putnstr(int n, string s){for(int _=1;_<=n;_++)cout << s;}
void noedit(){HANDLE hStdin=GetStdHandle(STD_INPUT_HANDLE);DWORD mode;GetConsoleMode(hStdin,&mode);mode&=~ENABLE_QUICK_EDIT_MODE;mode&=~ENABLE_INSERT_MODE;mode&=~ENABLE_MOUSE_INPUT;SetConsoleMode(hStdin,mode);}
void setcolor(int x){SetConsoleTextAttribute(GetStdHandle(4294967285u),x);}

struct{
	wstring s;
	int x;
} words[]={
	{L"你是 遥遥的路", 3},
	{L"山野大雾里的灯", 9},
	{L"我是孩童啊 走在你的眼眸", 9},
	{L"你是 明月清风", 5},
	{L"我是你照拂的梦", 6},
	{L"见与不见都 一生与你相拥", 4},
	{L"而我将爱你所爱的人间", 3},
	{L"愿你所愿的笑颜", 3},
	{L"你的手我蹒跚在牵", 3},
	{L"请带我去明天", 4},
	{L"如果说你曾苦过我的甜", 3},
	{L"我愿活成你的愿", 3},
	{L"愿不枉啊 愿勇往啊", 3},
	{L"这盛世每一天", 20},
	{L"你是 岁月长河", 3},
	{L"星火燃起的天空", 9},
	{L"我是仰望者 就把你唱成歌", 9},
	{L"你是 我之所来", 5},
	{L"也是我心之所归", 6},
	{L"世间所有路 都将与你相逢", 4},
	{L"而我将爱你所爱的人间", 3},
	{L"愿你所愿的笑颜", 3},
	{L"你的手我蹒跚在牵", 3},
	{L"请带我去明天", 4},
	{L"如果说你曾苦过我的甜", 3},
	{L"我愿活成你的愿", 3},
	{L"愿不枉啊 愿勇往啊", 3},
	{L"这盛世每一天", 6},
	{L"山河无恙 烟火寻常", 2},
	{L"可是你如愿的眺望", 4},
	{L"孩子们啊 安睡梦乡", 2},
	{L"像你深爱的那样", 6},
	{L"而我将梦你所梦的团圆", 3},
	{L"愿你所愿的永远", 3},
	{L"走你所走的长路", 3},
	{L"这样的爱你啊", 4},
	{L"我也将见你未见的世界", 3},
	{L"写你未写的诗篇", 3},
	{L"天边的月 心中的念", 3},
	{L"你永在我身边", 9},
	{L"与你相约 一生清澈", 12},
	{L"如你年轻的脸", 20},
	{L"", 0},
};

void putCenter(wstring x, int len, int pos){
	for(int i=1;i<=(len-x.size())/2;i++){
		wcout << " ";
	}
	for(int i=0;i<x.size();i++){
		if(i<=pos){
			wcout << x[i];
		}
		else{
			setcolor(0x8);
			wcout << x[i];
			setcolor(0x7);
		}
	}
	for(int i=1;i<=(len-x.size())/2;i++){
		wcout << " ";
	}
	wcout << "\n";
}
void clearScreen(){
	gotoxy(0, 0);
	wcout << L"                                                                                              \n";
	wcout << L"                                                                                              \n";
	wcout << L"                                                                                              \n";
}

int main(){
	hidecur();
	noedit();
	_setmode(_fileno(stdout), _O_U16TEXT);
	SetWindowPos(GetConsoleWindow(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	for(int i=0;i<54;i++){
		clearScreen();
		gotoxy(0, 0);
		if(i%2==0){
			putCenter(words[i].s, 50, -1);
			putCenter(words[i+1].s, 50, -1);
		}
		else{
			putCenter(words[i+1].s, 50, -1);
			putCenter(words[i].s, 50, -1);
		}

		if(i==0) Sleep(5000);

		if(i%2==0){
			for(int j=0;j<words[i].s.size();j++){
				if(words[i].s[j]==' ') Sleep(300);
				clearScreen();
				gotoxy(0, 0);
				putCenter(words[i].s, 50, j);
				putCenter(words[i+1].s, 50, -1);
				Sleep(280);
			}
		}
		else{
			for(int j=0;j<words[i].s.size();j++){
				if(words[i].s[j]==' ') Sleep(300);
				clearScreen();
				gotoxy(0, 0);
				putCenter(words[i+1].s, 50, -1);
				putCenter(words[i].s, 50, j);
				Sleep(280);
			}
		}
		Sleep(words[i].x*250);
	}
	system("pause");
	return 0;
}

/*

Idea from FJC-2013XH

*/
