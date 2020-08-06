#ifndef __BMPMAGE_H__
#define __BMPMAGE_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string>

#pragma pack(1)			//memory align
typedef uint32_t DWORD;
typedef unsigned short WORD;
typedef int32_t LONG;
typedef struct tagBITMAPFILEHEADER {
	WORD  bfType;
	DWORD bfSize;
	WORD  bfReserved1;
	WORD  bfReserved2;
	DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
	DWORD biSize;
	LONG  biWidth;
	LONG  biHeight;
	WORD  biPlanes;
	WORD  biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG  biXPelsPerMeter;
	LONG  biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

// a simple BMP image reader/writer for testing code
struct BMPImage {
	int width;
	int height;
	unsigned char *bitmapImage;	// size is rgb*width*height

	BMPImage(const char* inName) { readImage(inName); }

	void writeImage(const char* outName) {
		if(bitmapImage==NULL) {
			printf("no data to write\n");
		}
		_write_bmp(outName, width, height, bitmapImage);
		memset( bitmapImage, 0,	width*height*3);
	}

	void readImage(const char* inName) {
		BITMAPINFOHEADER bitmapInfoHeader;
		BITMAPFILEHEADER bitmapFileHeader; 
		unsigned char tmpC;

		FILE *fp = fopen(inName, "rb");
		if (fp == NULL) {
			printf("invalid input file\n");
			return;
		}

		fread(&bitmapFileHeader, sizeof(BITMAPFILEHEADER), 1, fp);
		fread(&bitmapInfoHeader, sizeof(BITMAPINFOHEADER), 1, fp);

		width = bitmapInfoHeader.biWidth;
		height = bitmapInfoHeader.biHeight;

		fseek(fp, bitmapFileHeader.bfOffBits, SEEK_SET);
		bitmapImage = (unsigned char*)malloc(bitmapInfoHeader.biSizeImage);
		fread(bitmapImage, bitmapInfoHeader.biSizeImage, 1, fp);

		//swap the r and b values to get RGB
		for (int i=0; i<bitmapInfoHeader.biSizeImage; i+=3) {
			tmpC = bitmapImage[i];
			bitmapImage[i] = bitmapImage[i+2];
			bitmapImage[i+2] = tmpC;
		}

		fclose(fp);
	}

private:
	void _write_bmp(const char *filename, int w, int h, unsigned char *demo) {
		int viewx = w, viewy = h;
		BITMAPFILEHEADER head;
		BITMAPINFOHEADER info;
		unsigned char tmpC;

		FILE *fp = fopen(filename, "wb");

		if(fp == NULL) {
			printf("Can't Create New File\n");
			return;
		}
		head.bfType = 0x4d42;	//bmp type image
		head.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + viewx*viewy*3;
		head.bfReserved1 = 0;
		head.bfReserved2 = 0;
		head.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
		info.biSize = sizeof(BITMAPINFOHEADER);
		info.biWidth = viewx;
		info.biHeight = viewy;
		info.biPlanes = 1;
		info.biBitCount = 24;
		info.biCompression = 0;
		info.biSizeImage = 0;
		info.biXPelsPerMeter = 0;
		info.biYPelsPerMeter = 0;
		info.biClrUsed = 0;
		info.biClrImportant = 0;
		for(int i = 0; i < viewx*viewy*3; i += 3) {// swap R and B 
			tmpC = demo[i+2];
			demo[i+2] = demo[i];
			demo[i] = tmpC;
		}

		fwrite(&head, sizeof(BITMAPFILEHEADER), 1, fp);
		fwrite(&info, sizeof(BITMAPINFOHEADER), 1, fp);
		fwrite(demo, 1, viewx*viewy*3, fp);
		fclose(fp);
	}
};

#endif//__BMPMAGE_H__