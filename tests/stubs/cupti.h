/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#ifndef CUPTI_H
#define CUPTI_H

#include <stddef.h>
#include <stdint.h>

#define CUPTIAPI
#define CUPTI_API_VERSION 21

typedef enum {
    CUPTI_SUCCESS = 0,
    CUPTI_ERROR_MAX_LIMIT_REACHED,
} CUptiResult;

typedef enum {
    CUPTI_ACTIVITY_KIND_INVALID  = 0,
    CUPTI_ACTIVITY_KIND_MEMCPY   = 1,
    CUPTI_ACTIVITY_KIND_MEMSET   = 2,
    CUPTI_ACTIVITY_KIND_KERNEL   = 3,
    CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL = 10,
    CUPTI_ACTIVITY_KIND_MEMCPY2 = 22,
    CUPTI_ACTIVITY_KIND_MEMORY2 = 49,
} CUpti_ActivityKind;

typedef enum {
    CUPTI_CB_DOMAIN_INVALID           = 0,
    CUPTI_CB_DOMAIN_RUNTIME_API       = 2,
} CUpti_CallbackDomain;

typedef enum {
    CUPTI_API_ENTER                 = 0,
    CUPTI_API_EXIT                  = 1,
    CUPTI_API_CBSITE_FORCE_INT     = 0x7fffffff
} CUpti_ApiCallbackSite;

typedef struct CUpti_SubscriberHandle {
    char _unused;
} CUpti_SubscriberHandle;

typedef struct CUctx_st {
    char _unused;
} CUcontext;


typedef struct {
    CUpti_ApiCallbackSite callbackSite;
    const char *functionName;
} CUpti_CallbackData;

typedef uint32_t CUpti_CallbackId;

typedef struct {
    CUpti_ActivityKind kind;
    uint64_t start;
    uint64_t end;
} CUpti_Activity;
typedef CUpti_Activity CUpti_ActivityKernel8;
typedef CUpti_Activity CUpti_ActivityKernel8;
typedef CUpti_Activity CUpti_ActivityKernel9;
typedef CUpti_Activity CUpti_ActivityKernel10;
typedef CUpti_Activity CUpti_ActivityMemcpy5;
typedef CUpti_Activity CUpti_ActivityMemcpy6;
typedef CUpti_Activity CUpti_ActivityMemset4;
typedef CUpti_Activity CUpti_ActivityMemcpyPtoP4;

typedef struct {
    uint64_t timestamp;
} CUpti_ActivityMemory;
typedef CUpti_ActivityMemory CUpti_ActivityMemory3;
typedef CUpti_ActivityMemory CUpti_ActivityMemory4;


typedef void (CUPTIAPI *CUpti_CallbackFunc)(
    void *userdata,
    CUpti_CallbackDomain domain,
    CUpti_CallbackId cbid,
    const void *cbdata);

typedef void (CUPTIAPI *CUpti_BuffersCallbackRequestFunc)(
    uint8_t **buffer,
    size_t *size,
    size_t *maxNumRecords);

typedef void (CUPTIAPI *CUpti_BuffersCallbackCompleteFunc)(
    CUcontext context,
    uint32_t streamId,
    uint8_t *buffer,
    size_t size,
    size_t validSize);


CUptiResult CUPTIAPI cuptiSubscribe(CUpti_SubscriberHandle *subscriber,
                                    CUpti_CallbackFunc callback,
                                    void *userdata);
CUptiResult CUPTIAPI cuptiUnsubscribe(CUpti_SubscriberHandle subscriber);
CUptiResult CUPTIAPI cuptiFinalize(void);
CUptiResult CUPTIAPI cuptiEnableDomain(uint32_t enable,
                                       CUpti_SubscriberHandle subscriber,
                                       CUpti_CallbackDomain domain);
CUptiResult CUPTIAPI cuptiActivityEnable(CUpti_ActivityKind kind);
CUptiResult CUPTIAPI cuptiActivityDisable(CUpti_ActivityKind kind);
CUptiResult CUPTIAPI cuptiActivityGetNextRecord(uint8_t* buffer, size_t validBufferSizeBytes,
        CUpti_Activity **record);
CUptiResult CUPTIAPI cuptiActivityFlushAll(uint32_t flag);
CUptiResult CUPTIAPI cuptiActivityRegisterCallbacks(
        CUpti_BuffersCallbackRequestFunc  funcBufferRequested,
        CUpti_BuffersCallbackCompleteFunc funcBufferCompleted);
CUptiResult CUPTIAPI cuptiGetResultString(CUptiResult result, const char **str);

#endif /* CUPTI_H */
