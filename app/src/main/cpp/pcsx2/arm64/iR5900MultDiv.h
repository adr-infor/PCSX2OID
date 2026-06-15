// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

/*********************************************************
* Multiply and Divide                                   *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recMULT();
	void recMULTU();
	void recDMULT();
	void recDMULTU();
	void recDIV();
	void recDIVU();
	void recDDIV();
	void recDDIVU();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
