/***************************************************************************\
* Module Name: bitmap.c
\***************************************************************************/

#define INCL_BITMAPFILEFORMAT
#define INCL_DOSMEMMGR
#define INCL_GPIBITMAPS

#include <os2.h>
#include <string.h>

typedef struct {
	ULONG cbFix;
	USHORT cx;
	USHORT cy;
	USHORT cPlanes;
	USHORT cBitCount;
	RGB argbColor[256];
} COLORMAPINFO;

char far * far pascal _loadds deskpicextension(HMODULE);
ULONG far pascal _loadds deskpicgetsize(HFILE);
BOOL far pascal _loadds deskpicread(HFILE, SEL, BITMAPINFO far *);
void far pascal _loadds deskpicinfo(char far *, HWND);

typedef struct {
	ULONG biSize;
	ULONG biWidth;
	ULONG biHeight;
	USHORT biPlanes;
	USHORT biBitCount;
	ULONG biCompression;
	ULONG biSizeImage;
	ULONG biXPelsPerMeter;
	ULONG biYPelsPerMeter;
	ULONG biClrUsed;
	ULONG biClrImportant;
} NEWBITMAPINFOHEADER;

typedef union {
	struct {
		unsigned int b1:8;
		unsigned int b2:8;
	} b;
	struct {
		unsigned int b1l:4;
		unsigned int b1h:4;
		unsigned int b2l:4;
		unsigned int b2h:4;
	} n;
} OPCODE;

static BITMAPFILEHEADER near bfh;
static NEWBITMAPINFOHEADER near nbfh;
static ULONG near totalblocksize;
static ULONG near rowlen;

char far * far pascal _loadds deskpicextension(thismodule)
HMODULE thismodule;

{
	thismodule;
	return ".BMP";
}

ULONG far pascal _loadds deskpicgetsize(hf)
HFILE hf;

{
	USHORT bytesread;
	long l;

	if (DosRead(hf,&bfh,sizeof(bfh),&bytesread) ||
		bytesread != sizeof(bfh) || bfh.usType != BFT_BMAP) return 0L;
	if (bfh.bmp.cbFix == sizeof(BITMAPINFOHEADER)) {
		rowlen = ((ULONG)bfh.bmp.cx*bfh.bmp.cBitCount+31>>3)&0xfffffffc;
		totalblocksize = rowlen*bfh.bmp.cy*bfh.bmp.cPlanes;
	}
	else {
		if (DosChgFilePtr(hf,(long)(sizeof(BITMAPFILEHEADER)-
			sizeof(BITMAPINFOHEADER)),FILE_BEGIN,&l)) return 0L;
		if (DosRead(hf,&nbfh,sizeof(nbfh),&bytesread) ||
			bytesread != sizeof(nbfh)) return 0L;
		if (nbfh.biCompression) nbfh.biBitCount = 8/(USHORT)nbfh.biCompression;
		rowlen = (nbfh.biWidth*nbfh.biBitCount+31>>3)&0xfffffffc;
		totalblocksize = rowlen*nbfh.biHeight*nbfh.biPlanes;
	}
	return totalblocksize;
}

BOOL far pascal _loadds deskpicread(hf,sel,cmi)
HFILE hf;
SEL sel;
BITMAPINFO far *cmi;

{
	USHORT hshift,bytesread,i,j;
	ULONG x,y;
	BYTE huge *bits;
	OPCODE b,c;

	if (bfh.bmp.cbFix == sizeof(BITMAPINFOHEADER)) {
		*((PBITMAPINFOHEADER)cmi) = bfh.bmp;
		nbfh.biCompression = 0;
	}
	else {
		cmi->cx = (USHORT)nbfh.biWidth;
		cmi->cy = (USHORT)nbfh.biHeight;
		cmi->cPlanes = nbfh.biPlanes;
		cmi->cBitCount = nbfh.biBitCount;
	}
	if (cmi->cBitCount <= 8) {
		DosChgFilePtr(hf,(long)(bfh.bmp.cbFix+sizeof(BITMAPFILEHEADER)-
			sizeof(BITMAPINFOHEADER)),FILE_BEGIN,&x);
		i = 1 << cmi->cBitCount;
		if (bfh.bmp.cbFix != sizeof(BITMAPINFOHEADER)) {
			if (nbfh.biClrUsed) i = (USHORT)nbfh.biClrUsed;
			for (j = 0; j < i; j++) {
				DosRead(hf,&cmi->argbColor[j],sizeof(RGB),&bytesread);
				DosRead(hf,&hshift,1,&bytesread);
			}
		}
		else DosRead(hf,cmi->argbColor,i*3,&bytesread);
	}
	DosChgFilePtr(hf,bfh.offBits,FILE_BEGIN,&x);
	switch (nbfh.biCompression) {
	case 0L:
		DosGetHugeShift(&hshift);
		for (i = 0; i < (USHORT)(totalblocksize>>16); i++) {
			DosRead(hf,MAKEP(sel+(i<<hshift),0),0x8000,&bytesread);
			DosRead(hf,MAKEP(sel+(i<<hshift),0x8000),0x8000,&bytesread);
		}
		if (totalblocksize&0xffff)
			DosRead(hf,MAKEP(sel+(i<<hshift),0),(USHORT)totalblocksize,&bytesread);
		break;
	case 1L:
	case 2L:
		bits = MAKEP(sel,0);
		for (x = 0; x < totalblocksize; x++) bits[x] = 0;
		x = y = 0;
		do {
			DosRead(hf,&b,sizeof(b),&bytesread);
			if (b.b.b1) for (i = 0; i < b.b.b1; i++) {
				if (x >= nbfh.biWidth) {
					x = 0;
					bits = (BYTE huge *)MAKEP(sel,0)+(++y*rowlen);
					if (y >= nbfh.biHeight) goto abort;
				}
				if (nbfh.biCompression == 1L) *bits++ = (BYTE)b.b.b2;
				else {
					if (x&1) *bits++ |= i&1 ? b.n.b2l : b.n.b2h;
					else *bits = i&1 ? (BYTE)(b.n.b2l<<4) : (BYTE)(b.n.b2h<<4);
				}
				x++;
			}
			else switch (b.b.b2) {
			case 0:
				x = 0;
				bits = (BYTE huge *)MAKEP(sel,0)+(++y*rowlen);
				break;
			case 1:
				break;
			case 2:
				DosRead(hf,&c,sizeof(c),&bytesread);
				x += c.b.b1;
				while (x >= nbfh.biWidth) {
					x -= nbfh.biWidth;
					y++;
				}
				y += c.b.b2;
				if (nbfh.biCompression == 1L) bits =
					(BYTE huge *)MAKEP(sel,0)+(y*rowlen+x);
				else bits = (BYTE huge *)MAKEP(sel,0)+(y*rowlen+(x>>1));
				break;
			default:
				for (i = 0; i < b.b.b2; i++) {
					if (x >= nbfh.biWidth) {
						x = 0;
						bits = (BYTE huge *)MAKEP(sel,0)+(++y*rowlen);
						if (y >= nbfh.biHeight) break;
					}
					if (nbfh.biCompression == 1L) {
						if (i&1) c.b.b1 = c.b.b2;
						else DosRead(hf,&c,sizeof(c),&bytesread);
						*bits++ = (BYTE)c.b.b1;
					}
					else {
						if (!(i&1)) {
							if (i&2) c.b.b1 = c.b.b2;
							else DosRead(hf,&c,sizeof(c),&bytesread);
						}
						if (x&1) *bits++ |= i&1 ? c.n.b1l : c.n.b1h;
						else *bits = i&1 ? (BYTE)(c.n.b1l<<4) : (BYTE)(c.n.b1h<<4);
					}
					x++;
				}
			}
		abort: ;
		} while (y < nbfh.biHeight && (b.b.b1 || b.b.b2 != 1));
		break;
	default:
		return TRUE;
	}
	return FALSE;
}

void far pascal _loadds deskpicinfo(filepath,hwnd)
char far *filepath;
HWND hwnd;
{
	char temp[255];

	strcpy(temp,"BITMAP.DPR - Picture reader for OS/2 and "
		"Windows 3.0 bitmaps.\n\nby John Ridges\n\nCurrent Bitmap:  ");
	strcat(temp,filepath);
	WinMessageBox(HWND_DESKTOP,hwnd,temp,"DESKPIC",0,MB_OK|MB_NOICON);
}
