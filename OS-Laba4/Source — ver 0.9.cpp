#define _CRT_SECURE_NO_WARNINGS //для использования функции sprintf
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string>


using namespace std;

struct WindowParams {
	//границы
	int width = 320;
	int height = 240;
	//координаты
	int xPos = 100;
	int yPos = 100;
	//фон
	int backgroundColorRed = 0;
	int backgroundColorGreen = 0;
	int backgroundColorBlue = 255;
	//имена картинок
	string crossPictureFileName = "Cross.png";
	string zeroPictureFileName = "Zero.jpg";
	//высота и ширина поля(в ячейках)
	int fieldSize = 3;
} hWndParams; //параметры окна

string nameOfCfgFile = "CFGFile.txt";


struct GameCell {
	int content = 0; //содержание ячейки(0-пуста, 1-первый игрок, 2-второй игрок)
	int top, bottom, left, right; //координаты ячейки
	int pictureCoordX, pictureCoordY; //координаты картинки в ней
};

GameCell** gameField; //поле

int currentPlayerId; //id игрока этого процесса
int lastSteppedPlayerId; //id игрока, который ходил последним



//Средства взаимного исключения
HANDLE playerIdMutex; //мьютекс для выдачи игроку id
HANDLE playersCountSem; //считающий семафор для того, чтоб допускать в игру только двоих игроков


//Средство для общения процессов между собой-разделяемая память
//share - имя этой разделяемой области, 
//а INVALID_HANDLE_VALUE говорит о том, что данное отображение не "привязано" к файлу
//Отображения, созданные в разные процессах с одинаковым именем, ссылаются на один (разделяемый) участок памяти

struct SharedMemory {
	//4096 байт - типичный размер страницы памяти
	HANDLE hmap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, "sharedMemory");
	char* map = (char*)MapViewOfFile(hmap, FILE_MAP_ALL_ACCESS, 0, 0, 4096);
} sharedMemory;

void CloseSharedMemory() {
	UnmapViewOfFile(sharedMemory.map);
	CloseHandle(sharedMemory.hmap);
}

//Получение буфера для записи в разделяемую память
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

//Обновление поля в соответствии с буфером
void BufToGamedata(char* buf) {
	int  i = 0; //для прохода по буферу
	for (int y = 0; y < hWndParams.fieldSize; y++)
		for (int x = 0; x < hWndParams.fieldSize; x++) {
			gameField[y][x].content = buf[i] - '0';
			i++;
		}

	lastSteppedPlayerId = buf[hWndParams.fieldSize * hWndParams.fieldSize] - '0';
}



//Перерасчёт координат клеток
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

//Инициализация игрового поля
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

//Удаление игрового поля
void DeleteGameField() {
	for (int y = 0; y < hWndParams.fieldSize; y++)
		delete gameField[y];
	delete gameField;
}

//Есть ли на поле пустые клетки
bool isEmptyCellsExists() {
	for (int y = 0; y < hWndParams.fieldSize; y++)
		for (int x = 0; x < hWndParams.fieldSize; x++)
			if (gameField[y][x].content == 0)
				return true;
	return false;
}

//Для игрока playerId проверяет линию с началом в (x,y) в направлении (deltaX,deltaY) на выигрышность
bool isWinLine(int y, int x, int deltaY, int deltaX, int playerId) {
	for (x, y; y < hWndParams.fieldSize && x < hWndParams.fieldSize; y += deltaY, x += deltaX)
		if (gameField[y][x].content != playerId)
			return false;
	return true;
}

//Проверяет поле на выигрыш игрока playerId
bool isPlayerWin(int playerId) {
	bool isWinner = false;
	//переберём все горизонтали
	for (int y = 0; y < hWndParams.fieldSize && !isWinner; y++)
		isWinner = isWinLine(y, 0, 0, 1, playerId);

	//переберём все вертикали
	for (int x = 0; x < hWndParams.fieldSize && !isWinner; x++)
		isWinner = isWinLine(0, x, 1, 0, playerId);

	if (isWinner)
		return true;

	//главная диагональ
	isWinner = isWinLine(0, 0, 1, 1, playerId);
	if (isWinner)
		return true;
	//побочная диагональ
	isWinner = isWinLine(0, hWndParams.fieldSize - 1, 1, -1, playerId);

	return isWinner;
}

//Рисунки
HBITMAP bitmapCross;
HBITMAP bitmapZero;

//Отображение рисунков
void DrawBitmapPicture(HDC hdc, HBITMAP picture, int xStart, int yStart) {
	HDC hdcMemory = CreateCompatibleDC(hdc); //дескриптор контекста устройства в памяти

	SelectObject(hdcMemory, picture); //делаем этот битмап текущим в совместимом контексте
	/*SetMapMode(hdcMemory, GetMapMode(hdc));*/

	//извлекаем информацию о картинке
	BITMAP bmPicture;
	GetObject(picture, sizeof(BITMAP), (LPVOID)&bmPicture);

	//Перенос битов из контекста памяти в контекст устройства
	BitBlt(hdc, xStart, yStart, bmPicture.bmWidth, bmPicture.bmHeight, hdcMemory, 0, 0, SRCCOPY);

	DeleteDC(hdcMemory);
}


//функция для закраски окна
void PaintWindow(HWND hwnd, RECT wndSize, COLORREF color) {
	HBRUSH newBrush = CreateSolidBrush(color);

	HBRUSH oldBrush = (HBRUSH)GetClassLongPtr(hwnd, GCLP_HBRBACKGROUND);
	SetClassLong(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)newBrush);

	DeleteObject(oldBrush);
	InvalidateRect(hwnd, NULL, TRUE); //перерисовываем окно

	//Сохраняем цвет фона в структуру свойств окна
	hWndParams.backgroundColorRed = GetRValue(color);
	hWndParams.backgroundColorGreen = GetGValue(color);
	hWndParams.backgroundColorBlue = GetBValue(color);
}

//Обработчик событий
LRESULT CALLBACK MyWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		//если само окошко закрыто, то выключаем программу
		case WM_CLOSE: {
			PostQuitMessage(0);
			return 0;
		} break;
		case WM_KEYDOWN: {
			//обработка нажатия Escape и CTRL + Q - выход
			if (wParam == VK_ESCAPE || GetAsyncKeyState(VK_CONTROL) && wParam == 'Q') {
				PostQuitMessage(0);
				return 0;
			}
			//обработка нажатия SHIFT + C - открыть блокнот
			if (GetAsyncKeyState(VK_SHIFT) && wParam == 'C') {
				STARTUPINFO sInfo;
				ZeroMemory(&sInfo, sizeof(STARTUPINFO)); // чистим память(по сути, обнуляем sInfo)

				PROCESS_INFORMATION pInfo;

				CreateProcess("C:\\Windows\\Notepad.exe", NULL, NULL, NULL, FALSE,
					NULL, NULL, NULL, &sInfo, &pInfo);

				return 0;
			}
			//Автор программы
			if (wParam == VK_SPACE) {
				cout << "\a";
				MessageBox(hwnd, "Автор программы - Барсамов Евгений, все права защищены : )", "Внимание!", 0);
				return 0;
			}
			//Обработка нажатия Enter - меняем цвет на случайный
			if (wParam == VK_RETURN) {
				//рандомный цвет
				int red = rand() % 256;
				int green = rand() % 256;
				int blue = rand() % 256;

				RECT wndSize; //структура с координатами области окна
				GetClientRect(hwnd, &wndSize);

				//Меняем цвет фона с помощью подмены кисти
				PaintWindow(hwnd, wndSize, RGB(red, green, blue));

				return 0;
			}
		} break;
		case WM_LBUTTONUP: {
			//Проверяем, можно ли игроку ходить
			if (lastSteppedPlayerId == currentPlayerId) {
				cout << "\a";
				MessageBox(hwnd, "Сейчас не ваш ход!", "Упс!", 0);
				return 0;
			}

			//Проверим состояние игры
			if (isPlayerWin(1)) {
				//сообщения выскакивает только для окна, чей ход привёл к концу игры
				if (currentPlayerId == lastSteppedPlayerId)
					MessageBox(hwnd, "Крестики победили!", "Конец игры", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}
			if (isPlayerWin(2)) {
				if (currentPlayerId == lastSteppedPlayerId)
					MessageBox(hwnd, "Нолики победили!", "Конец игры", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}
			if (!isEmptyCellsExists()) {
				if (currentPlayerId == lastSteppedPlayerId)
					MessageBox(hwnd, "Ничья!", "Конец игры", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}


			double xMouse = GET_X_LPARAM(lParam);
			double yMouse = GET_Y_LPARAM(lParam);

			HDC hdc = GetWindowDC(hwnd); //получаем контекст устройства окна


			//ищем клетку, в которой была нажата кнопка
			for (int y = 0; y < hWndParams.fieldSize; y++)
				for (int x = 0; x < hWndParams.fieldSize; x++)
					if (xMouse > gameField[y][x].left && xMouse < gameField[y][x].right
						&& yMouse > gameField[y][x].top && yMouse < gameField[y][x].bottom) {

						if (gameField[y][x].content == 0) { //клетка пуста
							gameField[y][x].content = currentPlayerId;
							y = hWndParams.fieldSize; break; //выход из цикла
						}
						else {
							cout << "\a";
							MessageBox(hwnd, "Клетка уже занята!", "Упс!", 0);
							return 0;
						}
					}

			sprintf(sharedMemory.map, "%s", GamedataToBuf()); //передаём состояние поля в разделяемую память
			SendMessage(HWND_BROADCAST, WM_PAINT, wParam, lParam); //посылаем сообщение о перерисовке всех окон(всем-всем-всем)

			//Проверим состояние игры ещё раз
			if (isPlayerWin(1)) {
				MessageBox(hwnd, "Крестики победили!", "Конец игры", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}
			if (isPlayerWin(2)) {
				MessageBox(hwnd, "Нолики победили!", "Конец игры", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}
			if (!isEmptyCellsExists()) {
				MessageBox(hwnd, "Ничья!", "Конец игры", 0);
				PostMessage(hwnd, WM_CLOSE, wParam, lParam);
				return 0;
			}

			return 0;
		} break;
		case WM_PAINT: {
			//берём состояние поля из разделяемой памяти
			int tmp;
			int bytes = sscanf(sharedMemory.map, "%i", &tmp);
			if (bytes != -1) BufToGamedata(sharedMemory.map);

			HBRUSH colorBr = CreateSolidBrush(RGB(hWndParams.backgroundColorRed, hWndParams.backgroundColorGreen, hWndParams.backgroundColorBlue)); //кисть цвета фона окна
			HPEN pen = CreatePen(PS_SOLID, 3, RGB(255, 0, 0)); //перо для рисования красного цвета
			HDC hdc = GetWindowDC(hwnd); //получаем контекст устройства окна
			
			//Получаем границы окна для рисования(размеры всего окна)
			RECT screenSize;
			GetWindowRect(hwnd, &screenSize);
			screenSize.bottom -= screenSize.top;
			screenSize.right -= screenSize.left;
			screenSize.left = screenSize.top = 0; //рисование начинается с начала окна

			PAINTSTRUCT ps; //структура, содержащая данные, нужные для закраски окна


			BeginPaint(hwnd, &ps); //начинаем красить

			//рисуем сетку поля
			SelectObject(hdc, pen);
			for (int i = 0; i <= hWndParams.fieldSize; ++i) {
				MoveToEx(hdc, i * screenSize.right / hWndParams.fieldSize, screenSize.top, NULL);
				LineTo(hdc, i * screenSize.right / hWndParams.fieldSize, screenSize.bottom);
				MoveToEx(hdc, screenSize.left, i * screenSize.bottom / hWndParams.fieldSize, NULL);
				LineTo(hdc, screenSize.right, i * screenSize.bottom / hWndParams.fieldSize);
			}

			//рисуем содержимое клеток
			for (int y = 0; y < hWndParams.fieldSize; y++)
				for (int x = 0; x < hWndParams.fieldSize; x++) {
					if (gameField[y][x].content == 1)
						DrawBitmapPicture(hdc, bitmapCross, gameField[y][x].pictureCoordX, gameField[y][x].pictureCoordY);
					else
						if (gameField[y][x].content == 2)
							DrawBitmapPicture(hdc, bitmapZero, gameField[y][x].pictureCoordX, gameField[y][x].pictureCoordY);
				}

			

			EndPaint(hwnd, &ps); //заканчиваем красить

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

//Конфигурационный файл
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
	//Получаем структуру с координатами области окна
	RECT wndCoords;
	GetWindowRect(hwnd, &wndCoords);
	//Записываем координаты окна
	hWndParams.xPos = wndCoords.left;
	hWndParams.yPos = wndCoords.top;
	//Записываем размеры окна
	hWndParams.height = wndCoords.bottom - wndCoords.top;
	hWndParams.width = wndCoords.right - wndCoords.left;

	CreateCfgByStream();
}

//Библиотека
enum LoadDllCondition {
	Sucсess,
	DllLoadFaild,
	FunctionLoadFaild
};

LoadDllCondition LoadPictures()
{
	HINSTANCE hDLL; //дескриптор динамической библиотеки

	HBITMAP (*LoadPicture) (const char* fileName); //объявляем указатель на функцию из dll

	hDLL = LoadLibrary("PictureLib.dll"); //загружаем библиотеку

	if (!hDLL)
		return LoadDllCondition::DllLoadFaild;

	//получаем адрес функции и присваиваем его указателю с явным приведением типа
	LoadPicture = (HBITMAP (*) (const char* fileName))
		GetProcAddress(hDLL, "LoadPicture");

	if (!LoadPicture)
		return LoadDllCondition::FunctionLoadFaild;

	//получаем картинки в виде bmp
	bitmapCross = LoadPicture(hWndParams.crossPictureFileName.c_str());
	bitmapZero = LoadPicture(hWndParams.zeroPictureFileName.c_str());

	FreeLibrary(hDLL); //выгрузка динамической библиотеки из памяти

	return LoadDllCondition::Sucсess;
}



int main(int argc, char **argv) {

	//Проверка количества игроков
	playersCountSem = CreateSemaphore(NULL, 2, 2, "playersCount");
	if (WaitForSingleObject(playersCountSem, 0) == WAIT_TIMEOUT) {
		cout << "\a";
		MessageBox(NULL, "Больше двух игроков не поддерживается", "Внимание!", 0);
		return 0;
	}

	//Присваивание игроку id
	playerIdMutex = CreateMutex(NULL, FALSE, "playerId"); //Свободный мьютекс
	WaitForSingleObject(playerIdMutex, 0) != WAIT_TIMEOUT ? currentPlayerId = 1 : currentPlayerId = 2;
	string playerName;
	if (currentPlayerId == 1)
		playerName = "крестик";
	else
		playerName = "нолик";
	string windowName = "Крестики-Нолики: вы " + playerName;


	ShowWindow(GetConsoleWindow(), SW_HIDE); //скрываем консольное окно

	
	LoadDllCondition isDllLoaded = LoadPictures();

	bool isCFGLoaded = LoadCfgByStream();

	HINSTANCE hInstance = GetModuleHandle(NULL);	//дескриптор текущего модуля

	HBRUSH hBrush = CreateSolidBrush(RGB(hWndParams.backgroundColorRed, hWndParams.backgroundColorGreen, hWndParams.backgroundColorBlue));	//создаём кисть синего цвета

	WNDCLASS winClass;	 //создаём дескриптор, которому будут соответствовать все окна нового класса
						 //Зададим его свойства:
	winClass.style = CS_HREDRAW |	//перерисовка области окна после изменения его ширины
		CS_VREDRAW |		//перерисовка области окна после изменения его высоты
		CS_OWNDC;		//каждому окну-уникальный контекст устройства
	winClass.lpfnWndProc = (WNDPROC)MyWndProc;	 //обработчик событий
	winClass.cbClsExtra = 0;	//доп.байты, которые размещаются вслед за структурой класса окна
	winClass.cbWndExtra = 0;	//доп.байты, которые размещаются вслед за экземпляром окна
	winClass.hInstance = hInstance;		//дескриптор экземпляра приложения
	winClass.hIcon = LoadIcon(NULL, IDI_WINLOGO);		//иконка
	winClass.hCursor = LoadCursor(NULL, IDC_ARROW);		//курсор
	winClass.hbrBackground = hBrush;	//дескриптор кисти фона класса
	winClass.lpszMenuName = NULL;	//указатель на строку, устанавливающую имя ресурса меню класса
	winClass.lpszClassName = _T("MyWindowsClass");	 //имя класса окна

	RegisterClass(&winClass); //регистрируем новый класс

	HWND hWnd = CreateWindow(_T("MyWindowsClass"),		//Создаём дескприптор окна с параметрами: название класса
		_T(windowName.c_str()), WS_OVERLAPPEDWINDOW |	//название окна, стиль-перекрывающееся окно
		WS_CLIPSIBLINGS | WS_CLIPCHILDREN, hWndParams.xPos, hWndParams.yPos,	//стили, связанные с прорисовкой дочених окон, горизонтальное и вертикальное расположение окна
		hWndParams.width, hWndParams.height, NULL, NULL, hInstance, NULL);		//ширина окна, высота окна, ..., ..., дескриптор экземпляра проги, доп.данные


	InitializeGameField(hWnd); //инициализируем игровое поле

	ShowWindow(hWnd, SW_SHOW); //отображаем окно

	MSG message; // для хранения сообщений

	if (!isCFGLoaded) {
		cout << "\a";
		MessageBox(hWnd, "Конфигурационный файл не был загружен. Применены стандартные настройки", "Внимание!", 0);
	} //оповещение о загрузке дефолтных параметров

	if (isDllLoaded == LoadDllCondition::DllLoadFaild) {
		cout << "\a";
		MessageBox(hWnd, "Библиотека ImageLib.dll не была загружена. Работа приложения будет завершена.", "Внимание!", 0);
	} //оповещение о незагруженной библиотеке

	if (isDllLoaded == LoadDllCondition::FunctionLoadFaild) {
		cout << "\a";
		MessageBox(hWnd, "Библиотека ImageLib.dll не содержит функции LoadPicture. Работа приложения будет завершена.", "Внимание!", 0);
	} //оповещение о ненайденной функции
	
	if (isDllLoaded == LoadDllCondition::Sucсess)
		//Цикл обработки событий
		while (true) {
			if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE)) {		//если сообщение успешно сохранено(переменная message) 
				if (message.message == WM_QUIT)		//если сообщение-закрытие окна(крестик)
					break;
				else {
					TranslateMessage(&message);		//Трансляция сообщения в символьные коды
					DispatchMessage(&message);		//Передача сообщения обработчику, указанному в описании класса окна
				}
			}
		}

	//Сохраняем параметры окна
	SaveWindowParams(hWnd);

	//Чистим память
	DeleteObject((HBRUSH)GetClassLongPtr(hWnd, GCLP_HBRBACKGROUND)); //удаляем текущую кисть фона окна
	DestroyWindow(hWnd);	//уничтожаем окно
	UnregisterClass(_T("MyWindowClass"), hInstance);	//отзываем регистрацию класса

	DeleteObject(hBrush);	 //удаляем первоначальную кисть

	//удаляем загруженные картинки
	DeleteObject(bitmapCross);
	DeleteObject(bitmapZero);

	//Удаляем игровые объекты и объекты синхронизации
	DeleteGameField();

	ReleaseMutex(playerIdMutex);
	CloseHandle(playerIdMutex);

	ReleaseSemaphore(playersCountSem, 1, NULL);
	CloseHandle(playersCountSem);

	//Последний процесс закрывает разделяемую память
	if (currentPlayerId != lastSteppedPlayerId)
		CloseSharedMemory();

	return 0;
}