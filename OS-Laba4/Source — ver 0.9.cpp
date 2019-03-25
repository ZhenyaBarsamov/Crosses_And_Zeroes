#define _CRT_SECURE_NO_WARNINGS //��� ������������� ������� sprintf
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string>


using namespace std;

struct WindowParams {
	//�������
	int width = 320;
	int height = 240;
	//����������
	int xPos = 100;
	int yPos = 100;
	//���
	int backgroundColorRed = 0;
	int backgroundColorGreen = 0;
	int backgroundColorBlue = 255;
	//����� ��������
	string crossPictureFileName = "Cross.png";
	string zeroPictureFileName = "Zero.jpg";
	//������ � ������ ����(� �������)
	int fieldSize = 3;
} hWndParams; //��������� ����

string nameOfCfgFile = "CFGFile.txt";


struct GameCell {
	int content = 0; //���������� ������(0-�����, 1-������ �����, 2-������ �����)
	int top, bottom, left, right; //���������� ������
	int pictureCoordX, pictureCoordY; //���������� �������� � ���
};

GameCell** gameField; //����

int currentPlayerId; //id ������ ����� ��������
int lastSteppedPlayerId; //id ������, ������� ����� ���������



//�������� ��������� ����������
HANDLE playerIdMutex; //������� ��� ������ ������ id
HANDLE playersCountSem; //��������� ������� ��� ����, ���� ��������� � ���� ������ ����� �������


//�������� ��� ������� ��������� ����� �����-����������� ������
//share - ��� ���� ����������� �������, 
//� INVALID_HANDLE_VALUE ������� � ���, ��� ������ ����������� �� "���������" � �����
//�����������, ��������� � ������ ��������� � ���������� ������, ��������� �� ���� (�����������) ������� ������

struct SharedMemory {
	//4096 ���� - �������� ������ �������� ������
	HANDLE hmap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, "sharedMemory");
	char* map = (char*)MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
} sharedMemory;

void CloseSharedMemory() {
	UnmapViewOfFile(sharedMemory.map);
	CloseHandle(sharedMemory.hmap);
}

//��������� ������ ��� ������ � ����������� ������
char* GamedataToBuf() {
	string s;
	for (int y = 0; y < hWndParams.fieldSize; y++)
		for (int x = 0; x < hWndParams.fieldSize; x++)
			s += to_string(gameField[y][x].content);
	s += to_string(currentPlayerId);

	char *buf = new char[hWndParams.fieldSize * hWndParams.fieldSize + 1];
	sprintf(buf, s.c_str());

	return buf;
}

//���������� ���� � ������������ � �������
void BufToGamedata(char* buf) {
	int  i = 0; //��� ������� �� ������
	for (int y = 0; y < hWndParams.fieldSize; y++)
		for (int x = 0; x < hWndParams.fieldSize; x++) {
			gameField[y][x].content = buf[i] - '0';
			i++;
		}

	lastSteppedPlayerId = buf[hWndParams.fieldSize * hWndParams.fieldSize] - '0';
}



//���������� ��������� ������
void GameFieldResize(RECT screenSize) {
	int cellHeigth = screenSize.bottom / hWndParams.fieldSize;
	int cellWidth = screenSize.right / hWndParams.fieldSize;
	for (int y = 0; y < hWndParams.fieldSize; y++)
		for (int x = 0; x < hWndParams.fieldSize; x++) {
			gameField[y][x].top = screenSize.top + y * cellHeigth;
			gameField[y][x].left = screenSize.left + x * cellWidth;
			gameField[y][x].bottom = gameField[y][x].top + cellHeigth;
			gameField[y][x].right = gameField[y][x].left + cellWidth;

			gameField[y][x].pictureCoordX = gameField[y][x].left + (gameField[y][x].right - gameField[y][x].left) / 4;
			gameField[y][x].pictureCoordY = gameField[y][x].top + (gameField[y][x].bottom - gameField[y][x].top) / 4;
		}
}

//������������� �������� ����
void InitializeGameField(HWND hwnd) {
	gameField = new GameCell*[hWndParams.fieldSize];
	for (int y = 0; y < hWndParams.fieldSize; y++)
		gameField[y] = new GameCell[hWndParams.fieldSize];

	RECT screenCoords;
	GetWindowRect(hwnd, &screenCoords);
	screenCoords.bottom -= screenCoords.top;
	screenCoords.right -= screenCoords.left;
	screenCoords.left = screenCoords.top = 0;

	GameFieldResize(screenCoords);
}

//�������� �������� ����
void DeleteGameField() {
	for (int y = 0; y < hWndParams.fieldSize; y++)
		delete gameField[y];
	delete gameField;
}

//���� �� �� ���� ������ ������
bool isEmptyCellsExists() {
	for (int y = 0; y < hWndParams.fieldSize; y++)
		for (int x = 0; x < hWndParams.fieldSize; x++)
			if (gameField[y][x].content == 0)
				return true;
	return false;
}

//��� ������ playerId ��������� ����� � ������� � (x,y) � ����������� (deltaX,deltaY) �� ������������
bool isWinLine(int y, int x, int deltaY, int deltaX, int playerId) {
	for (x, y; y < hWndParams.fieldSize && x < hWndParams.fieldSize; y += deltaY, x += deltaX)
		if (gameField[y][x].content != playerId)
			return false;
	return true;
}

//��������� ���� �� ������� ������ playerId
bool isPlayerWin(int playerId) {
	bool isWinner = false;
	//�������� ��� �����������
	for (int y = 0; y < hWndParams.fieldSize && !isWinner; y++)
		isWinner = isWinLine(y, 0, 0, 1, playerId);

	//�������� ��� ���������
	for (int x = 0; x < hWndParams.fieldSize && !isWinner; x++)
		isWinner = isWinLine(0, x, 1, 0, playerId);

	if (isWinner)
		return true;

	//������� ���������
	isWinner = isWinLine(0, 0, 1, 1, playerId);
	if (isWinner)
		return true;
	//�������� ���������
	isWinner = isWinLine(0, hWndParams.fieldSize - 1, 1, -1, playerId);

	return isWinner;
}

//�������
HBITMAP bitmapCross;
HBITMAP bitmapZero;

//����������� ��������
void DrawBitmapPicture(HDC hdc, HBITMAP picture, int xStart, int yStart) {
	HDC hdcMemory = CreateCompatibleDC(hdc); //���������� ��������� ���������� � ������

	SelectObject(hdcMemory, picture); //������ ���� ������ ������� � ����������� ���������
	/*SetMapMode(hdcMemory, GetMapMode(hdc));*/

	//��������� ���������� � ��������
	BITMAP bmPicture;
	GetObject(picture, sizeof(BITMAP), (LPVOID)&bmPicture);

	//������� ����� �� ��������� ������ � �������� ����������
	BitBlt(hdc, xStart, yStart, bmPicture.bmWidth, bmPicture.bmHeight, hdcMemory, 0, 0, SRCCOPY);

	DeleteDC(hdcMemory);
}


//������� ��� �������� ����
void PaintWindow(HWND hwnd, RECT wndSize, COLORREF color) {
	HBRUSH newBrush = CreateSolidBrush(color);

	HBRUSH oldBrush = (HBRUSH)GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND);
	SetClassLong(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)newBrush);

	DeleteObject(oldBrush);
	InvalidateRect(hwnd, NULL, TRUE); //�������������� ����

	//��������� ���� ���� � ��������� ������� ����
	hWndParams.backgroundColorRed = GetRValue(color);
	hWndParams.backgroundColorGreen = GetGValue(color);
	hWndParams.backgroundColorBlue = GetBValue(color);
}

//���������� �������
LRESULT CALLBACK MyWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		//���� ���� ������ �������, �� ��������� ���������
		case WM_CLOSE: {
			PostQuitMessage(0);
			return 0;
		} break;
		case WM_KEYDOWN: {
			//��������� ������� Escape � CTRL + Q - �����
			if (wParam == VK_ESCAPE || GetAsyncKeyState(VK_CONTROL) && wParam == 'Q') {
				PostQuitMessage(0);
				return 0;
			}
			//��������� ������� SHIFT + C - ������� �������
			if (GetAsyncKeyState(VK_SHIFT) && wParam == 'C') {
				STARTUPINFO sInfo;
				ZeroMemory(&sInfo, sizeof(STARTUPINFO)); // ������ ������(�� ����, �������� sInfo)

				PROCESS_INFORMATION pInfo;

				CreateProcess("C:\\Windows\\Notepad.exe", NULL, NULL, NULL, FALSE,
					NULL, NULL, NULL, &sInfo, &pInfo);

				return 0;
			}
			//����� ���������
			if (wParam == VK_SPACE) {
				cout << "\a";
				MessageBox(hwnd, "����� ��������� - �������� �������, ��� ����� �������� : )", "��������!", 0);
				return 0;
			}
			//��������� ������� Enter - ������ ���� �� ���������
			if (wParam == VK_RETURN) {
				//��������� ����
				int red = rand() % 256;
				int green = rand() % 256;
				int blue = rand() % 256;

				RECT wndSize; //��������� � ������������ ������� ����
				GetClientRect(hwnd, &wndSize);

				//������ ���� ���� � ������� ������� �����
				PaintWindow(hwnd, wndSize, RGB(red, green, blue));

				return 0;
			}
		} break;
		case WM_LBUTTONUP: {
			//���������, ����� �� ������ ������
			if (lastSteppedPlayerId == currentPlayerId) {
				cout << "\a";
				MessageBox(hwnd, "������ �� ��� ���!", "���!", 0);
				return 0;
			}

			//�������� ��������� ����
			if (isPlayerWin(1)) {
				//��������� ����������� ������ ��� ����, ��� ��� ����� � ����� ����
				if (currentPlayerId == lastSteppedPlayerId)
					MessageBox(hwnd, "�������� ��������!", "����� ����", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}
			if (isPlayerWin(2)) {
				if (currentPlayerId == lastSteppedPlayerId)
					MessageBox(hwnd, "������ ��������!", "����� ����", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}
			if (!isEmptyCellsExists()) {
				if (currentPlayerId == lastSteppedPlayerId)
					MessageBox(hwnd, "�����!", "����� ����", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}


			double xMouse = GET_X_LPARAM(lParam);
			double yMouse = GET_Y_LPARAM(lParam);

			HDC hdc = GetWindowDC(hwnd); //�������� �������� ���������� ����


			//���� ������, � ������� ���� ������ ������
			for (int y = 0; y < hWndParams.fieldSize; y++)
				for (int x = 0; x < hWndParams.fieldSize; x++)
					if (xMouse > gameField[y][x].left && xMouse < gameField[y][x].right
						&& yMouse > gameField[y][x].top && yMouse < gameField[y][x].bottom) {

						if (gameField[y][x].content == 0) { //������ �����
							gameField[y][x].content = currentPlayerId;
							y = hWndParams.fieldSize; break; //����� �� �����
						}
						else {
							cout << "\a";
							MessageBox(hwnd, "������ ��� ������!", "���!", 0);
							return 0;
						}
					}

			sprintf(sharedMemory.map, "%s", GamedataToBuf()); //������� ��������� ���� � ����������� ������
			SendMessage(HWND_BROADCAST, WM_PAINT, wParam, lParam); //�������� ��������� � ����������� ���� ����(����-����-����)

			//�������� ��������� ���� ��� ���
			if (isPlayerWin(1)) {
				MessageBox(hwnd, "�������� ��������!", "����� ����", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}
			if (isPlayerWin(2)) {
				MessageBox(hwnd, "������ ��������!", "����� ����", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}
			if (!isEmptyCellsExists()) {
				MessageBox(hwnd, "�����!", "����� ����", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}

			return 0;
		} break;
		case WM_PAINT: {
			//���� ��������� ���� �� ����������� ������
			int tmp;
			int bytes = sscanf(sharedMemory.map, "%i", &tmp);
			if (bytes != -1) BufToGamedata(sharedMemory.map);

			HBRUSH colorBr = CreateSolidBrush(RGB(hWndParams.backgroundColorRed, hWndParams.backgroundColorGreen, hWndParams.backgroundColorBlue)); //����� ����� ���� ����
			HPEN pen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0)); //���� ��� ��������� �������� �����
			HDC hdc = GetWindowDC(hwnd); //�������� �������� ���������� ����
			
			//�������� ������� ���� ��� ���������(������� ����� ����)
			RECT screenSize;
			GetWindowRect(hwnd, &screenSize);
			screenSize.bottom -= screenSize.top;
			screenSize.right -= screenSize.left;
			screenSize.left = screenSize.top = 0; //��������� ���������� � ������ ����

			PAINTSTRUCT ps; //���������, ���������� ������, ������ ��� �������� ����


			BeginPaint(hwnd, &ps); //�������� �������

			//������ ����� ����
			SelectObject(hdc, pen);
			for (int i = 0; i <= hWndParams.fieldSize; ++i) {
				MoveToEx(hdc, i * screenSize.right / hWndParams.fieldSize, screenSize.top, NULL);
				LineTo(hdc, i * screenSize.right / hWndParams.fieldSize, screenSize.bottom);
				MoveToEx(hdc, screenSize.left, i * screenSize.bottom / hWndParams.fieldSize, NULL);
				LineTo(hdc, screenSize.right, i * screenSize.bottom / hWndParams.fieldSize);
			}

			//������ ���������� ������
			for (int y = 0; y < hWndParams.fieldSize; y++)
				for (int x = 0; x < hWndParams.fieldSize; x++) {
					if (gameField[y][x].content == 1)
						DrawBitmapPicture(hdc, bitmapCross, gameField[y][x].pictureCoordX, gameField[y][x].pictureCoordY);
					else
						if (gameField[y][x].content == 2)
							DrawBitmapPicture(hdc, bitmapZero, gameField[y][x].pictureCoordX, gameField[y][x].pictureCoordY);
				}

			

			EndPaint(hwnd, &ps); //����������� �������

			ReleaseDC(hwnd, hdc);
			DeleteObject(pen);
			DeleteObject(colorBr);


			return 0;
		} break;
		case  WM_SIZE: {
			RECT screenSize;
			GetWindowRect(hwnd, &screenSize);
			screenSize.bottom -= screenSize.top;
			screenSize.right -= screenSize.left;
			screenSize.left = screenSize.top = 0;

			GameFieldResize(screenSize);

			return 0;
		} break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

//���������������� ����
void CreateCfgByStream() {
	ofstream oF;
	oF.open(nameOfCfgFile);
	oF << hWndParams.width << ' '
		<< hWndParams.height << ' '
		<< hWndParams.xPos << ' '
		<< hWndParams.yPos << ' '
		<< hWndParams.backgroundColorRed << ' '
		<< hWndParams.backgroundColorGreen << ' '
		<< hWndParams.backgroundColorBlue << ' '
		<< hWndParams.crossPictureFileName << ' '
		<< hWndParams.zeroPictureFileName << ' '
		<< hWndParams.fieldSize;
	oF.close();
}

bool LoadCfgByStream() {
	ifstream iF;
	iF.open(nameOfCfgFile);

	if (!iF) {
		iF.close();
		return false;
	}

	iF >> hWndParams.width
		>> hWndParams.height
		>> hWndParams.xPos
		>> hWndParams.yPos
		>> hWndParams.backgroundColorRed
		>> hWndParams.backgroundColorGreen
		>> hWndParams.backgroundColorBlue
		>> hWndParams.crossPictureFileName
		>> hWndParams.zeroPictureFileName
		>> hWndParams.fieldSize;

	iF.close();

	return true;
}

void SaveWindowParams(HWND hwnd) {
	//�������� ��������� � ������������ ������� ����
	RECT wndCoords;
	GetWindowRect(hwnd, &wndCoords);
	//���������� ���������� ����
	hWndParams.xPos = wndCoords.left;
	hWndParams.yPos = wndCoords.top;
	//���������� ������� ����
	hWndParams.height = wndCoords.bottom - wndCoords.top;
	hWndParams.width = wndCoords.right - wndCoords.left;

	CreateCfgByStream();
}

//����������
enum LoadDllCondition {
	Suc�ess,
	DllLoadFaild,
	FunctionLoadFaild
};

LoadDllCondition LoadPictures()
{
	HINSTANCE hDLL; //���������� ������������ ����������

	HBITMAP (*LoadPicture) (const char* fileName); //��������� ��������� �� ������� �� dll

	hDLL = LoadLibrary("PictureLib.dll"); //��������� ����������

	if (!hDLL)
		return LoadDllCondition::DllLoadFaild;

	//�������� ����� ������� � ����������� ��� ��������� � ����� ����������� ����
	LoadPicture = (HBITMAP (*) (const char* fileName))
		GetProcAddress(hDLL, "LoadPicture");

	if (!LoadPicture)
		return LoadDllCondition::FunctionLoadFaild;

	//�������� �������� � ���� bmp
	bitmapCross = LoadPicture(hWndParams.crossPictureFileName.c_str());
	bitmapZero = LoadPicture(hWndParams.zeroPictureFileName.c_str());

	FreeLibrary(hDLL); //�������� ������������ ���������� �� ������

	return LoadDllCondition::Suc�ess;
}



int main(int argc, char **argv) {

	//�������� ���������� �������
	playersCountSem = CreateSemaphore(NULL, 2, 2, "playersCount");
	if (WaitForSingleObject(playersCountSem, 0) == WAIT_TIMEOUT) {
		cout << "\a";
		MessageBox(NULL, "������ ���� ������� �� ��������������", "��������!", 0);
		return 0;
	}

	//������������ ������ id
	playerIdMutex = CreateMutex(NULL, FALSE, "playerId"); //��������� �������
	WaitForSingleObject(playerIdMutex, 0) != WAIT_TIMEOUT ? currentPlayerId = 1 : currentPlayerId = 2;
	string playerName;
	if (currentPlayerId == 1)
		playerName = "�������";
	else
		playerName = "�����";
	string windowName = "��������-������: �� " + playerName;


	ShowWindow(GetConsoleWindow(), SW_HIDE); //�������� ���������� ����

	
	LoadDllCondition isDllLoaded = LoadPictures();

	bool isCFGLoaded = LoadCfgByStream();

	HINSTANCE hInstance = GetModuleHandle(NULL);	//���������� �������� ������

	HBRUSH hBrush = CreateSolidBrush(RGB(hWndParams.backgroundColorRed, hWndParams.backgroundColorGreen, hWndParams.backgroundColorBlue));	//������ ����� ������ �����

	WNDCLASS winClass;	 //������ ����������, �������� ����� ��������������� ��� ���� ������ ������
						 //������� ��� ��������:
	winClass.style = CS_HREDRAW |	//����������� ������� ���� ����� ��������� ��� ������
		CS_VREDRAW |		//����������� ������� ���� ����� ��������� ��� ������
		CS_OWNDC;		//������� ����-���������� �������� ����������
	winClass.lpfnWndProc = (WNDPROC)MyWndProc;	 //���������� �������
	winClass.cbClsExtra = 0;	//���.�����, ������� ����������� ����� �� ���������� ������ ����
	winClass.cbWndExtra = 0;	//���.�����, ������� ����������� ����� �� ����������� ����
	winClass.hInstance = hInstance;		//���������� ���������� ����������
	winClass.hIcon = LoadIcon(NULL, IDI_WINLOGO);		//������
	winClass.hCursor = LoadCursor(NULL, IDC_ARROW);		//������
	winClass.hbrBackground = hBrush;	//���������� ����� ���� ������
	winClass.lpszMenuName = NULL;	//��������� �� ������, ��������������� ��� ������� ���� ������
	winClass.lpszClassName = _T("MyWindowsClass");	 //��� ������ ����

	RegisterClass(&winClass); //������������ ����� �����

	HWND hWnd = CreateWindow(_T("MyWindowsClass"),		//������ ����������� ���� � �����������: �������� ������
		_T(windowName.c_str()), WS_OVERLAPPEDWINDOW |	//�������� ����, �����-��������������� ����
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN, hWndParams.xPos, hWndParams.yPos,	//�����, ��������� � ����������� ������� ����, �������������� � ������������ ������������ ����
		hWndParams.width, hWndParams.height, NULL, NULL, hInstance, NULL);		//������ ����, ������ ����, ..., ..., ���������� ���������� �����, ���.������


	InitializeGameField(hWnd); //�������������� ������� ����

	ShowWindow(hWnd, SW_SHOW); //���������� ����

	MSG message; // ��� �������� ���������

	if (!isCFGLoaded) {
		cout << "\a";
		MessageBox(hWnd, "���������������� ���� �� ��� ��������. ��������� ����������� ���������", "��������!", 0);
	} //���������� � �������� ��������� ����������

	if (isDllLoaded == LoadDllCondition::DllLoadFaild) {
		cout << "\a";
		MessageBox(hWnd, "���������� ImageLib.dll �� ���� ���������. ������ ���������� ����� ���������.", "��������!", 0);
	} //���������� � ������������� ����������

	if (isDllLoaded == LoadDllCondition::FunctionLoadFaild) {
		cout << "\a";
		MessageBox(hWnd, "���������� ImageLib.dll �� �������� ������� LoadPicture. ������ ���������� ����� ���������.", "��������!", 0);
	} //���������� � ����������� �������
	
	if (isDllLoaded == LoadDllCondition::Suc�ess)
		//���� ��������� �������
		while (true) {
			if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {		//���� ��������� ������� ���������(���������� message) 
				if (message.message == WM_QUIT)		//���� ���������-�������� ����(�������)
					break;
				else {
					TranslateMessage(&message);		//���������� ��������� � ���������� ����
					DispatchMessage(&message);		//�������� ��������� �����������, ���������� � �������� ������ ����
				}
			}
		}

	//��������� ��������� ����
	SaveWindowParams(hWnd);

	//������ ������
	DeleteObject((HBRUSH)GetClassLongPtr(hWnd, GCLP_HBRBACKGROUND)); //������� ������� ����� ���� ����
	DestroyWindow(hWnd);	//���������� ����
	UnregisterClass(_T("MyWindowClass"), hInstance);	//�������� ����������� ������

	DeleteObject(hBrush);	 //������� �������������� �����

	//������� ����������� ��������
	DeleteObject(bitmapCross);
	DeleteObject(bitmapZero);

	//������� ������� ������� � ������� �������������
	DeleteGameField();

	ReleaseMutex(playerIdMutex);
	CloseHandle(playerIdMutex);

	ReleaseSemaphore(playersCountSem, 1, NULL);
	CloseHandle(playersCountSem);

	//��������� ������� ��������� ����������� ������
	if (currentPlayerId != lastSteppedPlayerId)
		CloseSharedMemory();

	return 0;
}