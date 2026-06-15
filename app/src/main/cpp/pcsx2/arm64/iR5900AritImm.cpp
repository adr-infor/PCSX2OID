// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* Immediate arithmetic                                   *
* Format:  OP rt, rs, imm16                              *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

static void recMoveStoT(int info)
{
	if (info & PROCESS_EE_S) {
		armAsm->Mov(a64::WRegister(EEREC_T), a64::WRegister(EEREC_S));
	}
	else {
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
	}
}

static void recMoveStoT64(int info)
{
	if (info & PROCESS_EE_S) {
		armAsm->Mov(a64::XRegister(EEREC_T), a64::XRegister(EEREC_S));
	}
	else {
		armLoad(a64::XRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rs_].UD[0]));
	}
}

//// DADDI (Doubleword Add Immediate)
static void recDADDI_const()
{
	g_cpuConstRegs[_Rt_].SD[0] = g_cpuConstRegs[_Rs_].SD[0] + _Imm16_;
}

static void recDADDI_(int info)
{
	recMoveStoT64(info);
	if (_Imm16_ != 0) {
		armAsm->Add(a64::XRegister(EEREC_T), a64::XRegister(EEREC_T), _Imm16_);
	}
}

void recDADDI(int info)
{
	recRecompileCodeConst1(recDADDI_const, recDADDI_, 0);
}

//// DADDIU (Doubleword Add Immediate Unsigned)
static void recDADDIU_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] + _Imm16_;
}

static void recDADDIU_(int info)
{
	recMoveStoT64(info);
	if (_Imm16_ != 0) {
		armAsm->Add(a64::XRegister(EEREC_T), a64::XRegister(EEREC_T), _Imm16_);
	}
}

void recDADDIU(int info)
{
	recRecompileCodeConst1(recDADDIU_const, recDADDIU_, 0);
}

//// DSUBI (Doubleword Subtract Immediate)
static void recDSUBI_const()
{
	g_cpuConstRegs[_Rt_].SD[0] = g_cpuConstRegs[_Rs_].SD[0] - _Imm16_;
}

static void recDSUBI_(int info)
{
	recMoveStoT64(info);
	if (_Imm16_ != 0) {
		armAsm->Sub(a64::XRegister(EEREC_T), a64::XRegister(EEREC_T), _Imm16_);
	}
}

void recDSUBI(int info)
{
	recRecompileCodeConst1(recDSUBI_const, recDSUBI_, 0);
}

//// DSUBIU (Doubleword Subtract Immediate Unsigned)
static void recDSUBIU_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] - _Imm16_;
}

static void recDSUBIU_(int info)
{
	recMoveStoT64(info);
	if (_Imm16_ != 0) {
		armAsm->Sub(a64::XRegister(EEREC_T), a64::XRegister(EEREC_T), _Imm16_);
	}
}

void recDSUBIU(int info)
{
	recRecompileCodeConst1(recDSUBIU_const, recDSUBIU_, 0);
}

//// SLTI (Set on Less Than Immediate)
static void recSLTI_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = (s32)g_cpuConstRegs[_Rs_].SL[0] < _Imm16_;
}

static void recSLTI_(int info)
{
	recMoveStoT(info);
	armAsm->Cmp(a64::WRegister(EEREC_T), _Imm16_);
	armAsm->Cset(a64::WRegister(EEREC_T), vixl::aarch64::lt);
	armAsm->Sxtw(a64::XRegister(EEREC_T), a64::WRegister(EEREC_T));
}

void recSLTI(int info)
{
	recRecompileCodeConst1(recSLTI_const, recSLTI_, 0);
}

//// SLTIU (Set on Less Than Immediate Unsigned)
static void recSLTIU_const()
{
	g_cpuConstRegs[_Rt_].UD[0] = g_cpuConstRegs[_Rs_].UL[0] < (u16)_Imm16_;
}

static void recSLTIU_(int info)
{
	recMoveStoT(info);
	armAsm->Cmp(a64::WRegister(EEREC_T), _Imm16_);
	armAsm->Cset(a64::WRegister(EEREC_T), vixl::aarch64::lo);
	armAsm->Sxtw(a64::XRegister(EEREC_T), a64::WRegister(EEREC_T));
}

void recSLTIU(int info)
{
	recRecompileCodeConst1(recSLTIU_const, recSLTIU_, 0);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
