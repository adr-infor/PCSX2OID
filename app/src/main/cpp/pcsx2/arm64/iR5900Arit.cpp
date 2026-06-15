// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

// Helper functions for moving registers
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

static void recMoveTtoD(int info)
{
	if (info & PROCESS_EE_T) {
		armAsm->Mov(a64::WRegister(EEREC_D), a64::WRegister(EEREC_T));
	}
	else {
		armLoad(a64::WRegister(EEREC_D), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
	}
}

static void recMoveTtoD64(int info)
{
	if (info & PROCESS_EE_T) {
		armAsm->Mov(a64::XRegister(EEREC_D), a64::XRegister(EEREC_T));
	}
	else {
		armLoad(a64::XRegister(EEREC_D), PTR_CPU(cpuRegs.GPR.r[_Rt_].UD[0]));
	}
}

//// ADD
static void recADD_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] + g_cpuConstRegs[_Rt_].UL[0]));
}

// s is constant
static void recADD_consts(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 cval = g_cpuConstRegs[_Rs_].SL[0];
	recMoveTtoD(info);
	if (cval != 0) {
		armAsm->Add(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

// t is constant
static void recADD_constt(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	recMoveStoD(info);
	if (cval != 0) {
		armAsm->Add(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

// nothing is constant
static void recADD_(int info)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	auto reg32 = a64::WRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->Add(reg32, reg32, a64::WRegister(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->Add(reg32, reg32, a64::WRegister(EEREC_S));
		}
		else
		{
			armAsm->Mov(reg32, a64::WRegister(EEREC_S));
			armAsm->Add(reg32, reg32, a64::WRegister(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_S));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Add(reg32, reg32, a64::WRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_T)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_T));
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Add(reg32, reg32, a64::WRegister(EEREC_S));
	}
	else
	{
		armLoad(reg32, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Add(reg32, reg32, a64::WRegister(EEREC_T));
	}

	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recADD(int info)
{
	recRecompileCodeConst0(recADD_const, recADD_consts, recADD_constt, recADD_, 0);
}

//// ADDU (same as ADD, no overflow check)
void recADDU(int info)
{
	recADD(info);
}

//// DADD (64-bit)
static void recDADD_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] + g_cpuConstRegs[_Rt_].UD[0];
}

static void recDADD_consts(int info)
{
	const u64 cval = g_cpuConstRegs[_Rs_].UD[0];
	recMoveTtoD64(info);
	if (cval != 0) {
		armAsm->Add(a64::XRegister(EEREC_D), a64::XRegister(EEREC_D), cval);
	}
}

static void recDADD_constt(int info)
{
	const u64 cval = g_cpuConstRegs[_Rt_].UD[0];
	recMoveStoD64(info);
	if (cval != 0) {
		armAsm->Add(a64::XRegister(EEREC_D), a64::XRegister(EEREC_D), cval);
	}
}

static void recDADD_(int info)
{
	auto reg64 = a64::XRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->Add(reg64, reg64, a64::XRegister(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->Add(reg64, reg64, a64::XRegister(EEREC_S));
		}
		else
		{
			armAsm->Mov(reg64, a64::XRegister(EEREC_S));
			armAsm->Add(reg64, reg64, a64::XRegister(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armAsm->Mov(reg64, a64::XRegister(EEREC_S));
		armLoad(a64::XRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UD[0]));
		armAsm->Add(reg64, reg64, a64::XRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_T)
	{
		armAsm->Mov(reg64, a64::XRegister(EEREC_T));
		armLoad(a64::XRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UD[0]));
		armAsm->Add(reg64, reg64, a64::XRegister(EEREC_S));
	}
	else
	{
		armLoad(reg64, PTR_CPU(cpuRegs.GPR.r[_Rs_].UD[0]));
		armLoad(a64::XRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UD[0]));
		armAsm->Add(reg64, reg64, a64::XRegister(EEREC_T));
	}
}

void recDADD(int info)
{
	recRecompileCodeConst0(recDADD_const, recDADD_consts, recDADD_constt, recDADD_, 0);
}

void recDADDU(int info)
{
	recDADD(info);
}

//// SUB
static void recSUB_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = s64(s32(g_cpuConstRegs[_Rs_].UL[0] - g_cpuConstRegs[_Rt_].UL[0]));
}

static void recSUB_consts(int info)
{
	const s32 cval = g_cpuConstRegs[_Rs_].SL[0];
	recMoveTtoD(info);
	armAsm->Neg(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D));
	if (cval != 0) {
		armAsm->Add(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

static void recSUB_constt(int info)
{
	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	recMoveStoD(info);
	if (cval != 0) {
		armAsm->Sub(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

static void recSUB_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->Sub(reg32, reg32, a64::WRegister(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->Mov(reg32, a64::WRegister(EEREC_S));
			armAsm->Sub(reg32, reg32, a64::WRegister(EEREC_T));
		}
		else
		{
			armAsm->Mov(reg32, a64::WRegister(EEREC_S));
			armAsm->Sub(reg32, reg32, a64::WRegister(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_S));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Sub(reg32, reg32, a64::WRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_T)
	{
		armLoad(reg32, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Sub(reg32, reg32, a64::WRegister(EEREC_T));
	}
	else
	{
		armLoad(reg32, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Sub(reg32, reg32, a64::WRegister(EEREC_T));
	}

	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recSUB(int info)
{
	recRecompileCodeConst0(recSUB_const, recSUB_consts, recSUB_constt, recSUB_, 0);
}

void recSUBU(int info)
{
	recSUB(info);
}

//// DSUB (64-bit)
static void recDSUB_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] - g_cpuConstRegs[_Rt_].UD[0];
}

static void recDSUB_consts(int info)
{
	const u64 cval = g_cpuConstRegs[_Rs_].UD[0];
	recMoveTtoD64(info);
	armAsm->Neg(a64::XRegister(EEREC_D), a64::XRegister(EEREC_D));
	if (cval != 0) {
		armAsm->Add(a64::XRegister(EEREC_D), a64::XRegister(EEREC_D), cval);
	}
}

static void recDSUB_constt(int info)
{
	const u64 cval = g_cpuConstRegs[_Rt_].UD[0];
	recMoveStoD64(info);
	if (cval != 0) {
		armAsm->Sub(a64::XRegister(EEREC_D), a64::XRegister(EEREC_D), cval);
	}
}

static void recDSUB_(int info)
{
	auto reg64 = a64::XRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->Sub(reg64, reg64, a64::XRegister(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->Mov(reg64, a64::XRegister(EEREC_S));
			armAsm->Sub(reg64, reg64, a64::XRegister(EEREC_T));
		}
		else
		{
			armAsm->Mov(reg64, a64::XRegister(EEREC_S));
			armAsm->Sub(reg64, reg64, a64::XRegister(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armAsm->Mov(reg64, a64::XRegister(EEREC_S));
		armLoad(a64::XRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UD[0]));
		armAsm->Sub(reg64, reg64, a64::XRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_T)
	{
		armLoad(reg64, PTR_CPU(cpuRegs.GPR.r[_Rs_].UD[0]));
		armAsm->Sub(reg64, reg64, a64::XRegister(EEREC_T));
	}
	else
	{
		armLoad(reg64, PTR_CPU(cpuRegs.GPR.r[_Rs_].UD[0]));
		armLoad(a64::XRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UD[0]));
		armAsm->Sub(reg64, reg64, a64::XRegister(EEREC_T));
	}
}

void recDSUB(int info)
{
	recRecompileCodeConst0(recDSUB_const, recDSUB_consts, recDSUB_constt, recDSUB_, 0);
}

void recDSUBU(int info)
{
	recDSUB(info);
}

//// AND
static void recAND_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] & g_cpuConstRegs[_Rt_].UD[0];
}

static void recAND_consts(int info)
{
	const u32 cval = g_cpuConstRegs[_Rs_].UL[0];
	recMoveTtoD(info);
	if (cval != 0xFFFFFFFF) {
		armAsm->And(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
}

static void recAND_constt(int info)
{
	const u32 cval = g_cpuConstRegs[_Rt_].UL[0];
	recMoveStoD(info);
	if (cval != 0xFFFFFFFF) {
		armAsm->And(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
}

static void recAND_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->And(reg32, reg32, a64::WRegister(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->And(reg32, reg32, a64::WRegister(EEREC_S));
		}
		else
		{
			armAsm->Mov(reg32, a64::WRegister(EEREC_S));
			armAsm->And(reg32, reg32, a64::WRegister(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_S));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->And(reg32, reg32, a64::WRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_T)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_T));
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->And(reg32, reg32, a64::WRegister(EEREC_S));
	}
	else
	{
		armLoad(reg32, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->And(reg32, reg32, a64::WRegister(EEREC_T));
	}
}

void recAND(int info)
{
	recRecompileCodeConst0(recAND_const, recAND_consts, recAND_constt, recAND_, 0);
}

//// OR
static void recOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0];
}

static void recOR_consts(int info)
{
	const u32 cval = g_cpuConstRegs[_Rs_].UL[0];
	recMoveTtoD(info);
	if (cval != 0) {
		armAsm->Orr(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
}

static void recOR_constt(int info)
{
	const u32 cval = g_cpuConstRegs[_Rt_].UL[0];
	recMoveStoD(info);
	if (cval != 0) {
		armAsm->Orr(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
}

static void recOR_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_S));
		}
		else
		{
			armAsm->Mov(reg32, a64::WRegister(EEREC_S));
			armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_S));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_T)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_T));
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_S));
	}
	else
	{
		armLoad(reg32, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_T));
	}
}

void recOR(int info)
{
	recRecompileCodeConst0(recOR_const, recOR_consts, recOR_constt, recOR_, 0);
}

//// XOR
static void recXOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0] ^ g_cpuConstRegs[_Rt_].UD[0];
}

static void recXOR_consts(int info)
{
	const u32 cval = g_cpuConstRegs[_Rs_].UL[0];
	recMoveTtoD(info);
	if (cval != 0) {
		armAsm->Eor(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
}

static void recXOR_constt(int info)
{
	const u32 cval = g_cpuConstRegs[_Rt_].UL[0];
	recMoveStoD(info);
	if (cval != 0) {
		armAsm->Eor(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	}
}

static void recXOR_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->Eor(reg32, reg32, a64::WRegister(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->Eor(reg32, reg32, a64::WRegister(EEREC_S));
		}
		else
		{
			armAsm->Mov(reg32, a64::WRegister(EEREC_S));
			armAsm->Eor(reg32, reg32, a64::WRegister(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_S));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Eor(reg32, reg32, a64::WRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_T)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_T));
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Eor(reg32, reg32, a64::WRegister(EEREC_S));
	}
	else
	{
		armLoad(reg32, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Eor(reg32, reg32, a64::WRegister(EEREC_T));
	}
}

void recXOR(int info)
{
	recRecompileCodeConst0(recXOR_const, recXOR_consts, recXOR_constt, recXOR_, 0);
}

//// NOR
static void recNOR_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = ~(g_cpuConstRegs[_Rs_].UD[0] | g_cpuConstRegs[_Rt_].UD[0]);
}

static void recNOR_consts(int info)
{
	const u32 cval = g_cpuConstRegs[_Rs_].UL[0];
	recMoveTtoD(info);
	armAsm->Orr(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	armAsm->Mvn(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D));
}

static void recNOR_constt(int info)
{
	const u32 cval = g_cpuConstRegs[_Rt_].UL[0];
	recMoveStoD(info);
	armAsm->Orr(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), cval);
	armAsm->Mvn(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D));
}

static void recNOR_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_S));
		}
		else
		{
			armAsm->Mov(reg32, a64::WRegister(EEREC_S));
			armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_T));
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_S));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_T)
	{
		armAsm->Mov(reg32, a64::WRegister(EEREC_T));
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_S));
	}
	else
	{
		armLoad(reg32, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Orr(reg32, reg32, a64::WRegister(EEREC_T));
	}

	armAsm->Mvn(reg32, reg32);
}

void recNOR(int info)
{
	recRecompileCodeConst0(recNOR_const, recNOR_consts, recNOR_constt, recNOR_, 0);
}

//// SLT
static void recSLT_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (s32)g_cpuConstRegs[_Rs_].SL[0] < (s32)g_cpuConstRegs[_Rt_].SL[0];
}

static void recSLT_consts(int info)
{
	const s32 cval = g_cpuConstRegs[_Rs_].SL[0];
	recMoveTtoD(info);
	armAsm->Cmp(a64::WRegister(EEREC_D), cval);
	armAsm->Cset(a64::WRegister(EEREC_D), vixl::aarch64::lt);
}

static void recSLT_constt(int info)
{
	const s32 cval = g_cpuConstRegs[_Rt_].SL[0];
	recMoveStoD(info);
	armAsm->Cmp(a64::WRegister(EEREC_D), cval);
	armAsm->Cset(a64::WRegister(EEREC_D), vixl::aarch64::lt);
}

static void recSLT_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
		armAsm->Cset(reg32, vixl::aarch64::lt);
	}
	else if (info & PROCESS_EE_S)
	{
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
		armAsm->Cset(reg32, vixl::aarch64::lt);
	}
	else if (info & PROCESS_EE_T)
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
		armAsm->Cset(reg32, vixl::aarch64::lt);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
		armAsm->Cset(reg32, vixl::aarch64::lt);
	}
}

void recSLT(int info)
{
	recRecompileCodeConst0(recSLT_const, recSLT_consts, recSLT_constt, recSLT_, 0);
}

//// SLTU
static void recSLTU_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UL[0] < g_cpuConstRegs[_Rt_].UL[0];
}

static void recSLTU_consts(int info)
{
	const u32 cval = g_cpuConstRegs[_Rs_].UL[0];
	recMoveTtoD(info);
	armAsm->Cmp(a64::WRegister(EEREC_D), cval);
	armAsm->Cset(a64::WRegister(EEREC_D), vixl::aarch64::lo);
}

static void recSLTU_constt(int info)
{
	const u32 cval = g_cpuConstRegs[_Rt_].UL[0];
	recMoveStoD(info);
	armAsm->Cmp(a64::WRegister(EEREC_D), cval);
	armAsm->Cset(a64::WRegister(EEREC_D), vixl::aarch64::lo);
}

static void recSLTU_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
		armAsm->Cset(reg32, vixl::aarch64::lo);
	}
	else if (info & PROCESS_EE_S)
	{
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
		armAsm->Cset(reg32, vixl::aarch64::lo);
	}
	else if (info & PROCESS_EE_T)
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
		armAsm->Cset(reg32, vixl::aarch64::lo);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
		armAsm->Cset(reg32, vixl::aarch64::lo);
	}
}

void recSLTU(int info)
{
	recRecompileCodeConst0(recSLTU_const, recSLTU_consts, recSLTU_constt, recSLTU_, 0);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
