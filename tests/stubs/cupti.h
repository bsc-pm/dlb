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

// This file contains portions derived from CUDA library.
// Original copyright:

/*
 * Copyright 2011-2025 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to NVIDIA intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and
 * conditions of a form of NVIDIA software license agreement by and
 * between NVIDIA and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of NVIDIA is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, NVIDIA MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
 * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
 * NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
 * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY
 * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THESE LICENSED DELIVERABLES.
 *
 * U.S. Government End Users.  These Licensed Deliverables are a
 * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
 * 1995), consisting of "commercial computer software" and "commercial
 * computer software documentation" as such terms are used in 48
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */

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
    CUPTI_CB_DOMAIN_DRIVER_API        = 1,
    CUPTI_CB_DOMAIN_RUNTIME_API       = 2,
} CUpti_CallbackDomain;

typedef enum {
    CUPTI_API_ENTER                 = 0,
    CUPTI_API_EXIT                  = 1,
    CUPTI_API_CBSITE_FORCE_INT      = 0x7fffffff,
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
    uint32_t deviceId;
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
    uint32_t deviceId;
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
