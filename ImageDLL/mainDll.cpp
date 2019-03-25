#include <windows.h>
#include <string>
#include "FreeImage.h"

using namespace std;

//часто имена наших функций могут совпадать с именами функций C. Для этого мы и пишем блок ниже(помечено "это")
#ifdef __cplusplus //это
extern "C" { //это
#endif //это

	 __declspec(dllexport) HBITMAP LoadPicture(const char* fileName)
	{
		string strFileName = (string)fileName;
		string fileExtension = strFileName.substr(strFileName.rfind('.') + 1); //получаем расширение файла

		FIBITMAP* dib; //структура BMP-файла

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

#ifdef __cplusplus //это
}
#endif //это