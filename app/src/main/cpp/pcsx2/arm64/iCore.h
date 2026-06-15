// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "VixlHelpers.h"
#include "VUmicro.h"

// ARM64 Register Allocation Tools

#define RALOG(...)

////////////////////////////////////////////////////////////////////////////////
// Shared Register allocation flags (apply to X86, XMM, MMX, etc).

#define MODE_READ        1
#define MODE_WRITE       2
#define MODE_CALLEESAVED  0x20 // can't flush reg to mem
#define MODE_COP2 0x40 // don't allow using reserved VU registers

#define PROCESS_EE_XMM 0x02

#define PROCESS_EE_S 0x04 // S is valid, otherwise take from mem
#define PROCESS_EE_T 0x08 // T is valid, otherwise take from mem
#define PROCESS_EE_D 0x10 // D is valid, otherwise take from mem

#define PROCESS_EE_LO         0x40 // lo reg is valid
#define PROCESS_EE_HI         0x80 // hi reg is valid
#define PROCESS_EE_ACC        0x40 // acc reg is valid

#define EEREC_S    (((info) >>  8) & 0xf)
#define EEREC_T    (((info) >> 12) & 0xf)
#define EEREC_D    (((info) >> 16) & 0xf)
#define EEREC_LO   (((info) >> 20) & 0xf)
#define EEREC_HI   (((info) >> 24) & 0xf)
#define EEREC_ACC  (((info) >> 20) & 0xf)

#define PROCESS_EE_SET_S(reg)   (((reg) <<  8) | PROCESS_EE_S)
#define PROCESS_EE_SET_T(reg)   (((reg) << 12) | PROCESS_EE_T)
#define PROCESS_EE_SET_D(reg)   (((reg) << 16) | PROCESS_EE_D)
#define PROCESS_EE_SET_LO(reg)  (((reg) << 20) | PROCESS_EE_LO)
#define PROCESS_EE_SET_HI(reg)  (((reg) << 24) | PROCESS_EE_HI)
#define PROCESS_EE_SET_ACC(reg) (((reg) << 20) | PROCESS_EE_ACC)

// special info not related to above flags
#define PROCESS_CONSTS 1
#define PROCESS_CONSTT 2

// XMM caching helpers
enum xmminfo : u16 
{
	XMMINFO_READLO = 0x001,
	XMMINFO_READHI = 0x002,
	XMMINFO_WRITELO = 0x004,
	XMMINFO_WRITEHI = 0x008,
	XMMINFO_WRITED = 0x010,
	XMMINFO_READD = 0x020,
	XMMINFO_READS = 0x040,
	XMMINFO_READT = 0x080,
	XMMINFO_READACC = 0x200,
	XMMINFO_WRITEACC = 0x400,
	XMMINFO_WRITET = 0x800,

	XMMINFO_64BITOP = 0x1000,
	XMMINFO_FORCEREGS = 0x2000,
	XMMINFO_FORCEREGT = 0x4000,
	XMMINFO_NORENAME = 0x8000 // disables renaming of Rs to Rt in Rt = Rs op imm
};

////////////////////////////////////////////////////////////////////////////////
//   ARM64 (64-bit) Register Allocation Tools

enum arm64type : u8 
{
	ARM64TYPE_TEMP = 0,
	ARM64TYPE_GPR = 1,
	ARM64TYPE_FPRC = 2,
	ARM64TYPE_VIREG = 3,
	ARM64TYPE_PCWRITEBACK = 4,
	ARM64TYPE_PSX = 5,
	ARM64TYPE_PSX_PCWRITEBACK = 6
};

struct _arm64regs
{
	u8 inuse;
	s8 reg;
	u8 mode;
	u8 needed;
	u8 type; // ARM64TYPE_
	u16 counter;
	u32 extra; // extra info assoc with the reg
};

extern _arm64regs arm64regs[iREGCNT_GPR], s_saveArm64regs[iREGCNT_GPR];

bool _isAllocatableArm64reg(int arm64reg);
void _initArm64regs();
int _getFreeArm64reg(int mode);
int _allocArm64reg(int type, int reg, int mode);
int _checkArm64reg(int type, int reg, int mode);
bool _hasArm64reg(int type, int reg, int required_mode = 0);
void _addNeededArm64reg(int type, int reg);
void _clearNeededArm64regs();
void _freeArm64reg(const a64::Register& arm64reg);
void _freeArm64reg(int arm64reg);
void _freeArm64regWithoutWriteback(int arm64reg);
void _freeArm64regs();
void _flushArm64regs();
void _flushConstRegs(bool delete_const);
void _flushConstReg(int reg);
void _validateRegs();
void _writebackArm64Reg(int arm64reg);

void mVUFreeCOP2GPR(int hostreg);
bool mVUIsReservedCOP2(int hostreg);

////////////////////////////////////////////////////////////////////////////////
//   Q (128-bit) Register Allocation Tools

#define QTYPE_TEMP   0 // has to be 0
#define QTYPE_GPRREG ARM64TYPE_GPR
#define QTYPE_FPREG  6
#define QTYPE_FPACC  7
#define QTYPE_VFREG  8

// lo and hi regs
#define QGPR_LO  33
#define QGPR_HI  32
#define QFPU_ACC 32

enum : int
{
	DELETE_REG_FREE = 0,
	DELETE_REG_FLUSH = 1,
	DELETE_REG_FLUSH_AND_FREE = 2,
	DELETE_REG_FREE_NO_WRITEBACK = 3
};

struct _qregs
{
	u8 inuse;
	s8 reg;
	u8 type;
	u8 mode;
	u8 needed;
	u16 counter;
};

extern _qregs qregs[iREGCNT_XMM], s_saveQregs[iREGCNT_XMM];

void _initQregs();
int _checkQreg(int type, int reg);
int _allocQreg(int type, int reg);
void _clearQregs();
void _freeQreg(int qreg);
void _freeQregs();
void _flushQregs();
void _flushConstQregs();
void _writebackQreg(int qreg);

// GPR allocation in Q registers
int _allocGPRtoQreg(int gprreg, int mode);
int _checkQregGPR(int gprreg, int mode);
void _freeQregGPR(int gprreg);
void _flushQregGPR(int gprreg);

// Helper macros for backward compatibility with x86 code
#define _allocX86reg _allocArm64reg
#define _freeX86reg _freeArm64reg
#define _freeX86regs _freeArm64regs
#define _flushX86regs _flushArm64regs
#define _checkX86reg _checkArm64reg
#define _hasX86reg _hasArm64reg
#define _addNeededX86reg _addNeededArm64reg
#define _clearNeededX86regs _clearNeededArm64regs
#define _freeX86regWithoutWriteback _freeArm64regWithoutWriteback
#define _writebackX86Reg _writebackArm64Reg

#define x86regs arm64regs
#define s_saveX86regs s_saveArm64regs
#define iREGCNT_GPR 32

#define _allocGPRtoXMMreg _allocGPRtoQreg
#define _checkXMMreg _checkQreg
#define _allocXMMreg _allocQreg
#define _freeXMMreg _freeQreg
#define _freeXMMregs _freeQregs
#define _flushXMMregs _flushQregs
#define _flushConstXMMregs _flushConstQregs
#define _writebackXMMReg _writebackQreg

#define xmmregs qregs
#define s_saveXmmregs s_saveQregs
#define iREGCNT_XMM 32

// Register type compatibility
#define X86TYPE_TEMP ARM64TYPE_TEMP
#define X86TYPE_GPR ARM64TYPE_GPR
#define X86TYPE_FPRC ARM64TYPE_FPRC
#define X86TYPE_VIREG ARM64TYPE_VIREG
#define X86TYPE_PCWRITEBACK ARM64TYPE_PCWRITEBACK
#define X86TYPE_PSX ARM64TYPE_PSX
#define X86TYPE_PSX_PCWRITEBACK ARM64TYPE_PSX_PCWRITEBACK

#define XMMTYPE_TEMP QTYPE_TEMP
#define XMMTYPE_GPRREG QTYPE_GPRREG
#define XMMTYPE_FPREG QTYPE_FPREG
#define XMMTYPE_FPACC QTYPE_FPACC
#define XMMTYPE_VFREG QTYPE_VFREG
