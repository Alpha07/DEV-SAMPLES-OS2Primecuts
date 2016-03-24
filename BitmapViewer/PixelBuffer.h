
typedef struct _PIXELBUFFER
{
   LONG cx;
   LONG cy;
   ULONG cBitCount;
   LONG cbLine;
   ULONG cbBitmap;
   BITMAPINFO2 bmi2;
   RECTL clip;
   PBYTE data;
}PIXELBUFFER, *PPIXELBUFFER;


PPIXELBUFFER _Optlink LoadPixelBuffer(PSZ pszFile);
void _Optlink FreePixelBuffer(PPIXELBUFFER pb);

