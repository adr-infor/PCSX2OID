// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"
#include "vtlb.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* Load and Store                                      *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

// Helper to calculate address
static void recCalcAddress(int info)
{
	if (info & PROCESS_EE_S) {
		if (_Imm16_ != 0) {
			armAsm->Add(a64::XRegister(EEREC_S), a64::XRegister(EEREC_S), _Imm16_);
		}
	}
	else {
		armLoad(a64::XRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UD[0]));
		if (_Imm16_ != 0) {
			armAsm->Add(a64::XRegister(EEREC_S), a64::XRegister(EEREC_S), _Imm16_);
		}
	}
}

//// LB (Load Byte, signed)
static void recLB_const()
{
	// For constant addresses, we need to use the interpreter
	// since we can't guarantee the address is valid
	// This is a stub - real implementation would handle constant addresses
}

static void recLB_(int info)
{
	recCalcAddress(info);
	
	// Use VTLB for memory access
	// TODO: Implement proper VTLB-based load
	// For now, call interpreter
	recCall(Interp::LB);
}

void recLB(int info)
{
	recRecompileCodeConst1(recLB_const, recLB_, 0);
}

//// LBU (Load Byte, unsigned)
static void recLBU_const()
{
	// Stub for constant address
}

static void recLBU_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LBU);
}

void recLBU(int info)
{
	recRecompileCodeConst1(recLBU_const, recLBU_, 0);
}

//// LH (Load Halfword, signed)
static void recLH_const()
{
	// Stub for constant address
}

static void recLH_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LH);
}

void recLH(int info)
{
	recRecompileCodeConst1(recLH_const, recLH_, 0);
}

//// LHU (Load Halfword, unsigned)
static void recLHU_const()
{
	// Stub for constant address
}

static void recLHU_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LHU);
}

void recLHU(int info)
{
	recRecompileCodeConst1(recLHU_const, recLHU_, 0);
}

//// LW (Load Word)
static void recLW_const()
{
	// Stub for constant address
}

static void recLW_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LW);
}

void recLW(int info)
{
	recRecompileCodeConst1(recLW_const, recLW_, 0);
}

//// LWL (Load Word Left)
static void recLWL_const()
{
	// Stub for constant address
}

static void recLWL_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LWL);
}

void recLWL(int info)
{
	recRecompileCodeConst1(recLWL_const, recLWL_, 0);
}

//// LWR (Load Word Right)
static void recLWR_const()
{
	// Stub for constant address
}

static void recLWR_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LWR);
}

void recLWR(int info)
{
	recRecompileCodeConst1(recLWR_const, recLWR_, 0);
}

//// LD (Load Doubleword)
static void recLD_const()
{
	// Stub for constant address
}

static void recLD_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LD);
}

void recLD(int info)
{
	recRecompileCodeConst1(recLD_const, recLD_, 0);
}

//// LDL (Load Doubleword Left)
static void recLDL_const()
{
	// Stub for constant address
}

static void recLDL_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LDL);
}

void recLDL(int info)
{
	recRecompileCodeConst1(recLDL_const, recLDL_, 0);
}

//// LDR (Load Doubleword Right)
static void recLDR_const()
{
	// Stub for constant address
}

static void recLDR_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LDR);
}

void recLDR(int info)
{
	recRecompileCodeConst1(recLDR_const, recLDR_, 0);
}

//// LQ (Load Quadword)
static void recLQ_const()
{
	// Stub for constant address
}

static void recLQ_(int info)
{
	recCalcAddress(info);
	recCall(Interp::LQ);
}

void recLQ(int info)
{
	recRecompileCodeConst1(recLQ_const, recLQ_, 0);
}

//// SB (Store Byte)
static void recSB_const()
{
	// Stub for constant address
}

static void recSB_(int info)
{
	recCalcAddress(info);
	recCall(Interp::SB);
}

void recSB(int info)
{
	recRecompileCodeConst1(recSB_const, recSB_, 0);
}

//// SH (Store Halfword)
static void recSH_const()
{
	// Stub for constant address
}

static void recSH_(int info)
{
	recCalcAddress(info);
	recCall(Interp::SH);
}

void recSH(int info)
{
	recRecompileCodeConst1(recSH_const, recSH_, 0);
}

//// SW (Store Word)
static void recSW_const()
{
	// Stub for constant address
}

static void recSW_(int info)
{
	recCalcAddress(info);
	recCall(Interp::SW);
}

void recSW(int info)
{
	recRecompileCodeConst1(recSW_const, recSW_, 0);
}

//// SWL (Store Word Left)
static void recSWL_const()
{
	// Stub for constant address
}

static void recSWL_(int info)
{
	recCalcAddress(info);
	recCall(Interp::SWL);
}

void recSWL(int info)
{
	recRecompileCodeConst1(recSWL_const, recSWL_, 0);
}

//// SWR (Store Word Right)
static void recSWR_const()
{
	// Stub for constant address
}

static void recSWR_(int info)
{
	recCalcAddress(info);
	recCall(Interp::SWR);
}

void recSWR(int info)
{
	recRecompileCodeConst1(recSWR_const, recSWR_, 0);
}

//// SD (Store Doubleword)
static void recSD_const()
{
	// Stub for constant address
}

static void recSD_(int info)
{
	recCalcAddress(info);
	recCall(Interp::SD);
}

void recSD(int info)
{
	recRecompileCodeConst1(recSD_const, recSD_, 0);
}

//// SDL (Store Doubleword Left)
static void recSDL_const()
{
	// Stub for constant address
}

static void recSDL_(int info)
{
	recCalcAddress(info);
	recCall(Interp::SDL);
}

void recSDL(int info)
{
	recRecompileCodeConst1(recSDL_const, recSDL_, 0);
}

//// SDR (Store Doubleword Right)
static void recSDR_const()
{
	// Stub for constant address
}

static void recSDR_(int info)
{
	recCalcAddress(info);
	recCall(Interp::SDR);
}

void recSDR(int info)
{
	recRecompileCodeConst1(recSDR_const, recSDR_, 0);
}

//// SQ (Store Quadword)
static void recSQ_const()
{
	// Stub for constant address
}

static void recSQ_(int info)
{
	recCalcAddress(info);
	recCall(Interp::SQ);
}

void recSQ(int info)
{
	recRecompileCodeConst1(recSQ_const, recSQ_, 0);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
