#include <windows.h>
#include <string>
#include "FreeImage.h"

using namespace std;

//����� ����� ����� ������� ����� ��������� � ������� ������� C. ��� ����� �� � ����� ���� ����(�������� "���")
#ifdef __cplusplus //���
extern "C" { //���
#endif //���

	 __declspec(dllexport) HBITMAP LoadPicture(const char* fileName)
	{
		string strFileName = (string)fileName;
		string fileExtension = strFileName.substr(strFileName.rfind('.') + 1); //�������� ���������� �����

		FIBITMAP* dib; //��������� BMP-�����

		if (fileExtension == "png")
			dib = FreeImage_Load(FIF_PNG, fileName, PNG_DEFAULT);
		else if (fileExtension == "jpg")
			dib = FreeImage_Load(FIF_JPEG, fileName, JPEG_DEFAULT);
		else
			return NULL;

		FIBITMAP* dib32 = FreeImage_ConvertTo32Bits(dib);

		int width = FreeImage_GetWidth(dib);
		int height = FreeImage_GetHeight(dib);

		HBITMAP hBitmap = CreateBitmap(width, height, 1, 32, FreeImage_GetBits(dib32));

		FreeImage_Unload(dib);
		FreeImage_Unload(dib32);

		return hBitmap;
	}

#ifdef __cplusplus //���
}
#endif //���