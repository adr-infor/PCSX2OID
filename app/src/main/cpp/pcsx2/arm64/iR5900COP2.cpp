// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* COP2 (VU) instructions                                 *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

// For now, COP2 instructions call the interpreter
// These are complex and require special handling of VU registers

//// MFC2 (Move from Coprocessor 2)
void recMFC2(int info)
{
	recCall(Interp::MFC2);
}

//// MTC2 (Move to Coprocessor 2)
void recMTC2(int info)
{
	recCall(Interp::MTC2);
}

//// CFC2 (Move Control from Coprocessor 2)
void recCFC2(int info)
{
	recCall(Interp::CFC2);
}

//// CTC2 (Move Control to Coprocessor 2)
void recCTC2(int info)
{
	recCall(Interp::CTC2);
}

//// LWC2 (Load Word to Coprocessor 2)
void recLWC2(int info)
{
	recCall(Interp::LWC2);
}

//// SWC2 (Store Word from Coprocessor 2)
void recSWC2(int info)
{
	recCall(Interp::SWC2);
}

//// LDC2 (Load Doubleword to Coprocessor 2)
void recLDC2(int info)
{
	recCall(Interp::LDC2);
}

//// SDC2 (Store Doubleword from Coprocessor 2)
void recSDC2(int info)
{
	recCall(Interp::SDC2);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
