// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

/*********************************************************
* Immediate arithmetic                                   *
* Format:  OP rt, rs, imm16                              *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recDADDI();
	void recDADDIU();
	void recDSUBI();
	void recDSUBIU();
	void recSLTI();
	void recSLTIU();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
