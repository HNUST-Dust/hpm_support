/*
* This source file is part of the EtherCAT Slave Stack Code licensed by Beckhoff Automation GmbH & Co KG, 33415 Verl, Germany.
* The corresponding license agreement applies. This hint shall not be removed.
*/

/**
* \addtogroup digital_io digital_io
* @{
*/

/**
\file digital_ioObjects
\author ET9300Utilities.ApplicationHandler (Version 1.6.3.0) | EthercatSSC@beckhoff.com

\brief digital_io specific objects<br>
\brief NOTE : This file will be overwritten if a new object dictionary is generated!<br>
*/

#if defined(_DIGITAL_IO_) && (_DIGITAL_IO_ == 1)
#define PROTO
#else
#define PROTO extern
#endif

/* process data length: ECAT_PD_WORD_CNT * 4 bytes for each direction */
#define ECAT_PD_WORD_CNT 8
/******************************************************************************
*                    Object 0x1600 : OutputCounter process data mapping
******************************************************************************/
/**
* \addtogroup 0x1600 0x1600 | OutputCounter process data mapping
* @{
* \brief Object 0x1600 (OutputCounter process data mapping) definition
*/
#ifdef _OBJD_
/**
* \brief Object entry descriptions<br>
* <br>
* SubIndex 0<br>
* SubIndex 1-N - Reference to 0x7010.1-N (one description per entry,
* USE_SINGLE_PDO_MAPPING_ENTRY_DESCR is 0)
*/
OBJCONST TSDOINFOENTRYDESC    OBJMEM asEntryDesc0x1600[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ }}; /* Subindex1-8 - Reference to 0x7010.1-8 */

/**
* \brief Object/Entry names
*/
OBJCONST UCHAR OBJMEM aName0x1600[] = "OutputCounter process data mapping\000"
"SubIndex 001\000\377";
#endif //#ifdef _OBJD_

#ifndef _DIGITAL_IO_OBJECTS_H_
/**
* \brief Object structure
*/
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT32 aEntries[ECAT_PD_WORD_CNT]; /* Subindex1-N - Reference to 0x7010.1-N */
} OBJ_STRUCT_PACKED_END
TOBJ1600;
#endif //#ifndef _DIGITAL_IO_OBJECTS_H_

/**
* \brief Object variable
*/
PROTO TOBJ1600 OutputCounterProcessDataMapping0x1600
#if defined(_DIGITAL_IO_) && (_DIGITAL_IO_ == 1)
={ECAT_PD_WORD_CNT,{0x70100120,0x70100220,0x70100320,0x70100420,0x70100520,0x70100620,0x70100720,0x70100820}}
#endif
;
/** @}*/



/******************************************************************************
*                    Object 0x1A00 : InputCounter process data mapping
******************************************************************************/
/**
* \addtogroup 0x1A00 0x1A00 | InputCounter process data mapping
* @{
* \brief Object 0x1A00 (InputCounter process data mapping) definition
*/
#ifdef _OBJD_
/**
* \brief Object entry descriptions<br>
* <br>
* SubIndex 0<br>
* SubIndex 1-N - Reference to 0x6000.1-N (one description per entry,
* USE_SINGLE_PDO_MAPPING_ENTRY_DESCR is 0)
*/
OBJCONST TSDOINFOENTRYDESC    OBJMEM asEntryDesc0x1A00[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ }}; /* Subindex1-8 - Reference to 0x6000.1-8 */

/**
* \brief Object/Entry names
*/
OBJCONST UCHAR OBJMEM aName0x1A00[] = "InputCounter process data mapping\000"
"SubIndex 001\000\377";
#endif //#ifdef _OBJD_

#ifndef _DIGITAL_IO_OBJECTS_H_
/**
* \brief Object structure
*/
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT32 aEntries[ECAT_PD_WORD_CNT]; /* Subindex1-N - Reference to 0x6000.1-N */
} OBJ_STRUCT_PACKED_END
TOBJ1A00;
#endif //#ifndef _DIGITAL_IO_OBJECTS_H_

/**
* \brief Object variable
*/
PROTO TOBJ1A00 InputCounterProcessDataMapping0x1A00
#if defined(_DIGITAL_IO_) && (_DIGITAL_IO_ == 1)
={ECAT_PD_WORD_CNT,{0x60000120,0x60000220,0x60000320,0x60000420,0x60000520,0x60000620,0x60000720,0x60000820}}
#endif
;
/** @}*/



/******************************************************************************
*                    Object 0x1C12 : SyncManager 2 assignment
******************************************************************************/
/**
* \addtogroup 0x1C12 0x1C12 | SyncManager 2 assignment
* @{
* \brief Object 0x1C12 (SyncManager 2 assignment) definition
*/
#ifdef _OBJD_
/**
* \brief Entry descriptions<br>
* 
* Subindex 0<br>
* Subindex 1 - n (the same entry description is used)<br>
*/
OBJCONST TSDOINFOENTRYDESC    OBJMEM asEntryDesc0x1C12[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ }};

/**
* \brief Object name definition<br>
* For Subindex 1 to n the syntax 'Subindex XXX' is used
*/
OBJCONST UCHAR OBJMEM aName0x1C12[] = "SyncManager 2 assignment\000\377";
#endif //#ifdef _OBJD_

#ifndef _DIGITAL_IO_OBJECTS_H_
/**
* \brief Object structure
*/
typedef struct OBJ_STRUCT_PACKED_START {
UINT16   u16SubIndex0;  /**< \brief Subindex 0 */
UINT16 aEntries[1];  /**< \brief Subindex 1 - 1 */
} OBJ_STRUCT_PACKED_END
TOBJ1C12;
#endif //#ifndef _DIGITAL_IO_OBJECTS_H_

/**
* \brief Object variable
*/
PROTO TOBJ1C12 sRxPDOassign
#if defined(_DIGITAL_IO_) && (_DIGITAL_IO_ == 1)
={1,{0x1600}}
#endif
;
/** @}*/



/******************************************************************************
*                    Object 0x1C13 : SyncManager 3 assignment
******************************************************************************/
/**
* \addtogroup 0x1C13 0x1C13 | SyncManager 3 assignment
* @{
* \brief Object 0x1C13 (SyncManager 3 assignment) definition
*/
#ifdef _OBJD_
/**
* \brief Entry descriptions<br>
* 
* Subindex 0<br>
* Subindex 1 - n (the same entry description is used)<br>
*/
OBJCONST TSDOINFOENTRYDESC    OBJMEM asEntryDesc0x1C13[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ }};

/**
* \brief Object name definition<br>
* For Subindex 1 to n the syntax 'Subindex XXX' is used
*/
OBJCONST UCHAR OBJMEM aName0x1C13[] = "SyncManager 3 assignment\000\377";
#endif //#ifdef _OBJD_

#ifndef _DIGITAL_IO_OBJECTS_H_
/**
* \brief Object structure
*/
typedef struct OBJ_STRUCT_PACKED_START {
UINT16   u16SubIndex0;  /**< \brief Subindex 0 */
UINT16 aEntries[1];  /**< \brief Subindex 1 - 1 */
} OBJ_STRUCT_PACKED_END
TOBJ1C13;
#endif //#ifndef _DIGITAL_IO_OBJECTS_H_

/**
* \brief Object variable
*/
PROTO TOBJ1C13 sTxPDOassign
#if defined(_DIGITAL_IO_) && (_DIGITAL_IO_ == 1)
={1,{0x1A00}}
#endif
;
/** @}*/



/******************************************************************************
*                    Object 0x6000 : InputCounter
******************************************************************************/
/**
* \addtogroup 0x6000 0x6000 | InputCounter
* @{
* \brief Object 0x6000 (InputCounter) definition
*/
#ifdef _OBJD_
/**
* \brief Entry descriptions (Subindex 1 - N use the same description)
*/
OBJCONST TSDOINFOENTRYDESC    OBJMEM asEntryDesc0x6000[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READ | OBJACCESS_TXPDOMAPPING }};
/**
* \brief Object name
*/
OBJCONST UCHAR OBJMEM aName0x6000[] = "InputCounter\000\377";
#endif //#ifdef _OBJD_

#ifndef _DIGITAL_IO_OBJECTS_H_
/**
* \brief Object structure
*/
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT32 aEntries[ECAT_PD_WORD_CNT];
} OBJ_STRUCT_PACKED_END
TOBJ6000;
#endif //#ifndef _DIGITAL_IO_OBJECTS_H_

/**
* \brief Object variable
*/
PROTO TOBJ6000 InputCounter0x6000
#if defined(_DIGITAL_IO_) && (_DIGITAL_IO_ == 1)
={ECAT_PD_WORD_CNT,{0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000}}
#endif
;
/** @}*/



/******************************************************************************
*                    Object 0x7010 : OutputCounter
******************************************************************************/
/**
* \addtogroup 0x7010 0x7010 | OutputCounter
* @{
* \brief Object 0x7010 (OutputCounter) definition
*/
#ifdef _OBJD_
/**
* \brief Entry descriptions (Subindex 1 - N use the same description)
*/
OBJCONST TSDOINFOENTRYDESC    OBJMEM asEntryDesc0x7010[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READWRITE | OBJACCESS_RXPDOMAPPING }};
/**
* \brief Object name
*/
OBJCONST UCHAR OBJMEM aName0x7010[] = "OutputCounter\000\377";
#endif //#ifdef _OBJD_

#ifndef _DIGITAL_IO_OBJECTS_H_
/**
* \brief Object structure
*/
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT32 aEntries[ECAT_PD_WORD_CNT];
} OBJ_STRUCT_PACKED_END
TOBJ7010;
#endif //#ifndef _DIGITAL_IO_OBJECTS_H_

/**
* \brief Object variable
*/
PROTO TOBJ7010 OutputCounter0x7010
#if defined(_DIGITAL_IO_) && (_DIGITAL_IO_ == 1)
={ECAT_PD_WORD_CNT,{0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000}}
#endif
;
/** @}*/



/******************************************************************************
*                    Object 0x2000 : MyMbxData
******************************************************************************/
/**
* \addtogroup 0x2000 0x2000 | MyMbxData
* @{
* \brief Object 0x2000 (MyMbxData) definition, mailbox data exchange with master via SDO
*/
#ifdef _OBJD_
/**
* \brief Entry description
*/
OBJCONST TSDOINFOENTRYDESC    OBJMEM sEntryDesc0x2000 = { DEFTYPE_UNSIGNED32 , 0x20 , ACCESS_READWRITE };
/**
* \brief Object name
*/
OBJCONST UCHAR OBJMEM aName0x2000[] = "MyMbxData\000\377";
#endif //#ifdef _OBJD_

/**
* \brief Object variable
*/
PROTO UINT32 MyMbxData0x2000
#if defined(_DIGITAL_IO_) && (_DIGITAL_IO_ == 1)
= 0x00000000
#endif
;
/** @} */


/******************************************************************************
*                    Object 0xF000 : Modular Device Profile
******************************************************************************/
/**
* \addtogroup 0xF000 0xF000 | Modular Device Profile
* @{
* \brief Object 0xF000 (Modular Device Profile) definition
*/
#ifdef _OBJD_
/**
* \brief Object entry descriptions<br>
* <br>
* SubIndex 0<br>
* SubIndex 1 - Index distance <br>
* SubIndex 2 - Maximum number of modules <br>
*/
OBJCONST TSDOINFOENTRYDESC    OBJMEM asEntryDesc0xF000[] = {
{ DEFTYPE_UNSIGNED8 , 0x8 , ACCESS_READ },
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ }, /* Subindex1 - Index distance  */
{ DEFTYPE_UNSIGNED16 , 0x10 , ACCESS_READ }}; /* Subindex2 - Maximum number of modules  */

/**
* \brief Object/Entry names
*/
OBJCONST UCHAR OBJMEM aName0xF000[] = "Modular Device Profile\000"
"Index distance \000"
"Maximum number of modules \000\377";
#endif //#ifdef _OBJD_

#ifndef _DIGITAL_IO_OBJECTS_H_
/**
* \brief Object structure
*/
typedef struct OBJ_STRUCT_PACKED_START {
UINT16 u16SubIndex0;
UINT16 IndexDistance; /* Subindex1 - Index distance  */
UINT16 MaximumNumberOfModules; /* Subindex2 - Maximum number of modules  */
} OBJ_STRUCT_PACKED_END
TOBJF000;
#endif //#ifndef _DIGITAL_IO_OBJECTS_H_

/**
* \brief Object variable
*/
PROTO TOBJF000 ModularDeviceProfile0xF000
#if defined(_DIGITAL_IO_) && (_DIGITAL_IO_ == 1)
={2,0x0010,0}
#endif
;
/** @}*/







#ifdef _OBJD_
TOBJECT    OBJMEM ApplicationObjDic[] = {
/* Object 0x1600 */
{NULL , NULL ,  0x1600 , {DEFTYPE_PDOMAPPING , ECAT_PD_WORD_CNT | (OBJCODE_REC << 8)} , asEntryDesc0x1600 , aName0x1600 , &OutputCounterProcessDataMapping0x1600 , NULL , NULL , 0x0000 },
/* Object 0x1A00 */
{NULL , NULL ,  0x1A00 , {DEFTYPE_PDOMAPPING , ECAT_PD_WORD_CNT | (OBJCODE_REC << 8)} , asEntryDesc0x1A00 , aName0x1A00 , &InputCounterProcessDataMapping0x1A00 , NULL , NULL , 0x0000 },
/* Object 0x1C12 */
{NULL , NULL ,  0x1C12 , {DEFTYPE_UNSIGNED16 , 1 | (OBJCODE_ARR << 8)} , asEntryDesc0x1C12 , aName0x1C12 , &sRxPDOassign , NULL , NULL , 0x0000 },
/* Object 0x1C13 */
{NULL , NULL ,  0x1C13 , {DEFTYPE_UNSIGNED16 , 1 | (OBJCODE_ARR << 8)} , asEntryDesc0x1C13 , aName0x1C13 , &sTxPDOassign , NULL , NULL , 0x0000 },
/* Object 0x2000 */
{NULL , NULL ,  0x2000 , {DEFTYPE_UNSIGNED32 , 0 | (OBJCODE_VAR << 8)} , &sEntryDesc0x2000 , aName0x2000 , &MyMbxData0x2000 , NULL , NULL , 0x0000 },
/* Object 0x6000 */
{NULL , NULL ,  0x6000 , {DEFTYPE_UNSIGNED32 , ECAT_PD_WORD_CNT | (OBJCODE_ARR << 8)} , asEntryDesc0x6000 , aName0x6000 , &InputCounter0x6000 , NULL , NULL , 0x0000 },
/* Object 0x7010 */
{NULL , NULL ,  0x7010 , {DEFTYPE_UNSIGNED32 , ECAT_PD_WORD_CNT | (OBJCODE_ARR << 8)} , asEntryDesc0x7010 , aName0x7010 , &OutputCounter0x7010 , NULL , NULL , 0x0000 },
/* Object 0xF000 */
{NULL , NULL ,  0xF000 , {DEFTYPE_RECORD , 2 | (OBJCODE_REC << 8)} , asEntryDesc0xF000 , aName0xF000 , &ModularDeviceProfile0xF000 , NULL , NULL , 0x0000 },
{NULL,NULL, 0xFFFF, {0, 0}, NULL, NULL, NULL, NULL}};
#endif    //#ifdef _OBJD_

#undef PROTO

/** @}*/
#define _DIGITAL_IO_OBJECTS_H_
