#pragma strings(readonly)

#define INCL_DOSFILEMGR
#define INCL_DOSDATETIME
#define INCL_DOSERRORS

#include <os2.h>

#include <stdio.h>
#include <memory.h>
#include <malloc.h>
#include <string.h>



typedef struct _COMMENTLINE
{
   struct _COMMENTLINE *next;
   char *ascii_line;
   USHORT cbLength;
}COMMENTLINE, *PCOMMENTLINE;

typedef struct _FILENODE
{
   struct _FILENODE *next;
   char achName[CCHMAXPATH];
   FDATE fdate;
   ULONG cbFile;
   PCOMMENTLINE firstcommentline;
}FILENODE, *PFILENODE;

typedef struct _QUEUENODE
{
   struct _QUEUENODE *next;
   char path[CCHMAXPATH];
   char prev[256];
}QUEUENODE, *PQUEUENODE;


void syntax(char *argv0);
BOOL writeFilelistHeader(HFILE hFile);
BOOL writeTreeDescriptions(PSZ pszBasePath, HFILE hFile);



PFILENODE getFilelist(PQUEUENODE n);

void write_file_description(PCOMMENTLINE commentline, HFILE hFile, USHORT width);


/*
 * argv[1] - base directory
 * argv[2] - output file
 *
 * options:
 *    -a append to file list (default overwrite)
 */
int main(int argc, char *argv[])
{
   APIRET rc = NO_ERROR;
   ULONG fsOpenFlags = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS;
   ULONG fsOpenMode = OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYWRITE | OPEN_ACCESS_WRITEONLY;
   ULONG hFile = NULLHANDLE;
   ULONG ulAction = 0;
   BOOL fAppend = FALSE;

   /*
    * Make sure base directory has been specified
    */
   if(argc < 2)
   {
      puts("Error: No base directory specified");
      syntax(argv[0]);
      return 1;
   }

   /*
    * Make sure output filename has been specified
    */
   if(argc < 3)
   {
      puts("Error: No file list filename specified");
      syntax(argv[0]);
      return 1;
   }

   /*
    * Check aditional parameters
    */
   if(argc > 2)
   {
      int i = 3;
      for(; i < argc; i++)
      {
         if(argv[i][0] == '-' || argv[i][0] == '/')
         {
            switch(argv[i][1])
            {
               case 'a':
               case 'A':
                  fAppend = TRUE;
                  puts("append");
                  break;
            }
         }
      }
   }

   if(fAppend)
   {
      fsOpenFlags = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS;
   }

   /*
    * Attempt to open output file
    */
   rc = DosOpen(argv[2], &hFile, &ulAction, 0UL, FILE_NORMAL, fsOpenFlags, fsOpenMode, NULL);
   if(rc == NO_ERROR)
   {
      if(ulAction == FILE_EXISTED)
      {
         ULONG ibActual = 0;

         /*
          * File already existed (and it wasn't replaced by DosOpen()), assume that
          * the append option was used and set file pointer to the end of the file.
          */
         rc = DosSetFilePtr(hFile, 0L, FILE_END, &ibActual);
      }
      else
      {
         /*
          * Write a file header
          */
         writeFilelistHeader(hFile);
      }

      writeTreeDescriptions(argv[1], hFile);

      rc = DosClose(hFile);
   }
   return 0;
}

void syntax(char *argv0)
{
   printf("Usage: %s <basedir> <outputfile> [options]\n", argv0);
   puts("options: -a - append list to <outfilefile> (overwrite by default)");

}

BOOL writeFilelistHeader(HFILE hFile)
{
   BOOL fSuccess = FALSE;
   APIRET rc = NO_ERROR;
   char tmp[256] = "";
   ULONG cbWritten = 0;
   char header[] = "File list for Treadstone BBS\r\n";

   if((rc = DosWrite(hFile, header, sizeof(header), &cbWritten)) == NO_ERROR)
   {
      DATETIME dt = { 0 };

      if((rc = DosGetDateTime(&dt)) == NO_ERROR)
      {
         sprintf(tmp, "Generated on %d-%02d-%02d\r\n", dt.year, dt.month, dt.day);
         if((rc = DosWrite(hFile, tmp, strlen(tmp), &cbWritten)) == NO_ERROR)
         {
            rc = DosWrite(hFile, "\r\n", 2UL, &cbWritten);
            if(rc == NO_ERROR)
            {
               fSuccess = TRUE;
            }
         }
      }
   }
   return fSuccess;
}

BOOL writeTreeDescriptions(PSZ pszBasePath, HFILE hFile)
{
   APIRET rc = NO_ERROR;
   ULONG ulFindMax = 32;
   ULONG cbFindBuf = ulFindMax*sizeof(FILEFINDBUF3);
   PFILEFINDBUF3 findbuf = malloc(cbFindBuf);
   ULONG flAttribute = FILE_ARCHIVED | FILE_READONLY | FILE_DIRECTORY | MUST_HAVE_DIRECTORY;
   ULONG cbBasePath = strlen(pszBasePath);
   EAOP2 eaop2 = { NULL, NULL, 0UL };
   PQUEUENODE node = NULL;
   PQUEUENODE addnode = NULL;
   PQUEUENODE tmpNode = NULL;
   const char achDescription[] = ".COMMENTS";
   const char achLongname[] = ".LONGNAME";
   ULONG cbWritten = 0;
   GEA2 *pgea2 = NULL;
   PBYTE ptmp = NULL;
   ULONG ulTemp = 0;

   /*
    * Prepare EAOP2 structure for fetching extended attributes
    */
   /* GEA2 List */
   eaop2.fpGEA2List = (PGEA2LIST)malloc(128);
   memset(eaop2.fpGEA2List, 0, 128);
   eaop2.fpGEA2List->cbList = sizeof(eaop2.fpGEA2List->cbList);

   pgea2 = &eaop2.fpGEA2List->list[0];

   /* Set up GEA2 to fetch .COMMENTS attribute */
   pgea2->cbName = sizeof(achDescription)-1;
   memcpy(pgea2->szName, achDescription, sizeof(achDescription));
   eaop2.fpGEA2List->cbList += (sizeof(GEA2)+pgea2->cbName);


   pgea2->oNextEntryOffset = sizeof(GEA2)+pgea2->cbName;
   pgea2->oNextEntryOffset += (4-(pgea2->oNextEntryOffset%4));

   ulTemp = (PBYTE)pgea2;
   ulTemp += pgea2->oNextEntryOffset;
   pgea2 = (GEA2*)ulTemp;

   /* Set up GEA2 to fetch .LONGNAME attribute */
   pgea2->oNextEntryOffset = 0;
   pgea2->cbName = sizeof(achLongname)-1;
   memcpy(pgea2->szName, achLongname, sizeof(achLongname));
   eaop2.fpGEA2List->cbList += (sizeof(GEA2)+pgea2->cbName);


   /* Allocate FEA2 List */
   eaop2.fpFEA2List = (PFEA2LIST)malloc(65536);


   /*
    * Create initial node
    */
   node = (PQUEUENODE)malloc(sizeof(QUEUENODE));
   memset(node, 0, sizeof(QUEUENODE));
   node->next = NULL;
   strcpy(node->path, pszBasePath);

   while(node)
   {
      ULONG cFilenames = ulFindMax;
      HFILE hFind = HDIR_CREATE;
      char area[CCHMAXPATH] = "";
      char path[CCHMAXPATH] = "";
      char findspec[CCHMAXPATH] = "";
      PFILENODE fnode = NULL;
      char achAreaName[256] = "";
      PBYTE p = node->path+cbBasePath;
      PFEA2 fea2desc = NULL;

      addnode = node;

      /*
       * Get file list for curent directory node
       */
      fnode = getFilelist(node);

      /* Attempt to parse directory name */
      while(*p)
      {
         p++;
      }
      while(*p != '\\')
      {
         if(p == (node->path+cbBasePath))
         {
            break;
         }
         p--;
      }
      if(*p == '\\')
      {
         p++;
      }

      if(p > (node->path+cbBasePath))
      {
         /*
          * Path isn't root, get file's EA:s
          */
         strcpy(achAreaName, p);

         eaop2.fpFEA2List->cbList = 65536;
         rc = DosQueryPathInfo(node->path, FIL_QUERYEASFROMLIST, &eaop2, sizeof(EAOP2));
         if(rc == NO_ERROR)
         {
            FEA2 *fea2 = &eaop2.fpFEA2List->list[0];

            while(1)
            {
               if((strcmp(fea2->szName, achLongname) == 0) && (fea2->cbValue != 0))
               {
                  PBYTE pValue = fea2->szName + (fea2->cbName+1);
                  USHORT eaType = MAKESHORT(*pValue, *(pValue+1));

                  /*
                   * Copy .LONGNAME attribute value to achAreaName
                   */
                  if(eaType == EAT_ASCII)
                  {
                     USHORT eaLen = MAKESHORT(*(pValue+2), *(pValue+3));

                     memcpy(achAreaName, (pValue+4), eaLen);
                     achAreaName[eaLen] = '\0';
                  }
               }
               else if((strcmp(fea2->szName, achDescription) == 0) && (fea2->cbValue != 0))
               {
                  /*
                   * A .COMMENTS attribute was found for directory, write it.
                   */
                  fea2desc = fea2;
               }

               if(fea2->oNextEntryOffset == 0)
               {
                  /*
                   * Break out of loop if current FEA2 entry is the last in the list.
                   */
                  break;
               }

               ulTemp = (ULONG)fea2;
               ulTemp += fea2->oNextEntryOffset;
               fea2 = (FEA2*)ulTemp;
            }
         }
      }

      if(fnode)
      {
         /*
          * Write area header
          */
         rc = DosWrite(hFile, "\r\n*\r\n* Area: ", 13UL, &cbWritten);
         if(node->prev[0])
         {
            DosWrite(hFile, node->prev, strlen(node->prev), &cbWritten);
            DosWrite(hFile, " -> ", 4UL, &cbWritten);
         }
         DosWrite(hFile, achAreaName, strlen(achAreaName), &cbWritten);
         rc = DosWrite(hFile, "\r\n*\r\n", 5UL, &cbWritten);

         if(fea2desc)
         {
            PBYTE pValue = fea2desc->szName + (fea2desc->cbName+1);
            USHORT eaType = MAKESHORT(*pValue, *(pValue+1));

            if((strcmp(fea2desc->szName, achDescription) == 0) && (fea2desc->cbValue != 0) && (eaType == EAT_MVMT))
            {
               USHORT codepage = MAKESHORT(*(pValue+2), *(pValue+3));   /* EAT_MVMT codepage */
               USHORT entries = MAKESHORT(*(pValue+4), *(pValue+5));    /* EAT_MVMT entries  */
               USHORT i = 0;
               PBYTE p = pValue + 6;

               /*
                * Write .COMMENTS attribute
                */
               rc = DosWrite(hFile, "\r\n", 2UL, &cbWritten);

               for(i = 0; i < entries; i++)
               {
                  USHORT eaLen = 0;

                  eaType = MAKESHORT(*p, *(p+1));
                  p += 2;
                  eaLen = MAKESHORT(*p, *(p+1));
                  p += 2;

                  switch(eaType)
                  {
                     case EAT_ASCII:
                        rc = DosWrite(hFile, "   ", 3UL, &cbWritten);
                        DosWrite(hFile, p, eaLen, &cbWritten);
                        rc = DosWrite(hFile, "\r\n", 2UL, &cbWritten);
                        break;
                  }
                  p+= eaLen;
               }
               rc = DosWrite(hFile, "\r\n", 2UL, &cbWritten);
            }
         }

         /*
          * Write file list
          */
         while(fnode)
         {
            char tmp[1024] = "";
            PFILENODE nextNode = fnode->next;
            PCOMMENTLINE commentline = fnode->firstcommentline;

            /*
             * Write file information
             */
            sprintf(tmp, "%-16s %8d %d-%02d-%02d\r\n", fnode->achName, fnode->cbFile, 1980+fnode->fdate.year, fnode->fdate.month, fnode->fdate.day);
            DosWrite(hFile, tmp, strlen(tmp), &cbWritten);

            if(fnode->firstcommentline)
            {
               /*
                * Write file's description
                */
               write_file_description(fnode->firstcommentline, hFile, 80);
            }
            else
            {
               char achNoDescription[] = "    No description available.\r\n";
               DosWrite(hFile, achNoDescription, sizeof(achNoDescription)-1, &cbWritten);
            }

            /*
             * free node memory
             */
            while(commentline)
            {
               PCOMMENTLINE nextLine = commentline->next;

               free(commentline->ascii_line);
               free(commentline);
               commentline = nextLine;
            }
            free(fnode);

            fnode = nextNode;
         }
      }
      strcpy(findspec, node->path);
      strcat(findspec, "\\*");

      /*
       * check if directory has sub directories
       */
      rc = DosFindFirst(findspec, &hFind, flAttribute, findbuf, cbFindBuf, &cFilenames, FIL_STANDARD);
      if(rc == NO_ERROR)
      {
         PFILEFINDBUF3 pfile = findbuf;

         while(cFilenames--)
         {
            char *pTmp = (char*)pfile;

            if(strcmp(pfile->achName, ".") != 0 && strcmp(pfile->achName, "..") != 0)
            {
               /*
                * Add new node
                */
               tmpNode = addnode->next;
               addnode->next = (PQUEUENODE)malloc(sizeof(QUEUENODE));
               memset(addnode->next, 0, sizeof(QUEUENODE));
               addnode = addnode->next;
               addnode->next = tmpNode;


               strcpy(addnode->path, node->path);
               strcat(addnode->path, "\\");
               strcat(addnode->path, pfile->achName);

               if(node->prev[0] == '\0')
               {
                  strcat(addnode->prev, achAreaName);
               }
               else
               {
                  strcpy(addnode->prev, node->prev);
                  strcat(addnode->prev, " -> ");
                  strcat(addnode->prev, achAreaName);
               }
            }

            /* Next file in list */
            pTmp += pfile->oNextEntryOffset;
            pfile = (FILEFINDBUF3*)pTmp;
         }
         /* Find next list of files */
         cFilenames = ulFindMax;
         rc = DosFindNext(hFind, findbuf, cbFindBuf, &cFilenames);
      }
      rc = DosFindClose(hFind);

      /* Next node in queue */
      tmpNode = node;
      node = node->next;
      free(tmpNode);
   }

   free(eaop2.fpFEA2List);
   free(eaop2.fpGEA2List);

   return TRUE;
}


PFILENODE getFilelist(PQUEUENODE n)
{
   APIRET rc = NO_ERROR;
   char filepath[CCHMAXPATH] = "";
   HFILE hFind = HDIR_CREATE;
   ULONG flAttribute = FILE_ARCHIVED | FILE_READONLY;
   EAOP2 eaop2 = { NULL, NULL, 0UL };
   PBYTE pFilename = NULL;
   const ULONG ulFindMax = 64;
   PFILEFINDBUF3 findbuf = calloc(ulFindMax, sizeof(FILEFINDBUF3));
   ULONG cbFindBuf = ulFindMax*sizeof(FILEFINDBUF3);
   ULONG cFilenames = ulFindMax;
   const char achDescription[] = ".COMMENTS";

   PFILENODE filenode = NULL;
   PFILENODE firstfilenode = NULL;


   /*
    * Prepare EAOP2 structure for fetching extended attributes
    */
   /* GEA2 */
   eaop2.fpGEA2List = (PGEA2LIST)malloc(128);
   memset(eaop2.fpGEA2List, 0, 128);
   eaop2.fpGEA2List->cbList = sizeof(eaop2.fpGEA2List->cbList);

   eaop2.fpGEA2List->list[0].oNextEntryOffset = 0;
   eaop2.fpGEA2List->list[0].cbName = sizeof(achDescription)-1;
   memcpy(eaop2.fpGEA2List->list[0].szName, achDescription, sizeof(achDescription));
   eaop2.fpGEA2List->cbList += (sizeof(GEA2)+eaop2.fpGEA2List->list[0].cbName);
   if(eaop2.fpGEA2List->cbList % 4)
   {
      eaop2.fpGEA2List->cbList += (4-(eaop2.fpGEA2List->cbList%4));
   }

   /* FEA2 */
   eaop2.fpFEA2List = (PFEA2LIST)malloc(65536);

   /*
    * Set up filespec
    */
   strcpy(filepath, n->path);
   strcat(filepath, "\\*");

   pFilename = filepath + strlen(filepath);

   /*
    * Get file list
    */
   rc = DosFindFirst(filepath, &hFind, flAttribute, findbuf, cbFindBuf, &cFilenames, FIL_STANDARD);
   while(rc == NO_ERROR)
   {
      PFILEFINDBUF3 pfile = findbuf;
      ULONG cbWritten = 0;

      while(cFilenames--)
      {
         BOOL fDescription = FALSE;
         char *pTmp = (char*)pfile;
         char tmp[260] = "";

         /*
          * Create a new FILENODE node for current file
          */
         if(filenode == NULL)
         {
            filenode = malloc(sizeof(FILENODE));
            firstfilenode = filenode;
         }
         else
         {
            filenode->next = malloc(sizeof(FILENODE));
            filenode = filenode->next;
         }
         memset(filenode, 0, sizeof(FILENODE));

         /*
          * Set node file information
          */
         strcpy(filenode->achName, pfile->achName);
         filenode->cbFile = pfile->cbFile;
         memcpy(&filenode->fdate, &pfile->fdateCreation, sizeof(FDATE));


         strcpy(pFilename, pfile->achName);
         strcpy(filepath, n->path);
         strcat(filepath, "\\");
         strcat(filepath, pFilename);

         /* Reset FEA2 list size */
         eaop2.fpFEA2List->cbList = 65536;

         /*
          * Get file's EA:s
          */
         rc = DosQueryPathInfo(filepath, FIL_QUERYEASFROMLIST, &eaop2, sizeof(eaop2));
         if(rc == NO_ERROR)
         {
            FEA2 *fea2 = &eaop2.fpFEA2List->list[0];
            PBYTE pValBuf = fea2->szName + (fea2->cbName+1);
            USHORT eaType = MAKESHORT(*pValBuf, *(pValBuf+1));

            if((strcmp(fea2->szName, achDescription) == 0) && (fea2->cbValue != 0) && (eaType == EAT_MVMT))
            {
               USHORT codepage = MAKESHORT(*(pValBuf+2), *(pValBuf+3));   /* EAT_MVMT codepage */
               USHORT entries = MAKESHORT(*(pValBuf+4), *(pValBuf+5));    /* EAT_MVMT entries  */
               USHORT i = 0;
               PBYTE p = pValBuf + 6;
               PCOMMENTLINE commentline = NULL;

               /*
                * A .COMMENTS attribute was found for file
                */

               for(i = 0; i < entries; i++)
               {
                  USHORT eaLen = 0;

                  eaType = MAKESHORT(*p, *(p+1));
                  p += 2;
                  eaLen = MAKESHORT(*p, *(p+1));
                  p += 2;

                  switch(eaType)
                  {
                     case EAT_ASCII:
                        if(commentline == NULL)
                        {
                           commentline = filenode->firstcommentline = malloc(sizeof(COMMENTLINE));
                        }
                        else
                        {
                            commentline->next = malloc(sizeof(COMMENTLINE));
                            commentline = commentline->next;
                        }
                        memset(commentline, 0, sizeof(COMMENTLINE));

                        commentline->cbLength = eaLen;
                        commentline->ascii_line = malloc(commentline->cbLength);
                        memcpy(commentline->ascii_line, p, eaLen);
                        break;
                  }
                  p+= eaLen;
               }
            }
         }
         else
         {
            printf("rc = %u\n", rc);
         }

         /* Next file in list */
         pTmp += pfile->oNextEntryOffset;
         pfile = (FILEFINDBUF3*)pTmp;
      }

      cFilenames = ulFindMax;
      rc = DosFindNext(hFind, findbuf, cbFindBuf, &cFilenames);
   }
   DosFindClose(hFind);

   free(eaop2.fpFEA2List);
   free(eaop2.fpGEA2List);

   return firstfilenode;
}


/*
 * Write file description in a "word wrapping" fashion.
 */
void write_file_description(PCOMMENTLINE commentline, HFILE hFile, USHORT width)
{
   while(commentline)
   {
      APIRET rc = NO_ERROR;
      USHORT i = 0;
      USHORT iStart = 0;
      USHORT iScreen = 4;
      PBYTE pBuf = commentline->ascii_line;
      USHORT cbLen = commentline->cbLength;

      while(i < cbLen)
      {
         ULONG cbWritten = 0;

         if(pBuf[i] == ' ')
         {
            i += 1;
         }

         iStart = i;

         while(iScreen < width && i < cbLen)
         {
            i++;
            iScreen++;
         }
         if(i < cbLen)
         {
            if(pBuf[i] != ' ')
            {
               USHORT iTmp = i;
               while(pBuf[i] != ' ')
               {
                  i--;
                  if(i < iStart)
                  {
                     i = iTmp;
                     break;
                  }
               }
            }
         }
         rc = DosWrite(hFile, "    ", 4, &cbWritten);
         rc = DosWrite(hFile, &pBuf[iStart], i-iStart, &cbWritten);
         rc = DosWrite(hFile, "\r\n", 2, &cbWritten);
         iScreen = 4;
      }
      commentline = commentline->next;
   }
}
