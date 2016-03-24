#define INCL_DOSFILEMGR

#include <os2.h>

#include <iostream.h>
#include <string.h>


void SetFEA2(PFEA2 pFEA2, PCSZ pszName, USHORT usType, USHORT ulValSize, PVOID pValue, BYTE fEA = 0);
void AddFEA2(PFEA2 *ppFEA2, PCSZ pszName, USHORT usType, USHORT ulValSize, PVOID pValue, BYTE fEA = 0);
USHORT MakeFEA2String(PVOID pszOutBuf, PSZ pszInString);
USHORT GetFEA2Size(PFEA2LIST pFEA2List);


int main(int argc, char *argv[])
{
   APIRET   rc = 0;
   EAOP2    eaop2;
   char     pchInBuf[255];
   PVOID    pTmpBuf;

   eaop2.fpGEA2List = 0;
   eaop2.fpFEA2List = PFEA2LIST(new char[65535]);

   PFEA2 pFEA2 = &eaop2.fpFEA2List->list[0];

   pTmpBuf = new char[255];


   cout << "Artist: ";
   cin >> pchInBuf;

   USHORT   usLen;
   usLen = MakeFEA2String(pTmpBuf, pchInBuf);
   SetFEA2(pFEA2, "RT.ARTIST", EAT_ASCII, usLen, pTmpBuf, 0);

   cout << "Title: ";
   cin >> pchInBuf;

   usLen = MakeFEA2String(pTmpBuf, pchInBuf);
   AddFEA2(&pFEA2, "RT.TITLE", EAT_ASCII, usLen, pTmpBuf, 0);

   cout << "Record: ";
   cin >> pchInBuf;

   usLen = MakeFEA2String(pTmpBuf, pchInBuf);
   AddFEA2(&pFEA2, "RT.RECORD", EAT_ASCII, usLen, pTmpBuf, 0);


   eaop2.fpFEA2List->cbList = GetFEA2Size(&eaop2.fpFEA2List[0]);

   rc = DosSetPathInfo(argv[1], FIL_QUERYEASIZE, &eaop2, sizeof(eaop2), 0);
   if(rc != 0)
   {
      cout << "Error!" << endl;
   }
}


void SetFEA2(PFEA2 pFEA2, PCSZ pszName, USHORT usType, USHORT usValSize, PVOID pValue, BYTE fEA)
{
   /* Set the required-flag */
   pFEA2->fEA = fEA;

   /* Set the length of the EA-name and EA-value to the FEA structure*/
   pFEA2->cbName = strlen(pszName);
   pFEA2->cbValue = usValSize+sizeof(USHORT);  /* the extra USHORT for the type identifier */

   /* Copy the EA-name to the FEA-buffer */
   strcpy(pFEA2->szName, pszName);

   /* Set a pointer for the EA-value after the EA-name */
   char *pData = pFEA2->szName+pFEA2->cbName+1;

   /* Store the value-type */
   *(PUSHORT)pData = EAT_ASCII;

   /* After the type, we store the actual EA-value */
   memcpy(pData+sizeof(USHORT), pValue, usValSize);

   /* Make sure that this is the last entry */
   pFEA2->oNextEntryOffset = 0;
}


void AddFEA2(PFEA2 *ppFEA2, PCSZ pszName, USHORT usType, USHORT usValSize, PVOID pValue, BYTE fEA)
{
   PFEA2 pFEA2 = *ppFEA2;

   pFEA2->oNextEntryOffset = sizeof(FEA2) + pFEA2->cbName + pFEA2->cbValue + 1;

   if(pFEA2->oNextEntryOffset)
      pFEA2->oNextEntryOffset+= 4 - (pFEA2->oNextEntryOffset % 4);

   pFEA2 = (FEA2*)((char*)pFEA2 + pFEA2->oNextEntryOffset);

   SetFEA2(pFEA2, pszName, usType, usValSize, pValue, fEA);

   *ppFEA2 = pFEA2;
}


USHORT MakeFEA2String(PVOID pszOutBuf, PSZ pszInString)
{
   USHORT   usLen = 0;

   usLen = strlen(pszInString);

   *(PUSHORT)pszOutBuf = usLen;

   strncpy((char*)pszOutBuf+2, pszInString, usLen);

   return usLen+sizeof(USHORT);
}


USHORT GetFEA2Size(PFEA2LIST pFEA2List)
{
   USHORT   usSize = 0;
   PFEA2    pFEA2 = &pFEA2List->list[0];

   usSize += sizeof(pFEA2List->cbList);

   while(pFEA2->oNextEntryOffset != 0)
   {
      usSize += sizeof(pFEA2[0].oNextEntryOffset);
      usSize += sizeof(pFEA2[0].fEA);
      usSize += sizeof(pFEA2[0].cbName);
      usSize += sizeof(pFEA2[0].cbValue);

      usSize += pFEA2[0].cbName;
      usSize += pFEA2[0].cbValue;

      pFEA2 = pFEA2 + pFEA2->oNextEntryOffset;
   }
   usSize += sizeof(pFEA2[0].oNextEntryOffset);
   usSize += sizeof(pFEA2[0].fEA);
   usSize += sizeof(pFEA2[0].cbName);
   usSize += sizeof(pFEA2[0].cbValue);

   usSize += pFEA2[0].cbName;
   usSize += pFEA2[0].cbValue;

   return usSize;
}
