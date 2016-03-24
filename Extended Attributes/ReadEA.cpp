#define INCL_DOSFILEMGR

#include <os2.h>

#include <stdio.h>
#include <string.h>


void InitGEA(GEA2 *pGEA2, PCSZ pszName);
void AddGEA(GEA2 *pGEA2, PCSZ pszName);
ULONG CalcGEA2ListSize(GEA2LIST *pGEA2List);

BOOL GetFEA(PFEA2 pFEA2, PSZ pszEAName, PUSHORT pusEAID);
PSZ  GetFEAString(PFEA2 pFEA2, PSZ pszString);
BOOL GetNextFEA(PFEA2 *ppFEA2, PSZ pszEAName, PUSHORT pusEAID);


int main(int argc, char *argv[])
{
   APIRET   rc = 0;
   char     pchRecord[] = "RT.RECORD";
   char     pchArtist[] = "RT.ARTIST";
   char     pchTitle[] = "RT.TITLE";
   EAOP2    eaop2;
   USHORT   usEaID = 0;

   eaop2.fpGEA2List = (GEA2LIST*)new char[500];
   eaop2.fpFEA2List = (FEA2LIST*)new char[65536];
   eaop2.oError = 0;

   /* Make EA list */
   InitGEA(eaop2.fpGEA2List->list, (PCSZ)pchRecord);
   AddGEA(eaop2.fpGEA2List->list, (PCSZ)pchArtist);
   AddGEA(eaop2.fpGEA2List->list, (PCSZ)pchTitle);

   /* Set the total size of GetEA-list */
   eaop2.fpGEA2List->cbList = CalcGEA2ListSize(eaop2.fpGEA2List);

   /* Set maximum size of Fetched-Buffer */
   eaop2.fpFEA2List->cbList = 65536;

   /* Get the EA's */
   if((rc = DosQueryPathInfo(argv[1], FIL_QUERYEASFROMLIST, &eaop2, sizeof(eaop2))) != 0)
   {
      printf("DosQueryPathInfo failed with %d\n", rc);
      return 1;
   }

   /* Exit if no matching EA's where found */
   if(eaop2.fpFEA2List->list[0].cbValue == 0)
   {
      printf("No requested EA's found in file %s\n", argv[1]);
      return 2;
   }

   PSZ      pszTmp = new char[256];
   USHORT   usType;
   PFEA2    pFEA2 = &eaop2.fpFEA2List->list[0];

   if(GetFEA(pFEA2, pszTmp, &usType))
   {
      do
      {
         if(usType == EAT_ASCII)
         {
            printf("String EA: '%s'.\n", pszTmp);
            printf("  String: '%s'\n", GetFEAString(pFEA2, pszTmp));
         }
      }while(GetNextFEA(&pFEA2, pszTmp, &usType));
   }


   delete eaop2.fpFEA2List;
   delete eaop2.fpGEA2List;
}


void InitGEA(GEA2 *pGEA2, PCSZ pszName)
{
   GEA2 *pTmp = NULL;

   pTmp = pGEA2;

   pTmp->oNextEntryOffset = 0;
   pTmp->cbName = strlen(pszName);
   strcpy(pTmp->szName, pszName);
}

void AddGEA(GEA2 *pGEA2, PCSZ pszName)
{
   GEA2 *pTmp;

   pTmp = pGEA2;

   while(pTmp->oNextEntryOffset != 0)
      pTmp = (GEA2*)((char*)pTmp + pTmp->oNextEntryOffset);
   pTmp->oNextEntryOffset = (ULONG)(pTmp->cbName + 1 + sizeof(ULONG));

   if(pTmp->oNextEntryOffset)
      pTmp->oNextEntryOffset+= 4 - (pTmp->oNextEntryOffset % 4);

   pTmp = (GEA2*)((char*)pTmp + pTmp->oNextEntryOffset);
   pTmp->oNextEntryOffset = 0;
   pTmp->cbName = strlen(pszName);
   strcpy(pTmp->szName, pszName);
}

ULONG CalcGEA2ListSize(GEA2LIST *pGEA2List)
{
   GEA2  *pTmp;
   ULONG ulSize = 0;

   pTmp = pGEA2List->list;

   ulSize += sizeof(pGEA2List->cbList);

   while(pTmp->oNextEntryOffset != 0)
   {
      ulSize += sizeof(pTmp[0].oNextEntryOffset);
      ulSize += sizeof(pTmp[0].cbName);
      ulSize += pTmp->cbName;

      pTmp = (GEA2*)((char*)pTmp + pTmp->oNextEntryOffset);
   }
   ulSize += sizeof(pTmp->oNextEntryOffset);
   ulSize += sizeof(pTmp->cbName);
   ulSize += pTmp->cbName;

   if(ulSize % 4)
      ulSize += 4 - (ulSize % 4);

   return ulSize;
}



BOOL GetFEA(PFEA2 pFEA2, PSZ pszEAName, PUSHORT pusEAID)
{
   USHORT   usEaID = 0;

   strncpy(pszEAName, pFEA2->szName, pFEA2->cbName);
   pszEAName[pFEA2->cbName] = '\0';

   /* First word is the EA type ID */
   usEaID = *((unsigned short*)(pFEA2->szName + pFEA2->cbName+1));

   *pusEAID = usEaID;

   return TRUE;
}


PSZ GetFEAString(PFEA2 pFEA2, PSZ pszString)
{
   USHORT usLen = 0;

   usLen = *((unsigned short*)(pFEA2->szName + pFEA2->cbName + sizeof(USHORT)+1));

   strncpy(pszString, (const char*)(pFEA2->szName+pFEA2->cbName+1+(2*sizeof(USHORT))), usLen);
   pszString[usLen] = '\0';

   return pszString;
}


BOOL GetNextFEA(PFEA2 *ppFEA2, PSZ pszEAName, PUSHORT pusEAID)
{
   PFEA2 pFEA2 = *ppFEA2;

   if(pFEA2->oNextEntryOffset == 0)
      return FALSE;

   pFEA2 = (PFEA2)((char*)pFEA2 + pFEA2->oNextEntryOffset);

   GetFEA(pFEA2, pszEAName, pusEAID);

   *ppFEA2 = pFEA2;

   return TRUE;
}
