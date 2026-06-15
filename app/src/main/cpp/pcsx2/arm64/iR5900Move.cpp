// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* Register move                                        *
* Format:  OP rd, rs                                    *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

static void recMoveStoD(int info)
{
	if (info & PROCESS_EE_S) {
		armAsm->Mov(a64::WRegister(EEREC_D), a64::WRegister(EEREC_S));
	}
	else {
		armLoad(a64::WRegister(EEREC_D), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
	}
}

static void recMoveStoD64(int info)
{
	if (info & PROCESS_EE_S) {
		armAsm->Mov(a64::XRegister(EEREC_D), a64::XRegister(EEREC_S));
	}
	else {
		armLoad(a64::XRegister(EEREC_D), PTR_CPU(cpuRegs.GPR.r[_Rs_].UD[0]));
	}
}

//// MOV
static void recMOV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0];
}

static void recMOV_(int info)
{
	recMoveStoD(info);
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recMOV(int info)
{
	recRecompileCodeConst1(recMOV_const, recMOV_, 0);
}

//// MFHI
static void recMFHI_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)g_cpuConstRegs[_Rd_].SD[0];
}

static void recMFHI_(int info)
{
	armLoad(a64::WRegister(EEREC_D), PTR_CPU(cpuRegs.HI.UD[0]));
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recMFHI(int info)
{
	recRecompileCodeConst1(recMFHI_const, recMFHI_, 0);
}

//// MTHI
static void recMTHI_const()
{
	// HI is not a register that can be constant
	// This should never be called
}

static void recMTHI_(int info)
{
	if (info & PROCESS_EE_S) {
		armStore(PTR_CPU(cpuRegs.HI.UD[0]), a64::XRegister(EEREC_S));
	}
	else {
		armLoad(a64::XRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UD[0]));
		armStore(PTR_CPU(cpuRegs.HI.UD[0]), a64::XRegister(EEREC_S));
	}
}

void recMTHI(int info)
{
	recRecompileCodeConst1(recMTHI_const, recMTHI_, 0);
}

//// MFLO
static void recMFLO_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)g_cpuConstRegs[_Rd_].SD[0];
}

static void recMFLO_(int info)
{
	armLoad(a64::WRegister(EEREC_D), PTR_CPU(cpuRegs.LO.UD[0]));
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recMFLO(int info)
{
	recRecompileCodeConst1(recMFLO_const, recMFLO_, 0);
}

//// MTLO
static void recMTLO_const()
{
	// LO is not a register that can be constant
	// This should never be called
}

static void recMTLO_(int info)
{
	if (info & PROCESS_EE_S) {
		armStore(PTR_CPU(cpuRegs.LO.UD[0]), a64::XRegister(EEREC_S));
	}
	else {
		armLoad(a64::XRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UD[0]));
		armStore(PTR_CPU(cpuRegs.LO.UD[0]), a64::XRegister(EEREC_S));
	}
}

void recMTLO(int info)
{
	recRecompileCodeConst1(recMTLO_const, recMTLO_, 0);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
