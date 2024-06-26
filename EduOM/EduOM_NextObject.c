/******************************************************************************/
/*                                                                            */
/*    ODYSSEUS/EduCOSMOS Educational-Purpose Object Storage System            */
/*                                                                            */
/*    Developed by Professor Kyu-Young Whang et al.                           */
/*                                                                            */
/*    Database and Multimedia Laboratory                                      */
/*                                                                            */
/*    Computer Science Department and                                         */
/*    Advanced Information Technology Research Center (AITrc)                 */
/*    Korea Advanced Institute of Science and Technology (KAIST)              */
/*                                                                            */
/*    e-mail: kywhang@cs.kaist.ac.kr                                          */
/*    phone: +82-42-350-7722                                                  */
/*    fax: +82-42-350-8380                                                    */
/*                                                                            */
/*    Copyright (c) 1995-2013 by Kyu-Young Whang                              */
/*                                                                            */
/*    All rights reserved. No part of this software may be reproduced,        */
/*    stored in a retrieval system, or transmitted, in any form or by any     */
/*    means, electronic, mechanical, photocopying, recording, or otherwise,   */
/*    without prior written permission of the copyright owner.                */
/*                                                                            */
/******************************************************************************/
/*
 * Module: EduOM_NextObject.c
 *
 * Description:
 *  Return the next Object of the given Current Object. 
 *
 * Export:
 *  Four EduOM_NextObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 */


#include "EduOM_common.h"
#include "BfM.h"
#include "EduOM_Internal.h"

/*@================================
 * EduOM_NextObject()
 *================================*/
/*
 * Function: Four EduOM_NextObject(ObjectID*, ObjectID*, ObjectID*, ObjectHdr*)
 *
 * Description:
 * (Following description is for original ODYSSEUS/COSMOS OM.
 *  For ODYSSEUS/EduCOSMOS EduOM, refer to the EduOM project manual.)
 *
 *  Return the next Object of the given Current Object.  Find the Object in the
 *  same page which has the current Object and  if there  is no next Object in
 *  the same page, find it from the next page. If the Current Object is NULL,
 *  return the first Object of the file.
 *
 * Returns:
 *  error code
 *    eBADCATALOGOBJECT_OM
 *    eBADOBJECTID_OM
 *    some errors caused by function calls
 *
 * Side effect:
 *  1) parameter nextOID
 *     nextOID is filled with the next object's identifier
 *  2) parameter objHdr
 *     objHdr is filled with the next object's header
 */
Four EduOM_NextObject(
    ObjectID  *catObjForFile,	/* IN informations about a data file */
    ObjectID  *curOID,		/* IN a ObjectID of the current Object */
    ObjectID  *nextOID,		/* OUT the next Object of a current Object */
    ObjectHdr *objHdr)		/* OUT the object header of next object */
{
    Four e;			/* error */
    Two  i;			/* index */
    Four offset;		/* starting offset of object within a page */
    PageID pid;			/* a page identifier */
    PageNo pageNo;		/* a temporary var for next page's PageNo */
    SlottedPage *apage;		/* a pointer to the data page */
    Object *obj;		/* a pointer to the Object */
    PhysicalFileID pFid;	/* file in which the objects are located */
    SlottedPage *catPage;	/* buffer page containing the catalog object */
    sm_CatOverlayForData *catEntry; /* data structure for catalog object access */



    /*@
     * parameter checking
     */
    if (catObjForFile == NULL) ERR(eBADCATALOGOBJECT_OM);
    
    if (nextOID == NULL) ERR(eBADOBJECTID_OM);

    //카탈로그 가져옴
    e = BfM_GetTrain((TrainID*)catObjForFile, (char**)&catPage, PAGE_BUF);
    if( e < 0 ) ERR( e );
    GET_PTR_TO_CATENTRY_FOR_DATA(catObjForFile, catPage, catEntry);
    MAKE_PHYSICALFILEID(pFid, catEntry->fid.volNo, catEntry->firstPage);
    Boolean findflag = 0;

    // If curOID given as a parameter is NULL, 
    if(curOID ==NULL){
        pageNo = catEntry->firstPage;
        while(pageNo> 0){
            MAKE_PAGEID(pid, pFid.volNo, pageNo);
            e = BfM_GetTrain((TrainID*)&pid, (char**)&apage, PAGE_BUF);
            if(e<0) ERR(e);
            for (i=0; apage->header.nSlots; i++){
                offset = apage->slot[-i].offset;

                if (offset!=EMPTYSLOT){
                    findflag = 1;
                    obj = (Object*)&(apage->data[offset]);
                    MAKE_OBJECTID(*nextOID, pFid.volNo, pageNo, i, apage->slot[-i].unique);
                    objHdr = &obj->header;
                    break;
                }
            }
            // 찾음
            if(findflag){
                BfM_FreeTrain((TrainID*)&pid, PAGE_BUF);
                break;
            }

            // 못찾음
            if(!findflag){
                // 방금 페이지가 마지막 페이지였다면 break
                if(pageNo == catEntry->lastPage){
                    BfM_FreeTrain((TrainID*)&pid, PAGE_BUF);
                    break;
                }
                else{
                    pageNo = apage->header.nextPage;
                    BfM_FreeTrain((TrainID*)&pid, PAGE_BUF);
                }
            }
        }
    }

    else{
        MAKE_PAGEID(pid, curOID->volNo, curOID->pageNo);
        e = BfM_GetTrain((TrainID*)&pid, (char**)&apage, PAGE_BUF);
        if (e<0) ERR(e);
        //일단 현재 curobj 있는 페이지에 다음 오브젝트 있는지 찾는다.
        if (curOID->slotNo +1 <apage->header.nSlots){
            for(i = curOID->slotNo+1; i < apage->header.nSlots; i++)
            {
                offset = apage->slot[-i].offset;
                if(offset!=EMPTYSLOT)
                {
                    findflag = 1;
                    obj = (Object*)&(apage->data[offset]);
                    MAKE_OBJECTID(*nextOID, curOID->volNo, curOID->pageNo, i, apage->slot[-i].unique);
                    objHdr = &obj->header;
                    break;
                        
                }
            }
        }
        
        pageNo = apage->header.nextPage;
        e = BfM_FreeTrain((TrainID*)&pid, PAGE_BUF);
        if (e<0) ERR(e);

        // 현재페이지에 없다. 그럼 찾을때까지 nextpage
        if (!findflag){
            
            while(pageNo>0)
            {
                // pageNo에 해당하는 페이지 찾고
                MAKE_PAGEID(pid, pFid.volNo, pageNo);
                e = BfM_GetTrain((TrainID *)&pid, (char**)&apage, PAGE_BUF);
                if (e<0) ERR(e);

                // 해당 페이지 slot 다 검사
                for(i = 0; i<apage->header.nSlots; i++){
                    offset = apage->slot[-i].offset;

                    if(offset!=EMPTYSLOT)
                    {
                        findflag = 1;
                        obj = (Object*)&(apage->data[offset]);
                        MAKE_OBJECTID(*nextOID, pFid.volNo, pageNo, i, apage->slot[-i].unique);
                        objHdr = &obj->header;
                        break;
                
                    }
                }

                // next object 찾았다.
                if(findflag){
                    BfM_FreeTrain((TrainID*)&pid, PAGE_BUF);
                    break;
                }
                // next obj 못찾았다. 
                if(!findflag)
                {   
                    // 방금 페이지가 파일의 마지막 페이지였다면
                    if(pageNo == catEntry->lastPage)
                    {
                        BfM_FreeTrain((TrainID*)&pid, PAGE_BUF); 
                        break;
                    }
                    else
                    {
                        BfM_FreeTrain((TrainID*)&pid, PAGE_BUF);
                        pageNo = apage->header.nextPage;
                    } 
                } 

            }
        }



    }
    
    e = BfM_FreeTrain((TrainID*)catObjForFile, PAGE_BUF);

    return(EOS);		/* end of scan */
    
} /* EduOM_NextObject() */
