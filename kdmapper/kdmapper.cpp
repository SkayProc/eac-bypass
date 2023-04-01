#include "kdmapper.hpp"


uint64_t kdmapper::AllocMdlMemory(HANDLE iqvw64e_device_handle, uint64_t size, uint64_t* mdlPtr) {
	/*added by psec*/
	LARGE_INTEGER LowAddress, HighAddress;
	LowAddress.QuadPart = 0;
	HighAddress.QuadPart = 0xffff'ffff'ffff'ffffULL;

	uint64_t pages = (size / PAGE_SIZE) + 1;
	auto mdl = intel_driver::MmAllocatePagesForMdl(iqvw64e_device_handle, LowAddress, HighAddress, LowAddress, pages * (uint64_t)PAGE_SIZE);
	if (!mdl) {
		Log(L"[-] Can't allocate pages for mdl" << std::endl);
		return { 0 };
	}

	uint32_t byteCount = 0;
	if (!intel_driver::ReadMemory(iqvw64e_device_handle, mdl + 0x028 /*_MDL : byteCount*/, &byteCount, sizeof(uint32_t))) {
		Log(L"[-] Can't read the _MDL : byteCount" << std::endl);
		return { 0 };
	}

	if (byteCount < size) {
		Log(L"[-] Couldn't allocate enough memory, cleaning up" << std::endl);
		intel_driver::MmFreePagesFromMdl(iqvw64e_device_handle, mdl);
		intel_driver::FreePool(iqvw64e_device_handle, mdl);
		return { 0 };
	}

	auto mappingStartAddress = intel_driver::MmMapLockedPagesSpecifyCache(iqvw64e_device_handle, mdl, nt::KernelMode, nt::MmCached, NULL, FALSE, nt::NormalPagePriority);
	if (!mappingStartAddress) {
		Log(L"[-] Can't set mdl pages cache, cleaning up." << std::endl);
		intel_driver::MmFreePagesFromMdl(iqvw64e_device_handle, mdl);
		intel_driver::FreePool(iqvw64e_device_handle, mdl);
		return { 0 };
	}

	const auto result = intel_driver::MmProtectMdlSystemAddress(iqvw64e_device_handle, mdl, PAGE_EXECUTE_READWRITE);
	if (!result) {
		Log(L"[-] Can't change protection for mdl pages, cleaning up" << std::endl);
		intel_driver::MmUnmapLockedPages(iqvw64e_device_handle, mappingStartAddress, mdl);
		intel_driver::MmFreePagesFromMdl(iqvw64e_device_handle, mdl);
		intel_driver::FreePool(iqvw64e_device_handle, mdl);
		return { 0 };
	}
	Log(L"[+] Allocated pages for mdl" << std::endl);

	if (mdlPtr)
		*mdlPtr = mdl;

	return mappingStartAddress;
}

uint64_t kdmapper::MapDriver(HANDLE iqvw64e_device_handle, BYTE* data, ULONG64 param1, ULONG64 param2, bool free, bool destroyHeader, bool mdlMode, bool PassAllocationAddressAsFirstParam, mapCallback callback, NTSTATUS* exitCode) 
{
	std::uint8_t stub[ ] = { 0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x08, 0x49, 0x89, 0x6B, 0x10, 0x49, 0x89, 0x73, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x60, 0x48, 0x8B, 0x05, 0xBD, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xEA, 0x48, 0x8B, 0xF1, 0x49, 0x8D, 0x53, 0xE0, 0x49, 0x8B, 0xC9, 0x41, 0x8B, 0xF8, 0x33, 0xDB, 0xFF, 0xD0, 0x85, 0xC0, 0x79, 0x07, 0x33, 0xC0, 0xE9, 0x87, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x05, 0x97, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x54, 0x24, 0x40, 0x48, 0x8B, 0x8C, 0x24, 0x90, 0x00, 0x00, 0x00, 0xFF, 0xD0, 0x85, 0xC0, 0x78, 0x5F, 0x8B, 0xCF, 0x85, 0xFF, 0x74, 0x39, 0x83, 0xE9, 0x01, 0x74, 0x34, 0x83, 0xF9, 0x01, 0x75, 0x44, 0x4C, 0x8B, 0x44, 0x24, 0x40, 0x48, 0x8D, 0x44, 0x24, 0x50, 0x48, 0x8B, 0x16, 0x4C, 0x8B, 0xCD, 0x48, 0x89, 0x44, 0x24, 0x30, 0x48, 0x8B, 0x46, 0x08, 0x88, 0x4C, 0x24, 0x28, 0x48, 0x8B, 0x4C, 0x24, 0x48, 0x48, 0x89, 0x44, 0x24, 0x20, 0xFF, 0x15, 0x4F, 0x00, 0x00, 0x00, 0xEB, 0x15, 0x48, 0x8B, 0x44, 0x24, 0x40, 0xF7, 0xDF, 0x48, 0x1B, 0xC9, 0x83, 0xE1, 0x30, 0x48, 0x8B, 0x9C, 0x01, 0x20, 0x05, 0x00, 0x00, 0x48, 0x8B, 0x4C, 0x24, 0x40, 0xFF, 0x15, 0x35, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x4C, 0x24, 0x48, 0xFF, 0x15, 0x2A, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xC3, 0x4C, 0x8D, 0x5C, 0x24, 0x60, 0x49, 0x8B, 0x5B, 0x10, 0x49, 0x8B, 0x6B, 0x18, 0x49, 0x8B, 0x73, 0x20, 0x49, 0x8B, 0xE3, 0x5F, 0xC3, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	const auto NtMapVisualRelativePoints = intel_driver::GetKernelModuleExport( iqvw64e_device_handle, utils::GetKernelModuleAddress( "win32kbase.sys" ), "NtMapVisualRelativePoints" );

	*reinterpret_cast< std::uintptr_t* >( stub + ( 0x308 - 0x230 ) ) = intel_driver::GetKernelModuleExport( iqvw64e_device_handle, intel_driver::ntoskrnlAddr, "PsLookupProcessByProcessId" );
	*reinterpret_cast< std::uintptr_t* >( stub + ( 0x310 - 0x230 ) ) = intel_driver::GetKernelModuleExport( iqvw64e_device_handle, intel_driver::ntoskrnlAddr, "MmCopyVirtualMemory" );
	*reinterpret_cast< std::uintptr_t* >( stub + ( 0x318 - 0x230 ) ) = intel_driver::GetKernelModuleExport( iqvw64e_device_handle, intel_driver::ntoskrnlAddr, "ObfDereferenceObject" );

	intel_driver::WriteToReadOnlyMemory( iqvw64e_device_handle, NtMapVisualRelativePoints, stub, sizeof( stub ) );

	return 0;
}

void kdmapper::RelocateImageByDelta(portable_executable::vec_relocs relocs, const uint64_t delta) {
	for (const auto& current_reloc : relocs) {
		for (auto i = 0u; i < current_reloc.count; ++i) {
			const uint16_t type = current_reloc.item[i] >> 12;
			const uint16_t offset = current_reloc.item[i] & 0xFFF;

			if (type == IMAGE_REL_BASED_DIR64)
				*reinterpret_cast<uint64_t*>(current_reloc.address + offset) += delta;
		}
	}
}

bool kdmapper::ResolveImports(HANDLE iqvw64e_device_handle, portable_executable::vec_imports imports) {
	for (const auto& current_import : imports) {
		ULONG64 Module = utils::GetKernelModuleAddress(current_import.module_name);
		if (!Module) {
#if !defined(DISABLE_OUTPUT)
			std::cout << "[-] Dependency " << current_import.module_name << " wasn't found" << std::endl;
#endif
			return false;
		}

		for (auto& current_function_data : current_import.function_datas) {
			uint64_t function_address = intel_driver::GetKernelModuleExport(iqvw64e_device_handle, Module, current_function_data.name);

			if (!function_address) {
				//Lets try with ntoskrnl
				if (Module != intel_driver::ntoskrnlAddr) {
					function_address = intel_driver::GetKernelModuleExport(iqvw64e_device_handle, intel_driver::ntoskrnlAddr, current_function_data.name);
					if (!function_address) {
#if !defined(DISABLE_OUTPUT)
						std::cout << "[-] Failed to resolve import " << current_function_data.name << " (" << current_import.module_name << ")" << std::endl;
#endif
						return false;
					}
				}
			}

			*current_function_data.address = function_address;
		}
	}

	return true;
}
