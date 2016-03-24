/*
 * This program will create a priority queue implemented as a heap.
 * A heap is a 'balanced tree'
 * A priority queue is a nifty thing. You insert nodes into the queue, each
 * node has a priority. You can then get the first node which always has the
 * top priority.
 *
 * You won't be able to use this source directly in any project. It's not
 * generic enough. ToDo to make it more modular:
 *  - In the PQUEUE structre; add two new entries:
 *      1. A function pointer to a function which will be used to compare
 *         nodes.
 *      2. A node-size variable.
 *  - You need to design a function which will compare two nodes.
 *  - If you're planning on using the queue in multithreaded/shared
 *    application you'll need another variable in the PQUEUE structure:
 *    a mutex sempahore handle. In OS/2, it's common design to make all
 *    libraries multithread safe. Remember to request (and release) the mutex
 *    sempahores in the push and pop functions.
 *
 * References:
 *  http://hissa.ncsl.nist.gov/~black/CRCDict/
 *  http://www.ee.uwa.edu.au/~plsd210/ds/
 */
#pragma strings(readonly)

#define INCL_DOSMEMMGR
#define INCL_DOSEXCEPTIONS
#define INCL_DOSERRORS

#include <os2.h>

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


/*
 * Each node in the priority queue
 */
typedef struct _NODE
{
   ULONG event;
   ULONG priority;
   PVOID param1;
   PVOID param2;
}NODE, *PNODE;

/*
 * Priority queue header
 */
typedef struct _PQUEUE
{
   ULONG cNodes;
   NODE aNodes[1];
}PQUEUE, *PPQUEUE;


/*
 * Function prototypes
 */
int pqueue_push(PPQUEUE pQueue, PNODE pNew);
PNODE pqueue_pop(PPQUEUE pQueue, PNODE pNode);
PNODE pqueue_pop2(PPQUEUE pQueue);
int pqueue_delete(PPQUEUE pQueue);
int _Inline pqueue_shiftup(PPQUEUE pQueue, ULONG i);

static ULONG _System exception_handler(PEXCEPTIONREPORTRECORD pERepRec, PEXCEPTIONREGISTRATIONRECORD pERegRep, PCONTEXTRECORD pCtxRec, PVOID p);


/*
 * Push a new node onto the heap/priority queue
 */
int pqueue_push(PPQUEUE pQueue, PNODE pNew)
{
   PNODE aNodes = &pQueue->aNodes[0];
   int i = 0, j = 0;

   pQueue->cNodes++;

   for(j = pQueue->cNodes; j > 1; j = i)
   {
      i = j / 2;
      if(aNodes[i].priority >= pNew->priority)
         break;
      memcpy(&aNodes[j], &aNodes[i], sizeof(NODE));
   }
   memcpy(&aNodes[j], pNew, sizeof(NODE));

   return 1;
}


/*
 * Get the top priority queue
 */
PNODE pqueue_pop(PPQUEUE pQueue, PNODE pNode)
{
   if(pQueue->cNodes < 1)
   {
      return NULL;
   }
   memcpy(pNode, &pQueue->aNodes[1], sizeof(NODE));

   pqueue_delete(pQueue);

   return pNode;
}

/*
 * Since node 0 isn't used, it can be used as a temporary storage
 * for the pop:ed node.
 * Important node: Unless you protect the first node with a sempahore
 * (which isn't good design) this version of getting the top priority node
 * is not thread safe!
 */
PNODE pqueue_pop2(PPQUEUE pQueue)
{
   if(pQueue->cNodes < 1)
   {
      return NULL;
   }
   memcpy(&pQueue->aNodes[0], &pQueue->aNodes[1], sizeof(NODE));

   pqueue_delete(pQueue);

   return &pQueue->aNodes[0];
}

/*
 * Strictly internal function - do not call from application code
 */
int pqueue_delete(PPQUEUE pQueue)
{
   if(pQueue->cNodes < 1)
   {
      return 0;
   }
   else
   {
      memcpy(&pQueue->aNodes[1], &pQueue->aNodes[pQueue->cNodes], sizeof(NODE));
      pQueue->cNodes--;
      pqueue_shiftup(pQueue, 1);
   }
   return 1;
}

/*
 * Strictly internal function - do not call from application code
 */
int _Inline pqueue_shiftup(PPQUEUE pQueue, ULONG i)
{
   NODE tmpNode;
   int j = 0;
   int n = pQueue->cNodes;
   PNODE aNodes = &pQueue->aNodes[0];

   while((j = 2*i) <= n)
   {
      if((j < n) && aNodes[j].priority < aNodes[j+1].priority)
         j++;

      if(aNodes[i].priority < aNodes[j].priority)
      {
         memcpy(&tmpNode, &aNodes[j], sizeof(NODE));
         memcpy(&aNodes[j], &aNodes[i], sizeof(NODE));
         memcpy(&aNodes[i], &tmpNode, sizeof(NODE));
         i = j;
      }
      else
      {
         break;
      }
   }
   return 1;
}



int main(int argc, char *argv)
{
   APIRET rc = NO_ERROR;
   PVOID pAlloc = NULL;
   PPQUEUE heap = NULL;
   int iRet = 0;
   EXCEPTIONREGISTRATIONRECORD xcpthand = { 0, &exception_handler };
   int i = 0;

   srand(time(NULL));

   /*
    * The whole idea with this sample is to demonstrate how to allocate memory
    * 'as needed'. The best way to do this is to use an exception handler
    */
   rc = DosSetExceptionHandler(&xcpthand);

   /*
    * Allocate a very large buffer. This will fit approx 524280 nodes
    * which is way more than you'll need.
    */
   rc = DosAllocMem(&pAlloc, 16*1024*1024, PAG_READ | PAG_WRITE);
   if(rc == NO_ERROR)
   {
      heap = pAlloc;
   }

   /*
    * Add some nodes with random priorities
    */
   if(heap)
   {
      NODE tmpNode = { 0 };
      PNODE node = NULL;

      puts("Add nodes");
      for(i = 0; i < 100; i++)
      {
         tmpNode.priority = rand();
         printf("Added node with priority: %u\n", tmpNode.priority);
         pqueue_push(heap, &tmpNode);
      }

      /*
       * Actually, it's neater to use the nodes-counter..
       */
      puts("\n\nEmpty heap");
      while((node = pqueue_pop2(heap)) != NULL)
      {
         printf("Priority: %u\n", node->priority);
      }
   }


   /*
    * Free heap buffer
    */
   if(heap)
   {
      DosFreeMem(heap);
   }

   /*
    * All registered exception handlers must be deregistered
    */
   DosUnsetExceptionHandler(&xcpthand);

   return iRet;
}



/*
 * Exception handler; attempt to commit memory if access violation occures.
 */
static ULONG _System exception_handler(PEXCEPTIONREPORTRECORD pERepRec, PEXCEPTIONREGISTRATIONRECORD pERegRep, PCONTEXTRECORD pCtxRec, PVOID p)
{
   ULONG ulRet = XCPT_CONTINUE_EXECUTION;

   switch(pERepRec->ExceptionNum)
   {
      case XCPT_ACCESS_VIOLATION:
         ulRet = XCPT_CONTINUE_SEARCH;

         if(pERepRec->ExceptionAddress == (PVOID)XCPT_DATA_UNKNOWN)
         {
         }
         else
         {
            APIRET rc = NO_ERROR;
            ULONG cbMem = 1;
            ULONG flMem = 0;

            rc = DosQueryMem((PVOID)pERepRec->ExceptionInfo[1], &cbMem, &flMem);
            if((flMem & PAG_FREE) == 0)
            {
               rc = DosSetMem((PVOID)pERepRec->ExceptionInfo[1], 4096, PAG_DEFAULT | PAG_COMMIT);
               if(rc == NO_ERROR)
               {
                  ulRet = XCPT_CONTINUE_EXECUTION;
               }
            }
         }
         break;

      default:
         ulRet = XCPT_CONTINUE_SEARCH;
         break;
   }

   return ulRet;
}

