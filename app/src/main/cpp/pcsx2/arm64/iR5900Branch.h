// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

/*********************************************************
* Branch instructions                                    *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recBEQ();
	void recBNE();
	void recBLTZ();
	void recBGEZ();
	void recBLEZ();
	void recBGTZ();
	void recBEQL();
	void recBNEL();
	void recBLTZL();
	void recBGEZL();
	void recBLEZL();
	void recBGTZL();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
