// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

/*********************************************************
* COP2 (VU) instructions                                 *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recMFC2();
	void recMTC2();
	void recCFC2();
	void recCTC2();
	void recLWC2();
	void recSWC2();
	void recLDC2();
	void recSDC2();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
