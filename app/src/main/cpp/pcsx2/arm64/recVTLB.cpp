// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "vtlb.h"
#include "iR5900.h"
#include "VixlHelpers.h"

#include "common/Perf.h"

using namespace vtlb_private;

// Pseudo-Code For the following Dynarec Implementations -->
//
// u32 vmv = vmap[addr>>VTLB_PAGE_BITS].raw();
// sptr ppf=addr+vmv;
// if (!(ppf<0))
// {
//     data[0]=*reinterpret_cast<DataType*>(ppf);
//     if (DataSize==128)
//         data[1]=*reinterpret_cast<DataType*>(ppf+8);
//     return 0;
// }
// else
// {
//     //has to: translate, find function, call function
//     u32 hand=(u8)vmv;
//     u32 paddr=(ppf-hand) << 1;
//     return reinterpret_cast<TemplateHelper<DataSize,false>::HandlerType*>(RWFT[TemplateHelper<DataSize,false>::sidx][0][hand])(paddr,data);
// }

namespace vtlb_private
{
	// ------------------------------------------------------------------------
	// Prepares registers for Direct or Indirect operations.
	// Returns the writeback pointer for indirect handling
	//
	static void DynGen_PrepRegs(int addr_reg, int value_reg, u32 sz, bool xmm)
	{
		EE::Profiler.EmitMem();

		// Move address into ECX (arg1)
		_freeX86reg(ECX.GetCode());
		armAsm->Mov(ECX, a64::WRegister(addr_reg));

		if (value_reg >= 0)
		{
			if (sz == 128)
			{
				pxAssert(xmm);
				_freeXMMreg(armQRegister(1).GetCode());
				armAsm->Mov(armQRegister(1), armQRegister(value_reg));
			}
			else if (xmm)
			{
				// 32bit xmms are passed in GPRs
				pxAssert(sz == 32);
				_freeX86reg(EDX.GetCode());
				armAsm->Fmov(EDX, a64::QRegister(value_reg).S());
			}
			else
			{
				_freeX86reg(EDX.GetCode());
				armAsm->Mov(RDX, a64::XRegister(value_reg));
			}
		}

		// Calculate vmap index: addr >> VTLB_PAGE_BITS
		armAsm->Mov(EAX, ECX);
		armAsm->Lsr(EAX, EAX, VTLB_PAGE_BITS);

		// Load vmap entry: vmap[index]
		armAsm->Ldr(RXVIXLSCRATCH, PTR_CPU(vtlbdata.vmap));
		armAsm->Ldr(RAX, a64::MemOperand(RXVIXLSCRATCH, RAX, a64::LSL, 3));

		// Add offset to address: addr + vmap_entry
		armAsm->Adds(RCX, RCX, RAX);
	}

	// Like DynGen_PrepRegs but without _freeX86reg/_freeXMMreg calls.
	// Safe to use in backpatch thunks (signal handler context)
	static void DynGen_PrepRegsNoFree(int addr_reg, int value_reg, u32 sz, bool xmm)
	{
		// Move address into ECX (arg1)
		if (static_cast<u32>(addr_reg) != ECX.GetCode())
			armAsm->Mov(ECX, a64::WRegister(addr_reg));

		if (value_reg >= 0)
		{
			if (sz == 128)
			{
				pxAssert(xmm);
				// Move data into Q1 (arg2 for 128-bit)
				if (static_cast<u32>(value_reg) != armQRegister(1).GetCode())
					armAsm->Mov(armQRegister(1), armQRegister(value_reg));
			}
			else if (xmm)
			{
				pxAssert(sz == 32);
				// Move 32-bit float data into EDX
				armAsm->Fmov(EDX, a64::QRegister(value_reg).S());
			}
			else
			{
				// Move data into RDX (arg2)
				if (static_cast<u32>(value_reg) != RDX.GetCode())
					armAsm->Mov(RDX, a64::XRegister(value_reg));
			}
		}

		// Calculate vmap index
		armAsm->Mov(EAX, ECX);
		armAsm->Lsr(EAX, EAX, VTLB_PAGE_BITS);

		// Load vmap entry
		armAsm->Ldr(RXVIXLSCRATCH, PTR_CPU(vtlbdata.vmap));
		armAsm->Ldr(RAX, a64::MemOperand(RXVIXLSCRATCH, RAX, a64::LSL, 3));

		// Add offset
		armAsm->Adds(RCX, RCX, RAX);
	}

	// ------------------------------------------------------------------------
	// Generates code for a direct memory access (fast path)
	//
	static void DynGen_DirectRead(u32 sz, bool sign)
	{
		// Check if result is negative (indirect access)
		a64::Label indirect;
		armAsm->B(mi, &indirect);

		// Direct read path
		switch (sz)
		{
			case 8:
				if (sign)
					armAsm->Ldrsb(EAX, a64::MemOperand(RCX));
				else
					armAsm->Ldrb(EAX, a64::MemOperand(RCX));
				break;
			case 16:
				if (sign)
					armAsm->Ldrsh(EAX, a64::MemOperand(RCX));
				else
					armAsm->Ldrh(EAX, a64::MemOperand(RCX));
				break;
			case 32:
				armAsm->Ldr(EAX, a64::MemOperand(RCX));
				break;
			case 64:
				armAsm->Ldr(RAX, a64::MemOperand(RCX));
				break;
			case 128:
				armAsm->Ldr(armQRegister(0), a64::MemOperand(RCX));
				break;
		}

		a64::Label done;
		armAsm->B(&done);

		armAsm->Bind(&indirect);

		// Indirect read path - call handler
		// Extract handler from low byte
		armAsm->And(EAX, RAX, 0xFF);

		// Calculate physical address
		armAsm->Sub(RCX, RCX, EAX);
		armAsm->Lsl(RCX, RCX, 1);

		// Call handler based on size and sign
		// TODO: Implement handler table lookup and call

		armAsm->Bind(&done);
	}

	static void DynGen_DirectWrite(u32 sz)
	{
		// Check if result is negative (indirect access)
		a64::Label indirect;
		armAsm->B(mi, &indirect);

		// Direct write path
		switch (sz)
		{
			case 8:
				armAsm->Strb(EAX, a64::MemOperand(RCX));
				break;
			case 16:
				armAsm->Strh(EAX, a64::MemOperand(RCX));
				break;
			case 32:
				armAsm->Str(EAX, a64::MemOperand(RCX));
				break;
			case 64:
				armAsm->Str(RAX, a64::MemOperand(RCX));
				break;
			case 128:
				armAsm->Str(armQRegister(0), a64::MemOperand(RCX));
				break;
		}

		a64::Label done;
		armAsm->B(&done);

		armAsm->Bind(&indirect);

		// Indirect write path - call handler
		// Extract handler from low byte
		armAsm->And(EAX, RAX, 0xFF);

		// Calculate physical address
		armAsm->Sub(RCX, RCX, EAX);
		armAsm->Lsl(RCX, RCX, 1);

		// Call handler based on size
		// TODO: Implement handler table lookup and call

		armAsm->Bind(&done);
	}

	// ------------------------------------------------------------------------
	// Public API for memory access generation
	//

	void vtlb_DynBackpatchLoadStore(uptr code_address, u32 code_size, u32 guest_pc, u32 guest_addr, u32 gpr_bitmask, u32 fpr_bitmask, u8 address_register, u8 data_register, u8 size_in_bits, bool is_signed, bool is_load, bool is_fpr)
	{
		// TODO: Implement backpatching for ARM64
		// This is used for fastmem backpatching when page protection triggers
		Console.Warning("vtlb_DynBackpatchLoadStore: ARM64 backpatching not yet implemented");
	}

} // namespace vtlb_private
