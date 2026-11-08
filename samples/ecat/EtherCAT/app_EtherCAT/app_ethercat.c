/*
* This source file is part of the EtherCAT Slave Stack Code licensed by Beckhoff Automation GmbH & Co KG, 33415 Verl, Germany.
* The corresponding license agreement applies. This hint shall not be removed.
*/

/**
\addtogroup digital_io digital_io
@{
*/

/**
\file digital_io.c
\brief Implementation
 Created with SSC Tool application parser 1.6.4.0
\version 0.0.0.1
*/


/*-----------------------------------------------------------------------------------------
------
------    Includes
------
-----------------------------------------------------------------------------------------*/
#include "ecat_def.h"

#include "applInterface.h"

#define _DIGITAL_IO_ 1
#include "digital_io.h"
#undef _DIGITAL_IO_

#include "ecat_config.h"
#include <zephyr/sys/printk.h>


UINT32 APPL_GetDipSw(void);
void APPL_SetLed(UINT32 value);

/*--------------------------------------------------------------------------------------
------
------    local types and defines
------
--------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------
------
------    local variables and constants
------
-----------------------------------------------------------------------------------------*/

/* 回环统计:中断上下文里只做计数和连续性检查,不在 ISR 里逐帧打印 */
static struct {
    UINT32 frameCnt;    /* 处理的帧数 */
    UINT32 gapCnt;      /* 序列不连续次数 */
    UINT32 missedFrames;/* 按跳变推算丢失的帧数 */
    UINT32 firstWord;   /* 首帧首字 */
    UINT32 lastWord;    /* 末帧首字 */
    UINT32 expectNext;  /* 期望的下一帧首字 */
    UINT8  started;
} appl_sm2_stat;

/*-----------------------------------------------------------------------------------------
------
------    application specific functions
------
-----------------------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------------------
------
------    generic functions
------
-----------------------------------------------------------------------------------------*/


/**
 \brief    The function is called when an error state was acknowledged by the master
*/

void    APPL_AckErrorInd(UINT16 stateTrans)
{

}


/**
 \return    AL Status Code (see ecatslv.h ALSTATUSCODE_....)

 \brief    The function is called in the state transition from INIT to PREOP when
             all general settings were checked to start the mailbox handler. This function
             informs the application about the state transition, the application can refuse
             the state transition when returning an AL Status error code.
            The return code NOERROR_INWORK can be used, if the application cannot confirm
            the state transition immediately, in that case this function will be called cyclically
            until a value unequal NOERROR_INWORK is returned

*/

UINT16 APPL_StartMailboxHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/**
 \return     0, NOERROR_INWORK

 \brief    The function is called in the state transition from PREEOP to INIT
             to stop the mailbox handler. This functions informs the application
             about the state transition, the application cannot refuse
             the state transition.

*/

UINT16 APPL_StopMailboxHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/**
 \param    pIntMask    pointer to the AL Event Mask which will be written to the AL event Mask
                       register (0x204) when this function is succeeded. The event mask can be adapted
                       in this function
 \return    AL Status Code (see ecatslv.h ALSTATUSCODE_....)

 \brief    The function is called in the state transition from PREOP to SAFEOP when
           all general settings were checked to start the input handler. This function
           informs the application about the state transition, the application can refuse
           the state transition when returning an AL Status error code.
           The return code NOERROR_INWORK can be used, if the application cannot confirm
           the state transition immediately, in that case the application need to be complete
           the transition by calling ECAT_StateChange.
*/

UINT16 APPL_StartInputHandler(UINT16 *pIntMask)
{
    return ALSTATUSCODE_NOERROR;
}

/**
 \return     0, NOERROR_INWORK

 \brief    The function is called in the state transition from SAFEOP to PREEOP
             to stop the input handler. This functions informs the application
             about the state transition, the application cannot refuse
             the state transition.

*/

UINT16 APPL_StopInputHandler(void)
{
    return ALSTATUSCODE_NOERROR;
}

/**
 \return    AL Status Code (see ecatslv.h ALSTATUSCODE_....)

 \brief    The function is called in the state transition from SAFEOP to OP when
             all general settings were checked to start the output handler. This function
             informs the application about the state transition, the application can refuse
             the state transition when returning an AL Status error code.
           The return code NOERROR_INWORK can be used, if the application cannot confirm
           the state transition immediately, in that case the application need to be complete
           the transition by calling ECAT_StateChange.
*/

UINT16 APPL_StartOutputHandler(void)
{
    /* 重置回环统计,支持反复测试 */
    memset(&appl_sm2_stat, 0, sizeof(appl_sm2_stat));
    return ALSTATUSCODE_NOERROR;
}

/**
 \return     0, NOERROR_INWORK

 \brief    The function is called in the state transition from OP to SAFEOP
             to stop the output handler. This functions informs the application
             about the state transition, the application cannot refuse
             the state transition.

*/

UINT16 APPL_StopOutputHandler(void)
{
    /* 退出 OP 时汇总打印一次(逐帧 printk 在中断里太慢,会丢帧) */
    if (appl_sm2_stat.started) {
        printk("SM2 stat: frames=%u gaps=%u missed=%u first=0x%08x last=0x%08x\n",
               appl_sm2_stat.frameCnt, appl_sm2_stat.gapCnt, appl_sm2_stat.missedFrames,
               appl_sm2_stat.firstWord, appl_sm2_stat.lastWord);
    }
    return ALSTATUSCODE_NOERROR;
}

/**
\return     0(ALSTATUSCODE_NOERROR), NOERROR_INWORK
\param      pInputSize  pointer to save the input process data length
\param      pOutputSize  pointer to save the output process data length

\brief    This function calculates the process data sizes from the actual SM-PDO-Assign
            and PDO mapping
*/

UINT16 APPL_GenerateMapping(UINT16 *pInputSize, UINT16 *pOutputSize)
{
    UINT16 result = ALSTATUSCODE_NOERROR;
    UINT16 InputSize = 0;
    UINT16 OutputSize = 0;

#if COE_SUPPORTED
    UINT16 PDOAssignEntryCnt = 0;
    OBJCONST TOBJECT OBJMEM *pPDO = NULL;
    UINT16 PDOSubindex0 = 0;
    UINT32 *pPDOEntry = NULL;
    UINT16 PDOEntryCnt = 0;

#if MAX_PD_OUTPUT_SIZE > 0
    /*Scan object 0x1C12 RXPDO assign*/
    for (PDOAssignEntryCnt = 0; PDOAssignEntryCnt < sRxPDOassign.u16SubIndex0; PDOAssignEntryCnt++) {
        pPDO = OBJ_GetObjectHandle(sRxPDOassign.aEntries[PDOAssignEntryCnt]);
        if (pPDO != NULL) {
            PDOSubindex0 = *((UINT16 *)pPDO->pVarPtr);
            for (PDOEntryCnt = 0; PDOEntryCnt < PDOSubindex0; PDOEntryCnt++) {
                pPDOEntry = (UINT32 *)(((UINT16 *)pPDO->pVarPtr) + (OBJ_GetEntryOffset((PDOEntryCnt+1), pPDO)>>4));    /* goto PDO entry */
                /* we increment the expected output size depending on the mapped Entry */
                OutputSize += (UINT16) ((*pPDOEntry) & 0xFF);
            }
        } else {
            /*assigned PDO was not found in object dictionary. return invalid mapping*/
            OutputSize = 0;
            result = ALSTATUSCODE_INVALIDOUTPUTMAPPING;
            break;
        }
    }

    OutputSize = (OutputSize + 7) >> 3;
#endif

#if MAX_PD_INPUT_SIZE > 0
    if (result == 0) {
        /*Scan Object 0x1C13 TXPDO assign*/
        for (PDOAssignEntryCnt = 0; PDOAssignEntryCnt < sTxPDOassign.u16SubIndex0; PDOAssignEntryCnt++) {
            pPDO = OBJ_GetObjectHandle(sTxPDOassign.aEntries[PDOAssignEntryCnt]);
            if (pPDO != NULL) {
                PDOSubindex0 = *((UINT16 *)pPDO->pVarPtr);
                for (PDOEntryCnt = 0; PDOEntryCnt < PDOSubindex0; PDOEntryCnt++) {
                    pPDOEntry = (UINT32 *)(((UINT16 *)pPDO->pVarPtr) + (OBJ_GetEntryOffset((PDOEntryCnt+1), pPDO)>>4));    /* goto PDO entry */
                    /* we increment the expected output size depending on the mapped Entry */
                    InputSize += (UINT16) ((*pPDOEntry) & 0xFF);
                }
            } else {
                /*assigned PDO was not found in object dictionary. return invalid mapping*/
                InputSize = 0;
                result = ALSTATUSCODE_INVALIDINPUTMAPPING;
                break;
            }
        }
    }
    InputSize = (InputSize + 7) >> 3;
#endif

#else
#if _WIN32
    #pragma message ("Warning: Define 'InputSize' and 'OutputSize'.")
#else
    #warning "Define 'InputSize' and 'OutputSize'."
#endif
#endif

    *pInputSize = InputSize;
    *pOutputSize = OutputSize;
    return result;
}

/**
\param      pData  pointer to input process data

\brief      This function will copies the inputs from the local memory to the ESC memory
            to the hardware
*/
void APPL_InputMapping(UINT16 *pData)
{
    MEMCPY(pData, &InputCounter0x6000.aEntries[0], SIZEOF(InputCounter0x6000.aEntries));
}

/**
\param      pData  pointer to output process data

\brief    This function will copies the outputs from the ESC memory to the local memory
            to the hardware
*/
void APPL_OutputMapping(UINT16 *pData)
{
    MEMCPY(&OutputCounter0x7010.aEntries[0], pData, SIZEOF(OutputCounter0x7010.aEntries));
}

/**
\brief    This function will called from the synchronisation ISR
            or from the mainloop if no synchronisation is supported
*/
void APPL_Application(void)
{
    UINT32 rawSm2[ECAT_PD_WORD_CNT];
    /* 直读SM2原始缓冲区(主站写入的输出数据,地址来自栈读回的SM配置) */
    HW_EscRead(rawSm2, nEscAddrOutputData, SIZEOF(rawSm2));
    /* 回环:主站写什么就回什么,SOEM侧可验证 */
    MEMCPY(InputCounter0x6000.aEntries, rawSm2, SIZEOF(rawSm2));

    if (!appl_sm2_stat.started) {
        appl_sm2_stat.started = 1;
        appl_sm2_stat.firstWord = rawSm2[0];
    } else if (rawSm2[0] != appl_sm2_stat.expectNext) {
        appl_sm2_stat.gapCnt++;
        if (rawSm2[0] > appl_sm2_stat.expectNext) {
            appl_sm2_stat.missedFrames += (rawSm2[0] - appl_sm2_stat.expectNext) / ECAT_PD_WORD_CNT;
        }
    }
    appl_sm2_stat.expectNext = rawSm2[0] + ECAT_PD_WORD_CNT;
    appl_sm2_stat.lastWord = rawSm2[0];
    appl_sm2_stat.frameCnt++;
}

#if EXPLICIT_DEVICE_ID

/**
 \return    The Explicit Device ID of the EtherCAT slave

 \brief     Read the Explicit Device ID (from an external ID switch)
*/
UINT16 APPL_GetDeviceID(void)
{
    /* Explicit Device 5 is expected by Explicit Device ID conformance tests*/
    return 0x5;
}
#endif

void APPL_SetLed(UINT32 value)
{
    (void)value;
}

UINT32 APPL_GetDipSw(void)
{
    return 0;
}

#if USE_DEFAULT_MAIN

/**

 \brief    This is the main function

*/
#if _PIC24 && EL9800_HW
int main(void)
#elif _WIN32
int main(int argc, char *argv[])
#else
void main(void)
#endif
{
    /* initialize the Hardware and the EtherCAT Slave Controller */
#if FC1100_HW
#if _WIN32
    u16FcInstance = 0;
    if (argc > 1) {
        u16FcInstance = atoi(argv[1]);
    }
#endif
    if (HW_Init()) {
        HW_Release();
        return;
    }
#else
    HW_Init();
#endif
    MainInit();

    bRunApplication = TRUE;
    do {
        MainLoop();
    } while (bRunApplication == TRUE);

    HW_Release();
#if _PIC24
    return 0;
#endif
}
#endif /* #if USE_DEFAULT_MAIN */
/** @} */
