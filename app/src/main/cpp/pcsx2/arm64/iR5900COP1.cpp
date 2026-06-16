// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* COP1 (FPU) instructions                                *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

// For now, COP1 instructions call the interpreter
// These are complex and require special handling of FPU registers

//// MFC1 (Move from Coprocessor 1)
void recMFC1(int info)
{
	recCall(Interp::MFC1);
}

//// MTC1 (Move to Coprocessor 1)
void recMTC1(int info)
{
	recCall(Interp::MTC1);
}

//// CFC1 (Move Control from Coprocessor 1)
void recCFC1(int info)
{
	recCall(Interp::CFC1);
}

//// CTC1 (Move Control to Coprocessor 1)
void recCTC1(int info)
{
	recCall(Interp::CTC1);
}

//// LWC1 (Load Word to Coprocessor 1)
void recLWC1(int info)
{
	recCall(Interp::LWC1);
}

//// SWC1 (Store Word from Coprocessor 1)
void recSWC1(int info)
{
	recCall(Interp::SWC1);
}

//// LDC1 (Load Doubleword to Coprocessor 1)
void recLDC1(int info)
{
	recCall(Interp::LDC1);
}

//// SDC1 (Store Doubleword from Coprocessor 1)
void recSDC1(int info)
{
	recCall(Interp::SDC1);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
