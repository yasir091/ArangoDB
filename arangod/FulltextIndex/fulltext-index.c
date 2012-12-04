////////////////////////////////////////////////////////////////////////////////
/// @brief full text search
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author R. A. Parker  
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "fulltext-index.h"

#include "BasicsC/locks.h"
#include "BasicsC/logging.h"

#include "FulltextIndex/zstr.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief not a valid kkey - 52 bits long!
////////////////////////////////////////////////////////////////////////////////

#define NOTFOUND 0xF777777777777

/* codes - in zcode.c so need externs here  */
extern ZCOD zcutf;
extern ZCOD zcbky;
extern ZCOD zcdelt;
extern ZCOD zcdoc;
extern ZCOD zckk;
extern ZCOD zcdh;

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual index struct used
////////////////////////////////////////////////////////////////////////////////

typedef struct { 
  TRI_read_write_lock_t _lock;
  void*                 _context; // arbitrary context info the index passed to getTexts
  int                   _options;

  FTS_collection_id_t   _colid;   /* collection ID for this index  */
  FTS_document_id_t*    _handles;    /* array converting handles to docid */
  uint8_t*              _handsfree;
  FTS_document_id_t     _firstfree;   /* Start of handle free chain.  */
  FTS_document_id_t     _lastslot;
  TUBER*                _index1;
  TUBER*                _index2;
  TUBER*                _index3;

  FTS_texts_t* (*getTexts)(FTS_collection_id_t, FTS_document_id_t, void*);
} 
FTS_real_index;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get a unicode character from UTF-8
////////////////////////////////////////////////////////////////////////////////

static uint64_t GetUnicode (uint8_t** ptr) {
  uint64_t c1;

  c1 = **ptr;
  if (c1 < 128) {
    (*ptr)++;
    return c1;
  }

  if (c1 < 224) {
    c1 = ((c1 - 192) << 6) + (*((*ptr) + 1) - 128);
    (*ptr) += 2;
    return c1;
  }

  if (c1 < 240) {
    c1 = ((c1 - 224) << 12) + ((*((*ptr) + 1) - 128) << 6) + (*((*ptr) + 2) - 128);
    (*ptr) += 3;
    return c1;
  }

  if (c1 < 248) {
    c1 = ((c1 - 240) << 18) + ((*((*ptr) + 1) - 128) << 12) + ((*((*ptr) + 2) - 128) << 6) + (*((*ptr) + 3) - 128);
    (*ptr) += 4;
    return c1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a document to the index
////////////////////////////////////////////////////////////////////////////////

static void RealAddDocument (FTS_index_t* ftx, FTS_document_id_t docid) {
    FTS_real_index * ix;
    FTS_texts_t *rawwords;
    CTX ctx2a, ctx2b, x3ctx, x3ctxb;
    STEX * stex;
    ZSTR *zstrwl, *zstr2a, *zstr2b, *x3zstr, *x3zstrb;
    uint64_t letters[42];
    uint64_t ixlet[42];
    uint64_t kkey[42];    /* for word *without* this letter */
    uint64_t kkey1[42];   /* ix1 word whose last letter is this */
    int ixlen;
    uint16_t * wpt;
    uint64_t handle, newhan, oldhan;
    uint64_t kroot;
    uint64_t kroot1 = 0; /* initialise even if unused. this will prevent compiler warnings */
    int nowords,wdx;
    int i,j,len,j1,j2;
    uint8_t * utf;
    uint64_t unicode;
    uint64_t tran,x64,oldlet, newlet;
    uint64_t bkey = 0;
    uint64_t docb,dock;

    ix = (FTS_real_index *)ftx;
    kroot=ZStrTuberK(ix->_index2,0,0,0);
    if(ix->_options == FTS_INDEX_SUBSTRINGS)
        kroot1=ZStrTuberK(ix->_index1,0,0,0);
    kkey[0]=kroot;     /* origin of index 2 */

/*     allocate the document handle  */
    handle = ix->_firstfree;
/* TBD what to do if no more handles  */
    if(handle==0)
    {
        printf("Run out of document handles!\n");
        return;
    }
    ix->_firstfree = ix->_handles[handle];
    ix->_handles[handle]=docid;
    ix->_handsfree[handle]=0;
    
/*     Get the actual words from the caller  */
    rawwords = ix->getTexts(ix->_colid, docid, ix->_context);
    if (rawwords == NULL) {
      return;
    }

    nowords=rawwords->_len;
/*     Put the words into a STEX */

    stex=ZStrSTCons(2);  /* format 2=uint16 is all that there is! */
    zstrwl=ZStrCons(25);  /* 25 enough for word list  */
    zstr2a=ZStrCons(30);  /* 30 uint64's is always enough for ix2  */
    zstr2b=ZStrCons(30);
    x3zstr=ZStrCons(35);
    x3zstrb=ZStrCons(35);
    for(i=0;i<nowords;i++)
    {
        utf=rawwords->_texts[i];
        j=0;
        ZStrClear(zstrwl);
        unicode=GetUnicode(&utf);
        while(unicode!=0)
        {
            ZStrEnc(zstrwl,&zcutf,unicode);
            unicode=GetUnicode(&utf);
            j++;
            if(j>40) {
              break;
            }
        }
/* terminate the word and insert into STEX  */
        ZStrEnc(zstrwl,&zcutf,0);
        ZStrNormalize(zstrwl);
        ZStrSTAppend(stex,zstrwl);
    }
/*     Sort them */
    ZStrSTSort(stex);
/*     Set current length of word = 0 */
    ixlen=0;
/*     For each word in the STEX */
    nowords=stex->cnt;
    wpt=(uint16_t *) stex->list;
    for(wdx=0;wdx<nowords;wdx++)
    {
/*         get it out as a word  */
        ZStrInsert(zstrwl,wpt,2);
        len=0;
        while(1)
        {
            letters[len]=ZStrDec(zstrwl,&zcutf);
            if(letters[len]==0) break;
            len++;
        }

        wpt+=ZStrExtLen(wpt,2);
/*         find out where it first differs from previous one */
        for(j=0;j<ixlen;j++)
        {
            if(letters[j]!=ixlet[j]) break;
        }
/* For every new letter in the word, get its K-key into array */
        while(j<len)
        {
/*     obtain the translation of the letter  */
            tran=ZStrXlate(&zcutf,letters[j]);
/*     Get the Z-string for the index-2 entry before this letter */
            i=ZStrTuberRead(ix->_index2,kkey[j],zstr2a);
            if(i==1)
            {
                printf("Kkey not found - we're buggered\n");
            }

            x64=ZStrBitsOut(zstr2a,1);
            if(x64==1) 
            {
/*     skip over the B-key into index 3  */
                docb=ZStrDec(zstr2a,&zcbky);
            }
/*     look to see if the letter is there  */
            ZStrCxClear(&zcdelt, &ctx2a);
            newlet=0;
            while(1)
            {
                oldlet=newlet;
                newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                if(newlet==oldlet) break;
                bkey=ZStrDec(zstr2a,&zcbky);
                if(newlet>=tran) break;
            }
            if(newlet != tran)
            {
/*       if not there, create a new index-2 entry for it  */
                bkey=ZStrTuberIns(ix->_index2,kkey[j],tran);
                kkey[j+1]=ZStrTuberK(ix->_index2,kkey[j],tran,bkey); 
/*     update old index-2 entry to insert new letter  */
                ZStrCxClear(&zcdelt, &ctx2a);
                ZStrCxClear(&zcdelt, &ctx2b);
                i=ZStrTuberRead(ix->_index2,kkey[j],zstr2a);
                ZStrClear(zstr2b);
                x64=ZStrBitsOut(zstr2a,1);
                ZStrBitsIn(x64,1,zstr2b);
                if(x64==1) 
                {
/*     copy over the B-key into index 3  */
                    docb=ZStrDec(zstr2a,&zcbky);
                    ZStrEnc(zstr2b,&zcbky,docb);
                }
                newlet=0;
                while(1)
                {
                    oldlet=newlet;
                    newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                    if(newlet==oldlet) break;
                    if(newlet>tran) break;
                    ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                    x64=ZStrDec(zstr2a,&zcbky);
                    ZStrEnc(zstr2b,&zcbky,x64);
                }
                ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,tran);
                ZStrEnc(zstr2b,&zcbky,bkey);
                if(newlet==oldlet)
                {
                    ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,tran);
                }
                else
                {
                    while(newlet!=oldlet)
                    {
                        oldlet=newlet;
                        ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                        x64=ZStrDec(zstr2a,&zcbky);
                        ZStrEnc(zstr2b,&zcbky,x64);
                        newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                    }
                    ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                }
                ZStrNormalize(zstr2b);
                ZStrTuberUpdate(ix->_index2,kkey[j],zstr2b);
            }
            else
            {
/*     - if it is, get its KKey and put in (next) slot */
                kkey[j+1]=ZStrTuberK(ix->_index2,kkey[j],tran,bkey);
            }
            j++;
        }
/*      kkey[j] is kkey of whole word.     */
/*      so read the zstr from index2       */
        i=ZStrTuberRead(ix->_index2,kkey[j],zstr2a);
        if(i==1)
        {
            printf("Kkey not found - we're running for cover\n");
        }
/*  is there already an index-3 entry available?  */
        x64=ZStrBitsOut(zstr2a,1);
/*     If so, get its b-key  */
        if(x64==1)
        {
            docb=ZStrDec(zstr2a,&zcbky);
        }
        else
        {
            docb=ZStrTuberIns(ix->_index3,kkey[j],0);
/*     put it into index 2 */
            ZStrCxClear(&zcdelt, &ctx2a);
            ZStrCxClear(&zcdelt, &ctx2b);
            i=ZStrTuberRead(ix->_index2,kkey[j],zstr2a);
            ZStrClear(zstr2b);
            x64=ZStrBitsOut(zstr2a,1);
            ZStrBitsIn(1,1,zstr2b);
            ZStrEnc(zstr2b,&zcbky,docb);
            newlet=0;
            while(1)
            {
                oldlet=newlet;
                newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                if(newlet==oldlet) break;
                ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                x64=ZStrDec(zstr2a,&zcbky);
                ZStrEnc(zstr2b,&zcbky,x64);
            }
            ZStrNormalize(zstr2b);
            ZStrTuberUpdate(ix->_index2,kkey[j],zstr2b); 
        }
        dock=ZStrTuberK(ix->_index3,kkey[j],0,docb);
/*     insert doc handle into index 3      */
        i=ZStrTuberRead(ix->_index3,dock,x3zstr);
        ZStrClear(x3zstrb);
        if(i==1)
        {
            printf("Kkey not found in ix3 - we're doomed\n");
        }
        ZStrCxClear(&zcdoc, &x3ctx);
        ZStrCxClear(&zcdoc, &x3ctxb);
        newhan=0;
        while(1)
        {
            oldhan=newhan;
            newhan=ZStrCxDec(x3zstr,&zcdoc,&x3ctx);
            if(newhan==oldhan) break;
            if(newhan>handle) break;
            ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,newhan);
        }
        ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,handle);
        if(newhan==oldhan)
            ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,handle);
        else
        {
            ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,newhan);
            while(newhan!=oldhan)
            {
                oldhan=newhan;
                newhan=ZStrCxDec(x3zstr,&zcdoc,&x3ctx);
                ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,newhan);
            }
        }
        ZStrNormalize(x3zstrb);
        ZStrTuberUpdate(ix->_index3,dock,x3zstrb); 
/*      copy the word into ix                */
        ixlen=len;
        for(j=0;j<len;j++) ixlet[j]=letters[j];
        
        if(ix->_options==FTS_INDEX_SUBSTRINGS)
        {
            for(j1=0;j1<len;j1++)
            {
                kkey1[j1+1]=kroot1;
                for(j2=j1;j2>=0;j2--)
                {
                    tran=ZStrXlate(&zcutf,ixlet[j2]);
                    i=ZStrTuberRead(ix->_index1,kkey1[j2+1],zstr2a);
                    if(i==1)
                    {
          printf("Kkey not found - we're in trouble!\n");
                    }
/*     look to see if the letter is there  */
                    ZStrCxClear(&zcdelt, &ctx2a);
                    newlet=0;
                    while(1)
                    {
                        oldlet=newlet;
                        newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                        if(newlet==oldlet) break;
                        bkey=ZStrDec(zstr2a,&zcbky);
                        if(newlet>=tran) break;
                    }
                    if(newlet != tran)
                    {

/*       if not there, create a new index-1 entry for it  */
                        bkey=ZStrTuberIns(ix->_index1,kkey1[j2+1],tran);
                        kkey1[j2]=ZStrTuberK(ix->_index1,kkey1[j2+1],tran,bkey); 
/*     update old index-1 entry to insert new letter  */
                        ZStrCxClear(&zcdelt, &ctx2a);
                        ZStrCxClear(&zcdelt, &ctx2b);
                        i=ZStrTuberRead(ix->_index1,kkey1[j2+1],zstr2a);
                        ZStrClear(zstr2b);
                        newlet=0;
                        while(1)
                        {
                            oldlet=newlet;
                            newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                            if(newlet==oldlet) break;
                            if(newlet>tran) break;
                            ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                            x64=ZStrDec(zstr2a,&zcbky);
                            ZStrEnc(zstr2b,&zcbky,x64);
                        }
                        ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,tran);
                        ZStrEnc(zstr2b,&zcbky,bkey);
                        if(newlet==oldlet)
                        {
                            ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,tran);
                        }
                        else
                        {
                            while(newlet!=oldlet)
                            {
                                oldlet=newlet;
                                ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                                x64=ZStrDec(zstr2a,&zcbky);
                                ZStrEnc(zstr2b,&zcbky,x64);
                                newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                            }
                            ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                        }
                        ZStrNormalize(zstr2b);
                        ZStrTuberUpdate(ix->_index1,kkey1[j2+1],zstr2b);
                    }
                    else
                    {
                        kkey1[j2]=ZStrTuberK(ix->_index1,kkey1[j2+1],tran,bkey);
                    }
                }
            }
        }
    }
    ZStrSTDest(stex);
    ZStrDest(zstrwl);
    ZStrDest(zstr2a);
    ZStrDest(zstr2b);
    ZStrDest(x3zstr);
    ZStrDest(x3zstrb);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document from the index
////////////////////////////////////////////////////////////////////////////////

static void RealDeleteDocument (FTS_index_t* ftx, FTS_document_id_t docid) {
  FTS_real_index* ix;
  FTS_document_id_t i;

  ix = (FTS_real_index*) ftx;
  for (i = 1; i <= ix->_lastslot; i++) {
    if (ix->_handsfree[i] == 1) {
      continue;
    }

    if (ix->_handles[i] == docid) {
      break;
    }
  }

  if (i > ix->_lastslot) {
    /* TBD - what to do if a document is deleted that isn't there?  */
    printf("tried to delete nonexistent document\n");
  }

  ix->_handsfree[i] = 1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief TODO
////////////////////////////////////////////////////////////////////////////////

static uint64_t FindKKey1 (FTS_real_index* ix, uint64_t* word) {
  ZSTR* zstr;
  CTX ctx;
  uint64_t* wd;
  uint64_t tran, newlet, oldlet, bkey, kk1;
  int j;

  zstr = ZStrCons(10);
  wd = word;

  while (*wd != 0) {
    wd++;
  }
  kk1 = ZStrTuberK(ix->_index2, 0, 0, 0);

  while(1) {
    if (wd == word) {
      break;
    }

    tran = *(--wd);
/*     Get the Z-string for the index-1 entry of this key */
    j = ZStrTuberRead(ix->_index1, kk1, zstr);
    if (j == 1) {
      kk1 = NOTFOUND;
      break;
    }

    ZStrCxClear(&zcdelt, &ctx);
    newlet = 0;
    while (1) {
      oldlet = newlet;
      newlet = ZStrCxDec(zstr, &zcdelt, &ctx);
      if (newlet == oldlet) {
        kk1 = NOTFOUND;
        break;
      }

      bkey = ZStrDec(zstr, &zcbky);
      if (newlet > tran) {
        kk1 = NOTFOUND;
        break;
      }
      if (newlet == tran) {
        break;
      }
    }
    if (kk1 == NOTFOUND) {
      break;
    }

    kk1 = ZStrTuberK(ix->_index1, kk1, tran, bkey);
  }

  ZStrDest(zstr);
  return kk1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief TODO
////////////////////////////////////////////////////////////////////////////////

static uint64_t FindKKey2 (FTS_real_index* ix, uint64_t* word) {
  ZSTR* zstr;
  CTX ctx;
  uint64_t kk2;
  int j;

  zstr = ZStrCons(10);
  kk2 = ZStrTuberK(ix->_index2, 0, 0, 0);

  while (1) {
    uint64_t tran;
    uint64_t x64;
    uint64_t newlet;
    uint64_t bkey;

    tran = *(word++);
    if (tran==0) {
      break;
    }
/*     Get the Z-string for the index-2 entry of this key */
    j = ZStrTuberRead(ix->_index2, kk2, zstr);
    if (j == 1) {
      kk2=NOTFOUND;
      break;
    }

    x64=ZStrBitsOut(zstr, 1);
    if (x64 == 1) {
      uint64_t docb;

/*     skip over the B-key into index 3  */
      docb = ZStrDec(zstr, &zcbky);
/* silly use of docb to get rid of compiler warning  */
      if (docb == 0xffffff) {
        printf("impossible\n");
      }
    }
    ZStrCxClear(&zcdelt, &ctx);
    
    newlet = 0;
    while (1) {
      uint64_t oldlet;

      oldlet = newlet;
      newlet = ZStrCxDec(zstr, &zcdelt, &ctx);
      if (newlet == oldlet) {
        kk2 = NOTFOUND;
        break;
      }

      bkey = ZStrDec(zstr, &zcbky);
      if (newlet > tran) {
        kk2 = NOTFOUND;
        break;
      }
      if (newlet == tran) {
        break;
      }
    }

    if (kk2 == NOTFOUND) {
      break;
    }

    kk2 = ZStrTuberK(ix->_index2, kk2, tran, bkey);
  }

  ZStrDest(zstr);
  return kk2;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief TODO
/// for each query term, update zstra2 to only contain handles matching that
/// also recursive index 2 handles kk2 to dochan STEX using zcdh 
////////////////////////////////////////////////////////////////////////////////

static void Ix2Recurs (STEX* dochan, FTS_real_index* ix, uint64_t kk2) {
  ZSTR* zstr2;
  ZSTR* zstr3;
  ZSTR* zstr;
  CTX ctx2, ctx3;
  uint64_t newlet;
  uint64_t x64; 
  int j;

  zstr2 = ZStrCons(10);  /* index 2 entry for this prefix */
  zstr3 = ZStrCons(10);  /* index 3 entry for this prefix */
                               /* if any  */
  zstr = ZStrCons(2);  /* single doc handle work area */

  j = ZStrTuberRead(ix->_index2, kk2, zstr2);
  if (j == 1) {
    // TODO: change to return code
    printf("recursion failed to read kk2\n");
    exit(1);
  }

  x64 = ZStrBitsOut(zstr2, 1);
  if (x64 == 1) {
/* process the documents into the STEX  */
/* uses zcdh not LastEnc because it must sort into */
/* numerical order                                 */
    uint64_t docb;
    uint64_t dock;
    uint64_t newhan;
    int i;

    docb = ZStrDec(zstr2, &zcbky);
    dock = ZStrTuberK(ix->_index3, kk2, 0, docb);
    i = ZStrTuberRead(ix->_index3, dock, zstr3);
    if (i == 1) {
      printf("Kkey not in ix3 - we're doomed\n");
    }
    ZStrCxClear(&zcdoc, &ctx3);

    newhan = 0;
    while (1) {
      uint64_t oldhan;

      oldhan = newhan;
      newhan = ZStrCxDec(zstr3, &zcdoc, &ctx3);
      if (newhan == oldhan) {
        break;
      }

      if (ix->_handsfree[newhan] == 0) {
        ZStrClear(zstr);
        ZStrEnc(zstr, &zcdh, newhan);
        ZStrSTAppend(dochan, zstr);
      }
    }
  }
  ZStrCxClear(&zcdelt, &ctx2);

  newlet = 0;
  while (1) {
    uint64_t oldlet;
    uint64_t newkk2;
    uint64_t bkey;

    oldlet = newlet;
    newlet = ZStrCxDec(zstr2, &zcdelt, &ctx2);
    if (newlet == oldlet) {
      break;
    }
    bkey = ZStrDec(zstr2, &zcbky);
    newkk2 = ZStrTuberK(ix->_index2, kk2, newlet, bkey);
    Ix2Recurs(dochan, ix, newkk2);
  }

  ZStrDest(zstr2);
  ZStrDest(zstr3);
  ZStrDest(zstr);    
}

////////////////////////////////////////////////////////////////////////////////
/// @brief TODO
////////////////////////////////////////////////////////////////////////////////

static void Ix1Recurs (STEX* dochan, FTS_real_index* ix, uint64_t kk1, uint64_t* wd) {
  ZSTR* zstr;
  CTX ctx;
  int j;
  uint64_t newlet, kk2;

  kk2 = FindKKey2(ix,wd);

  if (kk2 != NOTFOUND) {
    Ix2Recurs(dochan,ix,kk2);
  }

  zstr = ZStrCons(10);  /* index 1 entry for this prefix */
  j = ZStrTuberRead(ix->_index1, kk1, zstr);
  if (j == 1) {
    printf("recursion failed to read kk1\n");
    // TODO: change to error code
    exit(1);
  }

  ZStrCxClear(&zcdelt, &ctx);
  newlet = 0;

  while (1) {
    uint64_t oldlet;
    uint64_t bkey;
    uint64_t newkk1;

    oldlet = newlet;
    newlet = ZStrCxDec(zstr, &zcdelt, &ctx);
    if (newlet == oldlet) {
      break;
    }
    bkey = ZStrDec(zstr, &zcbky);
    newkk1 = ZStrTuberK(ix->_index1, kk1, newlet, bkey);
    *(wd - 1) = newlet;
    Ix1Recurs(dochan, ix, newkk1, wd - 1);
  }

  ZStrDest(zstr);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new fulltext index
////////////////////////////////////////////////////////////////////////////////

FTS_index_t* FTS_CreateIndex (FTS_collection_id_t coll,
                              void* context,
                              FTS_texts_t* (*getTexts)(FTS_collection_id_t, FTS_document_id_t, void*),
                              int options, 
                              uint64_t sizes[10]) {
/* sizes[0] = size of handles table to start with  */
/* sizes[1] = number of bytes for index 1 */
/* sizes[2] = number of bytes for index 2 */
/* sizes[3] = number of bytes for index 3 */
  FTS_real_index* ix;
  uint64_t bk;
  int i;
        
  ix = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(FTS_real_index), false);
  if (ix == NULL) {
    return NULL;
  }

  ix->_handles = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (sizes[0] + 2) * sizeof(FTS_document_id_t), false);
  if (ix->_handles == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);
    return NULL;
  }

  ix->_handsfree = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (sizes[0] + 2) * sizeof(uint8_t), false);
  if (ix->_handsfree == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handles);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);
    return NULL;
  }
  
  ix->_colid = coll;
  ix->_context = context;
  ix->getTexts = getTexts;

/* set up free chain of document handles  */
  for (i = 1; i < sizes[0]; i++) {
    ix->_handles[i]   = i+1;
    ix->_handsfree[i] = 1;
  }

  ix->_handles[sizes[0]] = 0;  /* end of free chain  */
  ix->_handsfree[sizes[0]] = 1;
  ix->_firstfree = 1;
  ix->_lastslot = sizes[0];

/*     create index 2 */
  ix->_index2 = ZStrTuberCons(sizes[2], TUBER_BITS_8);
  bk = ZStrTuberIns(ix->_index2, 0, 0);
  if (bk != 0) {
    printf("Help - Can't insert root of index 2\n");
  }

/*     create index 3 */
  ix->_index3 = ZStrTuberCons(sizes[3], TUBER_BITS_32);
  ix->_options = options;
/*     create index 1 if needed */
  if (ix->_options == FTS_INDEX_SUBSTRINGS) {
    ix->_index1 = ZStrTuberCons(sizes[1], TUBER_BITS_8);
    bk = ZStrTuberIns(ix->_index1,0,0);
    if (bk != 0) {
      printf("Help - Can't insert root of index 1\n");
    }
  }
  
  TRI_InitReadWriteLock(&ix->_lock);

  return (FTS_index_t*) ix;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an existing fulltext index
////////////////////////////////////////////////////////////////////////////////

void FTS_FreeIndex (FTS_index_t* ftx) {
  FTS_real_index* ix;

  ix = (FTS_real_index*) ftx;

  TRI_DestroyReadWriteLock(&ix->_lock);

  if (ix->_options == FTS_INDEX_SUBSTRINGS) {
    ZStrTuberDest(ix->_index1);
  }

  ZStrTuberDest(ix->_index2);
  ZStrTuberDest(ix->_index3);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handsfree);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handles);
  
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a document to the index
////////////////////////////////////////////////////////////////////////////////

void FTS_AddDocument(FTS_index_t* ftx, FTS_document_id_t docid) {
  FTS_real_index* ix = (FTS_real_index*) ftx;

  TRI_WriteLockReadWriteLock(&ix->_lock);
  RealAddDocument(ftx,docid);
  TRI_WriteUnlockReadWriteLock(&ix->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document from the index
////////////////////////////////////////////////////////////////////////////////

void FTS_DeleteDocument(FTS_index_t* ftx, FTS_document_id_t docid) {
  FTS_real_index* ix = (FTS_real_index*) ftx;

  TRI_WriteLockReadWriteLock(&ix->_lock);
  RealDeleteDocument(ftx,docid);
  TRI_WriteUnlockReadWriteLock(&ix->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update an existing document in the index
////////////////////////////////////////////////////////////////////////////////

void FTS_UpdateDocument(FTS_index_t* ftx, FTS_document_id_t docid) {
  FTS_real_index* ix = (FTS_real_index*) ftx;

  TRI_WriteLockReadWriteLock(&ix->_lock);
  RealDeleteDocument(ftx,docid);
  RealAddDocument(ftx,docid);
  TRI_WriteUnlockReadWriteLock(&ix->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief current not called. TODO: find out what its intention is
////////////////////////////////////////////////////////////////////////////////

void FTS_BackgroundTask(FTS_index_t* ftx) {
  /* obtain LOCKMAIN */
  /* remove deleted handles from index3 not done QQQ  */
  /* release LOCKMAIN */
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform a search in the index
////////////////////////////////////////////////////////////////////////////////

FTS_document_ids_t* FTS_FindDocuments (FTS_index_t* ftx,
                                       FTS_query_t* query) {
    FTS_document_ids_t* dc;
    FTS_real_index* ix;
    size_t queryterm;
    ZSTR *zstr2, *zstr3;
    ZSTR *zstra1, *zstra2, *ztemp;
    ZSTR *zstr;
    STEX * dochan;
    CTX ctxa1, ctxa2;
    CTX ctx3;
    uint64_t word1[100];
    int i,j;
    uint64_t kk2,kk1,x64,docb,dock;
    uint64_t oldhan,newhan,ndocs,lasthan,odocs;
    uint64_t nhand1,ohand1;
    uint8_t * utf;
    uint64_t unicode;
    uint16_t *docpt;

    ix = (FTS_real_index*) ftx;

    dc = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(FTS_document_ids_t), false);
    if (dc == NULL) {
      // out of memory
      return NULL;
    }
    
    dc->_len = 0;     /* no docids so far  */
    dc->_docs = NULL;

    zstr2  = ZStrCons(10);  /* from index-2 tuber */
    zstr3  = ZStrCons(10);  /* from index-3 tuber  */
    zstra1 = ZStrCons(10); /* current list of documents */
    zstra2 = ZStrCons(10); /* new list of documents  */
    zstr   = ZStrCons(4);  /* work zstr from stex  */
    
    ndocs = 0;
    
    TRI_ReadLockReadWriteLock(&ix->_lock);

/*     - for each term in the query */
    for (queryterm = 0; queryterm < query->_len; queryterm++) {
        if (query->_localOptions[queryterm] == FTS_MATCH_SUBSTRING &&
            ix->_options != FTS_INDEX_SUBSTRINGS) {
          TRI_ReadUnlockReadWriteLock(&ix->_lock);

          ZStrDest(zstra1);
          ZStrDest(zstra2);
          ZStrDest(zstr); 
          ZStrDest(zstr2); 
          ZStrDest(zstr3);  
          return NULL;
        }

/*  Depending on the query type, the objective is do   */
/*  populate or "and" zstra1 with the sorted list      */
/*  of document handles that match that term           */
/*  TBD - what to do if it is not a legal option?  */
/* TBD combine this with other options - no need to use zstring  */
        ndocs = 0;
            
        if(query->_localOptions[queryterm] == FTS_MATCH_COMPLETE)
        {
            j=0;
            utf= query->_texts[queryterm];
            while(1)
            {
                unicode=GetUnicode(&utf);
                word1[j++]=ZStrXlate(&zcutf,unicode);
                if(unicode==0) break;
                if(j>40) break;
            }
            word1[j]=0;
            kk2=FindKKey2(ix,word1);
            if(kk2==NOTFOUND) {
              break;
            }
            j=ZStrTuberRead(ix->_index2,kk2,zstr2);
            x64=ZStrBitsOut(zstr2,1);
            if(x64!=1) break;
            docb=ZStrDec(zstr2,&zcbky);
            dock=ZStrTuberK(ix->_index3,kk2,0,docb);
            i=ZStrTuberRead(ix->_index3,dock,zstr3);
            if(i==1)
            {
                printf("Kkey not in ix3 - we're terrified\n");
            }
            ZStrCxClear(&zcdoc, &ctx3);
            ZStrCxClear(&zcdoc, &ctxa2);
            ZStrClear(zstra2);
            newhan=0;
            lasthan=0;
            ndocs=0;
            if(queryterm==0)
            {
                while(1)
                {
                    oldhan=newhan;
                    newhan=ZStrCxDec(zstr3,&zcdoc,&ctx3);
                    if(newhan==oldhan) break;
                    if(ix->_handsfree[newhan]==0)
                    {
                        ZStrCxEnc(zstra2,&zcdoc,&ctxa2,newhan);
                        lasthan=newhan;
                        ndocs++;
                    }
                }

            }
            else
            {
                ZStrCxClear(&zcdoc, &ctxa1);
                ohand1=0;
                nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                oldhan=0;
                newhan=ZStrCxDec(zstr3,&zcdoc,&ctx3);
/*     zstra1 = zstra1 & zstra2  */
                while(1)
                {
                    if(nhand1==ohand1) break;
                    if(oldhan==newhan) break;
                    if(newhan==nhand1)
                    {
                        if(ix->_handsfree[newhan]==0)
                        {
                            ZStrCxEnc(zstra2,&zcdoc,&ctxa2,newhan);
                            lasthan=newhan;
                            ndocs++;
                        }
                        oldhan=newhan;
                        newhan=ZStrCxDec(zstr3,&zcdoc,&ctx3);
                        ohand1=nhand1;
                        nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                    }
                    else if(newhan>nhand1)
                    {
                        ohand1=nhand1;
                        nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                    }
                    else
                    {
                        oldhan=newhan;
                        newhan=ZStrCxDec(zstr3,&zcdoc,&ctx3);
                    }
                }
            }
            ZStrCxEnc(zstra2,&zcdoc,&ctxa2,lasthan);
            ZStrNormalize(zstra2);
            ztemp=zstra1;
            zstra1=zstra2;
            zstra2=ztemp;
        }  /* end of match-complete code  */
        if (  (query->_localOptions[queryterm] == FTS_MATCH_PREFIX)  ||
              (query->_localOptions[queryterm] == FTS_MATCH_SUBSTRING)  )
        {
/*  Make STEX to contain new list of handles  */
            dochan=ZStrSTCons(2);
            j=50;
            utf= query->_texts[queryterm];
/* TBD protect against query string greater than 40?  */
            while(1)
            {
                unicode=GetUnicode(&utf);
                word1[j++]=ZStrXlate(&zcutf,unicode);
                if(unicode==0) break;
                if(j>40+50) break;
            }
            word1[j] = 0;
            if (query->_localOptions[queryterm] == FTS_MATCH_PREFIX)
            {
                kk2=FindKKey2(ix,word1+50);
                if(kk2==NOTFOUND) {
                  break;
                }
/*  call routine to recursively put handles to STEX  */
                Ix2Recurs(dochan,ix,kk2);
            }
            if (query->_localOptions[queryterm] == FTS_MATCH_SUBSTRING)
            {
                kk1=FindKKey1(ix,word1+50);
                if(kk1==NOTFOUND) break;
/*  call routine to recursively put handles to STEX  */
                Ix1Recurs(dochan,ix,kk1,word1+50);
            }
            ZStrSTSort(dochan);
            odocs=dochan->cnt;
            docpt=dochan->list;
            ZStrCxClear(&zcdoc, &ctxa2);
            ZStrClear(zstra2);
            lasthan=0;
            ndocs=0;
            if(queryterm==0)
            {
                for(i=0;i<odocs;i++)
                {
                    ZStrInsert(zstr,docpt,2);
                    newhan=ZStrDec(zstr,&zcdh);
                    docpt+=ZStrExtLen(docpt,2);
                    if(ix->_handsfree[newhan]==0)
                    {
                        ZStrCxEnc(zstra2,&zcdoc,&ctxa2,newhan);
                        lasthan=newhan;
                        ndocs++;
                    }
                }
            }
            else
            {
/*  merge prefix stex with zstra1  */
                ZStrCxClear(&zcdoc, &ctxa1);
                ohand1=0;
                if(odocs==0) continue;
                nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                ZStrInsert(zstr,docpt,2);
                newhan=ZStrDec(zstr,&zcdh);
                docpt+=ZStrExtLen(docpt,2);
                odocs--;
/*     zstra1 = zstra1 & zstra2  */
                while(1)
                {
                    if(nhand1==ohand1) break;
                    if(newhan==nhand1)
                    {
                        if(ix->_handsfree[newhan]==0)
                        {
                            ZStrCxEnc(zstra2,&zcdoc,&ctxa2,newhan);
                            lasthan=newhan;
                            ndocs++;
                        }
                        if(odocs==0) break;
                        ZStrInsert(zstr,docpt,2);
                        newhan=ZStrDec(zstr,&zcdh);
                        docpt+=ZStrExtLen(docpt,2);
                        odocs--;
                        ohand1=nhand1;
                        nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                    }
                    else if(newhan>nhand1)
                    {
                        ohand1=nhand1;
                        nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                    }
                    else
                    {
                        if(odocs==0) break;
                        ZStrInsert(zstr,docpt,2);
                        newhan=ZStrDec(zstr,&zcdh);
                        docpt+=ZStrExtLen(docpt,2);
                        odocs--;
                    }
                }
            }
            ZStrCxEnc(zstra2,&zcdoc,&ctxa2,lasthan);
            ZStrNormalize(zstra2);
            ztemp=zstra1;
            zstra1=zstra2;
            zstra2=ztemp; 
            ZStrSTDest(dochan); 
        }   /* end of match-prefix code  */
    }

    if (ndocs != 0) {
      ZStrCxClear(&zcdoc, &ctxa1);

      dc->_docs = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, ndocs * sizeof(FTS_document_id_t), false);
      if (dc->_docs == NULL) {
        // out of memory
        TRI_ReadUnlockReadWriteLock(&ix->_lock);
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, dc);
    
        ZStrDest(zstra1);
        ZStrDest(zstra2);
        ZStrDest(zstr); 
        ZStrDest(zstr2); 
        ZStrDest(zstr3);  
        return NULL;
      }

      newhan=0;
      ndocs=0;
      while (1) {
        oldhan=newhan;
        newhan=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
        if(newhan==oldhan) break;
        if(ix->_handsfree[newhan]==0)
          dc->_docs[ndocs++]=ix->_handles[newhan];
      }
      dc->_len=ndocs;
    }
    
    TRI_ReadUnlockReadWriteLock(&ix->_lock);

    ZStrDest(zstra1);
    ZStrDest(zstra2);
    ZStrDest(zstr); 
    ZStrDest(zstr2); 
    ZStrDest(zstr3);  

    return dc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free results of a search
////////////////////////////////////////////////////////////////////////////////

void FTS_Free_Documents (FTS_document_ids_t* doclist) {
  if (doclist->_docs != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, doclist->_docs);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, doclist);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#if 0
int xxlet[100];
void index2dump(FTS_real_index * ix, uint64_t kkey, int lev)
{
    CTX ctx, dctx,x3ctx;
    ZSTR *zstr, *x3zstr;
    int i,temp,md;
    uint64_t x64,oldlet,newlet,bkey,newkkey;
    uint64_t docb,dock,han,oldhan;
    zstr=ZStrCons(30);
    x3zstr=ZStrCons(35);
    ZStrCxClear(&zcutf,&ctx);
    ZStrCxClear(&zcdelt,&dctx);
    ZStrCxClear(&zcdoc,&x3ctx);
    for(i=1;i<lev;i++) printf(" %c",xxlet[i]);
    i=ZStrTuberRead(ix->_index2,kkey,zstr);
    temp=kkey;
    if(i!=0)
    {
        printf("cannot read kkey = %d from TUBER\n",temp);
        return;
    }
    md=ZStrBitsOut(zstr,1);
    temp=kkey;
    printf("...kkey %d ",temp);
    temp=md;
    printf("Md=%d ",temp);
    temp=zstr->dat[0];
    printf(" zstr %x",temp);
    if(md==1)
    {
        docb=ZStrCxDec(zstr,&zcbky,&ctx);
        temp=docb;
        printf(" doc-b = %d",temp);
        dock=ZStrTuberK(ix->_index3,kkey,0,docb);
        temp=dock;
        printf(" doc-k = %d",temp);
    }
    oldlet=0;

    while(1)
    {
        newlet=ZStrCxDec(zstr,&zcdelt,&dctx);
        if(newlet==oldlet) break;
        bkey=ZStrCxDec(zstr,&zcbky,&ctx);
        x64=ZStrUnXl(&zcutf,newlet);
        temp=x64;
        if(temp<128)
            printf(" %c",temp);
        else
            printf(" %x",temp);
        temp=bkey;
        printf(" %d",temp);
        oldlet=newlet;
    }
    if(md==1)
    {
        printf("\n --- Docs ---");
        i=ZStrTuberRead(ix->_index3,dock,x3zstr);
        oldhan=0;
        while(1)
        {
            han=ZStrCxDec(x3zstr,&zcdoc,&x3ctx);
            if(han==oldhan) break;
            temp=han;
            printf("h= %d ",temp);
            temp=ix->_handles[han];
            printf("id= %d; ",temp);
            oldhan=han;
        }
    }
    printf("\n");
    i=ZStrTuberRead(ix->_index2,kkey,zstr);
    x64=ZStrBitsOut(zstr,1);
    if(x64==1)
        bkey=ZStrCxDec(zstr,&zcbky,&ctx);
    oldlet=0;
    ZStrCxClear(&zcdelt,&dctx);
    while(1)
    {
        newlet=ZStrCxDec(zstr,&zcdelt,&dctx);
        if(newlet==oldlet) return;
        bkey=ZStrCxDec(zstr,&zcbky,&ctx);
        newkkey=ZStrTuberK(ix->_index2,kkey,newlet,bkey);
        xxlet[lev]=ZStrUnXl(&zcutf,newlet);
        index2dump(ix,newkkey,lev+1);
        oldlet=newlet;
    }
}

void indexd(FTS_index_t * ftx)
{
    FTS_real_index * ix;
    int i;
    uint64_t kroot;
int temp;
    ix = (FTS_real_index *)ftx;
    printf("\n\nDump of Index\n");
temp=ix->_firstfree;
    printf("Free-chain starts at handle %d\n",temp);
    printf("======= First ten handles======\n");
    for(i=1;i<11;i++)
    {
temp=ix->_handles[i];
        printf("Handle %d is docid %d\n", i,temp);
    }
    printf("======= Index 2 ===============\n");
    kroot=ZStrTuberK(ix->_index2,0,0,0);
    index2dump(ix,kroot,1);
}
#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End: