/***************************************************************************\
* Module Name: readgif.c
\***************************************************************************/

#define MAXBITS 12
#define LZSTRINGSIZE 1024

#define INCL_DOSMEMMGR
#define INCL_GPIBITMAPS

#include <os2.h>
#include <memory.h>

typedef struct {
	ULONG cbFix;
	USHORT cx;
	USHORT cy;
	USHORT cPlanes;
	USHORT cBitCount;
	RGB argbColor[256];
} COLORMAPINFO;

char far * far pascal gifextension(HMODULE);
ULONG far pascal getgifsize(HFILE);
BOOL far pascal readgif(HFILE, SEL, COLORMAPINFO far *);
static int near getcode(int);
static void near putpixel(int);
static void near getimage(void);

static int near width;
static int near height;
static int near gcbits;
static int near iwidth;
static int near iheight;
static int near xoff;
static int near yoff;
static int near eodcode;
static int near bitsleft;
static int near intpass;      
static int near imgx;
static int near imgy;
static unsigned char near rbp;
static unsigned char near rblen;
static unsigned char * near rasterblock;
static unsigned char huge * near scn;
static long near totalblocksize;
static unsigned long near codebuf;
static BOOL near interlaced;
static BOOL near overflow;
static BOOL near newscn;
static HFILE near hf;

char far * far pascal gifextension(thismodule)
HMODULE thismodule;

{
	thismodule;
	return ".GIF";
}

ULONG far pascal getgifsize(lhf)
HFILE lhf;

{
	unsigned char temp[10];
	USHORT bytesread;

	if (DosRead(lhf,temp,10,&bytesread) || bytesread != 10) return 0L;
	if (temp[0] != 'G' || temp[1] != 'I' || temp[2] != 'F') return 0L;
	width = temp[6]+((int)temp[7]<<8);
	height = temp[8]+((int)temp[9]<<8);

	totalblocksize = (ULONG)height*((width+3)&0xfffc);
	return totalblocksize;
}

BOOL far pascal readgif(lhf,sel,cmi)
HFILE lhf;
SEL sel;
COLORMAPINFO far *cmi;

{
	int i;
	unsigned char c,bkg;
	unsigned char temp[256];
	USHORT hshift,bytesread;
	ULONG l;

	hf = lhf;
	scn = MAKEP(sel,0);
	DosRead(hf,&c,1,&bytesread);
	gcbits = (c&0x07)+1;

	DosGetHugeShift(&hshift);
	DosRead(hf,&bkg,1,&bytesread);
	for (i = 0; i < (int)(totalblocksize>>16); i++) {
		memset(MAKEP(sel+(i<<hshift),0),bkg,0x8000);
		memset(MAKEP(sel+(i<<hshift),0x8000),bkg,0x8000);
	}
	if (totalblocksize&0xffff)
		memset(MAKEP(sel+(i<<hshift),0),bkg,(USHORT)totalblocksize);

	DosChgFilePtr(hf,1L,FILE_CURRENT,&l);

	cmi->cx = width;
	cmi->cy = height;
	cmi->cPlanes = 1;
	cmi->cBitCount = 8;
	if (c&0x80)	for (i = 0; i < (1<<gcbits); i++) {
		DosRead(hf,&cmi->argbColor[i].bRed,1,&bytesread);
		DosRead(hf,&cmi->argbColor[i].bGreen,1,&bytesread);
		DosRead(hf,&cmi->argbColor[i].bBlue,1,&bytesread);
	} 

	rasterblock = temp;
	c = 0;
	while (c != ';') {
		DosRead(hf,&c,1,&bytesread);
		if (!bytesread) break;
		if (c == ',') getimage();
		if (c == '!') {
			DosChgFilePtr(hf,1L,FILE_CURRENT,&l);
			DosRead(hf,&c,1,&bytesread);
			if (!bytesread) break;

			while (c) {
				DosChgFilePtr(hf,(long)c,FILE_CURRENT,&l);
				DosRead(hf,&c,1,&bytesread);
				if (!bytesread) return FALSE;
			}
		}
	}
	return FALSE;
}

static int near getcode(codesize)
int codesize;

{
	int i;
	USHORT bytesread;

	while (bitsleft < codesize) {
		if (rbp >= rblen) {
			DosRead(hf,&rblen,1,&bytesread);
			if (!bytesread || !rblen) return eodcode;
			DosRead(hf,rasterblock,(USHORT)rblen,&bytesread);
			if (bytesread != (USHORT)rblen) return eodcode;
			rbp = 0;
		}
		codebuf |= (unsigned long)(rasterblock[rbp++])<<bitsleft;
		bitsleft += 8;
	}
	i = (int)codebuf;
	codebuf >>= codesize;
	bitsleft -= codesize;
	return i;
}

static void near putpixel(color)
int color;

{
	static ULONG near scnoffset;
	static int intstart[] = { 0, 4, 2, 1 };
	static int intinc[] = { 8, 8, 4, 2 };

	if (!overflow) {
		if (newscn) {
			scnoffset = (ULONG)(height-imgy-yoff-1)*((width+3)&0xfffc)+xoff;
			newscn = FALSE;
		}

		scn[scnoffset++] = (unsigned char)color; 

		if (++imgx >= iwidth) {
			imgx = 0;
			newscn = TRUE;
			if (interlaced) {
				if ((imgy += intinc[intpass]) >= iheight) {
					if (++intpass >= 4) overflow = TRUE;
					else imgy = intstart[intpass];
				}
			}
			else if (++imgy >= iheight) overflow = TRUE;
		}
	}
}

static void near getimage()

{
	int c,i,code,initialcodesize,clearcode,maxcode,maxcol,nextcode;
	int csmask,codesize,cbits,finchar,oldcode;
	int prefix[1<<MAXBITS];
	unsigned char tempch;
	unsigned char *bufp,*tempp;
	unsigned char lastchar[1<<MAXBITS];
	unsigned char buf[LZSTRINGSIZE];
	USHORT bytesread;
	ULONG l;

	rbp = rblen = 0;
	bitsleft = intpass = imgx = imgy = 0;
	codebuf = 0L;
	overflow = FALSE;
	newscn = TRUE;
	finchar = oldcode = 0;
	tempp = buf;

	DosRead(hf,prefix,8,&bytesread);

	xoff = prefix[0];
	yoff = prefix[1];
	iwidth = prefix[2];
	iheight = prefix[3];

	DosRead(hf,&tempch,1,&bytesread);
	if (!bytesread) return;

	interlaced = ((tempch&0x40) == 0x40);
	if (tempch&0x80) {
		cbits = (tempch&0x07)+1;
		DosChgFilePtr(hf,3L*(1<<cbits),FILE_CURRENT,&l);
	}
	else cbits = gcbits;

	DosRead(hf,&tempch,1,&bytesread);
	if (!bytesread) return;
	initialcodesize = tempch;
	codesize = ++initialcodesize;

	if (codesize < 3 || codesize > 9) return;

	for (i = 0; i < (1<<MAXBITS); i++) {
		lastchar[i] = 0;
		prefix[i] = 0;
	}

	maxcol = (1<<cbits)-1;
	clearcode = 1<<(codesize-1);
	eodcode = clearcode+1;
	nextcode = clearcode+2;
	maxcode = clearcode<<1;
	csmask = maxcode-1;
	bufp = buf+sizeof(buf);

	while ((c = csmask&getcode(codesize)) != eodcode) {
		if (c == clearcode) {
			codesize = initialcodesize;
			nextcode = clearcode+2;
			maxcode = 1<<codesize;
			csmask = maxcode-1;

			while ((c = csmask&getcode(codesize)) == clearcode);
			if (c == eodcode) break;
			if (c >= nextcode) c = 0;

			putpixel(oldcode = finchar = c);
		}
		else {
			code = c;
			if (code == nextcode) {
				*--bufp = (unsigned char)finchar;
				code = oldcode;
			}
			else if (code > nextcode) return;

			while (code > maxcol) {
				if (bufp <= tempp) while (bufp < tempp+sizeof(buf)) putpixel(*bufp++);
				*--bufp = lastchar[code];
				code = prefix[code];
			}
			finchar = code;
			*--bufp = (unsigned char)finchar;
			while (bufp < tempp+sizeof(buf)) putpixel(*bufp++);

			if (nextcode < maxcode) {
				prefix[nextcode] = oldcode;
				finchar = code;
				lastchar[nextcode] = (unsigned char)finchar;
				oldcode = c;
			}

			if (++nextcode >= maxcode && codesize < MAXBITS) {
				++codesize;
				maxcode <<= 1;
				csmask = maxcode-1;
			}
		}
	}
}
