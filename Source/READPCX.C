/***************************************************************************\
* Module Name: readpcx.c
\***************************************************************************/

#define COUNTMASK 0xC0
#define REPEATMASK 0x3F

#define INCL_GPIBITMAPS

#include <os2.h>

#define BIT1 0x80
#define BIT2 0x40
#define BIT3 0x20
#define BIT4 0x10
#define BIT5 0x08
#define BIT6 0x04
#define BIT7 0x02
#define BIT8 0x01

#define R1 0x10
#define G1 0x20
#define B1 0x40
#define I1 0x80

#define R2 0x01
#define G2 0x02
#define B2 0x04
#define I2 0x08

typedef struct {
	ULONG cbFix;
	USHORT cx;
	USHORT cy;
	USHORT cPlanes;
	USHORT cBitCount;
	RGB argbColor[256];
} COLORMAPINFO;

typedef struct {
	USHORT xLeft;
	USHORT yBottom;
	USHORT xRight;
	USHORT yTop;
} PCXRECT;

typedef struct {
	BYTE rvalue;
	BYTE gvalue;
	BYTE bvalue;
} PCXRGBCOLOR;

typedef struct {
	BYTE manufacturer;
	BYTE version;
	BYTE encoding;
	BYTE bitsperpixel;
	PCXRECT window;
	USHORT hres;
	USHORT vres;
	PCXRGBCOLOR colormap[16];
	BYTE reserved;
	BYTE nplanes;
	USHORT bytesperline;
	USHORT paletteinfo;
	BYTE filler[58];
} PCXFILEHEADER;

char far * far pascal pcxextension(HMODULE);
ULONG far pascal getpcxsize(HFILE);
BOOL far pascal readpcx(HFILE, SEL, COLORMAPINFO far *);
static void near unpackchunkyscanline(int);
static void near unpackplanarscanline(int, SHORT);
static BOOL near readcolors(COLORMAPINFO *);

static long near bmplinelength;
static USHORT near bytesperline;
static HFILE near pcxfile;
static BYTE huge * near bp;
static BYTE * near tmpbuf;

char far * far pascal pcxextension(thismodule)
HMODULE thismodule;

{
	thismodule;
	return ".PCX";
}

ULONG far pascal getpcxsize(hf)
HFILE hf;

{
	USHORT count;
	PCXFILEHEADER pfh;

	if (DosRead(hf,&pfh,sizeof(PCXFILEHEADER),&count) ||
		count != sizeof(PCXFILEHEADER)) return 0;
	if ((count = pfh.nplanes) == 3) count++;
	if (count != 1 && (count != 4 || pfh.bitsperpixel != 1)) return 0;
	bytesperline = pfh.bytesperline;
	bmplinelength = ((long)pfh.bytesperline*count+3)&0xfffffffc;
	return bmplinelength*(pfh.window.yTop-pfh.window.yBottom+1);
}

BOOL far pascal readpcx(hf,sel,cmi)
HFILE hf;
SEL sel;
COLORMAPINFO far *cmi;

{
	int currline,i;
	ULONG ultmp;
	USHORT ustmp;
	BOOL rv;
	SEL selt;
	PCXFILEHEADER pfh;
	static RGB clut[16] = {
		{0,0,0},
		{0x80,0,0},
		{0,0x80,0},
		{0x80,0x80,0},
		{0,0,0x80},
		{0x80,0,0x80},
		{0,0x80,0x80},
		{0x80,0x80,0x80},
		{0,0,0},
		{0xFF,0,0},
		{0,0xFF,0},
		{0xFF,0xFF,0},
		{0,0,0xFF},
		{0xFF,0,0xFF},
		{0,0xFF,0xFF},
		{0xFF,0xFF,0xFF}
	};

	pcxfile = hf;
	bp = MAKEP(sel,0);
	if (DosChgFilePtr(pcxfile,0L,FILE_BEGIN,&ultmp)) return TRUE;
	if (DosRead(pcxfile,&pfh,sizeof(PCXFILEHEADER),&ustmp) ||
		ustmp != sizeof(PCXFILEHEADER)) return TRUE;
	currline = pfh.window.yTop-pfh.window.yBottom;
	cmi->cx = pfh.window.xRight-pfh.window.xLeft+1;
	cmi->cy = currline+1;
	cmi->cPlanes = 1;
	if (pfh.nplanes > 1) {
		cmi->cBitCount = 4;
		DosAllocSeg(bytesperline*4,&selt,SEG_NONSHARED);
		tmpbuf = MAKEP(selt,0);
		do {
			unpackplanarscanline(currline,pfh.nplanes);
		} while (currline--);
		DosFreeSeg(selt);
	}
	else {
		cmi->cBitCount = pfh.bitsperpixel;
		do {
			unpackchunkyscanline(currline);
		} while (currline--);
	}

	if (pfh.version == 5) rv = readcolors(cmi);
	if (pfh.version == 2 || (pfh.version == 5 && rv))
		for (i = 0; i < 16; i++) {
	  	cmi->argbColor[i].bBlue = pfh.colormap[i].bvalue;
	  	cmi->argbColor[i].bGreen = pfh.colormap[i].gvalue;
	  	cmi->argbColor[i].bRed = pfh.colormap[i].rvalue;
	}
	else if (pfh.version != 5)
		for (i = 0; i < 16; i++) cmi->argbColor[i] = clut[i];
	return FALSE;
}

static void near unpackchunkyscanline(currline)
int currline;

{
	int repeatcount;
	long bpoffset;
	USHORT ustmp,bytecount;
	BYTE cb;

	bytecount = 0;
	bpoffset = currline*bmplinelength;

	while (bytecount < bytesperline) {
		DosRead(pcxfile,&cb,1,&ustmp);
		if ((cb&COUNTMASK) == COUNTMASK) {
			repeatcount = (int)(cb&REPEATMASK);
			DosRead(pcxfile,&cb,1,&ustmp);
			while (repeatcount--) {
				bp[bpoffset++] = cb;
				bytecount++;
			}
		}
		else {
			bp[bpoffset++] = cb;
			bytecount++;
		}
	}
}

static void near unpackplanarscanline(currline,numplanes)
int currline;
SHORT numplanes;

{

	int bytecount,repeatcount,bytestoget,j;
	long bpoffset;
	BYTE cb,*red,*green,*blue,*intensity;
	BYTE batmp[4];
	USHORT ustmp,i;

	bytecount = 0;
	bpoffset = currline*bmplinelength;
	if (numplanes < 4)
		for (i = bytesperline*3; i < bytesperline*4; i++) tmpbuf[i] = 0xFF;
	bytestoget = numplanes*bytesperline;

	while (bytecount < bytestoget) {
		DosRead(pcxfile,&cb,1,&ustmp);
		if ((cb&COUNTMASK) == COUNTMASK) {
			repeatcount = (int)(cb&REPEATMASK);
			DosRead(pcxfile,&cb,1,&ustmp);
			while (repeatcount--) tmpbuf[bytecount++] = cb;
		}
		else tmpbuf[bytecount++] = cb;
	}

	red = tmpbuf;
	green = tmpbuf+bytesperline;
	blue = tmpbuf+bytesperline*2;
	intensity = tmpbuf+bytesperline*3;

	for (i = 0; i < bytesperline; i++) {
		for (j = 0; j < 4; j++) batmp[j] = 0;

		if (red[i]&BIT1) batmp[0] |= R1;
		if (red[i]&BIT2) batmp[0] |= R2;
		if (red[i]&BIT3) batmp[1] |= R1;
		if (red[i]&BIT4) batmp[1] |= R2;
		if (red[i]&BIT5) batmp[2] |= R1;
		if (red[i]&BIT6) batmp[2] |= R2;
		if (red[i]&BIT7) batmp[3] |= R1;
		if (red[i]&BIT8) batmp[3] |= R2;

		if (green[i]&BIT1) batmp[0] |= G1;
		if (green[i]&BIT2) batmp[0] |= G2;
		if (green[i]&BIT3) batmp[1] |= G1;
		if (green[i]&BIT4) batmp[1] |= G2;
		if (green[i]&BIT5) batmp[2] |= G1;
		if (green[i]&BIT6) batmp[2] |= G2;
		if (green[i]&BIT7) batmp[3] |= G1;
		if (green[i]&BIT8) batmp[3] |= G2;

		if (blue[i]&BIT1) batmp[0] |= B1;
		if (blue[i]&BIT2) batmp[0] |= B2;
		if (blue[i]&BIT3) batmp[1] |= B1;
		if (blue[i]&BIT4) batmp[1] |= B2;
		if (blue[i]&BIT5) batmp[2] |= B1;
		if (blue[i]&BIT6) batmp[2] |= B2;
		if (blue[i]&BIT7) batmp[3] |= B1;
		if (blue[i]&BIT8) batmp[3] |= B2;

		if (intensity[i]&BIT1) batmp[0] |= I1;
		if (intensity[i]&BIT2) batmp[0] |= I2;
		if (intensity[i]&BIT3) batmp[1] |= I1;
		if (intensity[i]&BIT4) batmp[1] |= I2;
		if (intensity[i]&BIT5) batmp[2] |= I1;
		if (intensity[i]&BIT6) batmp[2] |= I2;
		if (intensity[i]&BIT7) batmp[3] |= I1;
		if (intensity[i]&BIT8) batmp[3] |= I2;

		for (j = 0; j < 4; j++) bp[bpoffset++] = batmp[j];
	}
}

static BOOL near readcolors(cmi)
COLORMAPINFO *cmi;

{
	BYTE b;
	PCXRGBCOLOR pcxrgb;
	SHORT i;
	ULONG ultmp;
	USHORT ustmp;

	if (DosChgFilePtr(pcxfile,-769L,FILE_END,&ultmp)) return TRUE;
	if (DosRead(pcxfile,&b,1,&ustmp) || ustmp != 1 || b != 12) return TRUE;
	for (i = 0; i < 256; i++) {
		DosRead(pcxfile,&pcxrgb,sizeof(PCXRGBCOLOR),&ustmp);
	  	cmi->argbColor[i].bBlue = pcxrgb.bvalue;
	  	cmi->argbColor[i].bGreen = pcxrgb.gvalue;
	  	cmi->argbColor[i].bRed = pcxrgb.rvalue;
	}
	return FALSE;
}
